#include "mbed.h"
#include "zest-radio-atzbrf233.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"

// Led
static DigitalOut led1(LED1);

// capteur humidite
static AnalogIn soil_moisture(ADC_IN1);
static float air_value = 0.000000 * 3.3; // valeurs negative si air value != 0
static float water_value = 0.748962 * 3.3;

// Capteur temperature
I2C i2c(I2C1_SDA, I2C1_SCL);
uint8_t lm75_adress = 0x48 << 1;

// Network interface
NetworkInterface *net;

/* Printf the message received and its configuration */
void messageArrived(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s \r\n\n", message.payloadlen, (char*)message.payload);

    if (strcmp((const char *)md.message.payload, "ON") >= 0) {
    	led1 = true;
    }
    else if (strcmp((const char *)md.message.payload, "OFF") >= 0) {
		led1 = false;
	}
}

void sendData(MQTT::Client<MQTTNetwork, Countdown>& client, char* topic_nom, float value)
{
	char topic[100];
	sprintf(topic, "paralixo/feeds/%s", topic_nom);
	printf("On envoie %f à %s : \r\n", value, topic);
	int rc = 0;

	MQTT::Message message;
	char buf[100];
	sprintf(buf, "%f", value);
	message.qos = MQTT::QOS0;
	message.retained = false;
	message.dup = false;
	message.payload = (void*)buf;
	message.payloadlen = strlen(buf)+1;

	rc = client.publish(topic, message);
}

float getHumidity()
{
	return ((soil_moisture.read() * 3.3) - air_value) * 100.0 / (water_value - air_value);
}

float getTemperature()
{
	char cmd[2];
	cmd[0] = 0x00; // adresse registre temperature
	i2c.write(lm75_adress, cmd, 1);
	i2c.read(lm75_adress, cmd, 2);
	return ((cmd[0] << 8 | cmd[1] ) >> 7) * 0.5;
}

// MQTT demo
int main() {
	int result;

    // Add the border router DNS to the DNS table
    nsapi_addr_t new_dns = {
        NSAPI_IPv6,
        { 0xfd, 0x9f, 0x59, 0x0a, 0xb1, 0x58, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x01 }
    };
    nsapi_dns_add_server(new_dns);

    printf("Starting MQTT connection...\n");

    // Get default Network interface (6LowPAN) & Connect 6LowPAN interface
    net = NetworkInterface::get_default_instance();
    if (!net) {
        printf("Error! No network inteface found.\n");
        return 0;
    }
    result = net->connect();
    if (result != 0) {
        printf("Error! net->connect() returned: %d\n", result);
        return result;
    }

    // Build the socket that will be used for MQTT & Declare a MQTT Client
    MQTTNetwork mqttNetwork(net);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    // Connect the socket to the MQTT Broker
    const char* hostname = "io.adafruit.com";
	uint16_t port = 1883;
    printf("Connecting to %s:%d\r\n", hostname, port);
    int rc = mqttNetwork.connect(hostname, port);
    if (rc != 0)
        printf("rc from TCP connect is %d\r\n", rc);

    // Connect the MQTT Client
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "mbed-sample";
    data.username.cstring = "paralixo";
    data.password.cstring = "3b3071da0b4545d7b66b80a72b9fd8fe";
    data.keepAliveInterval = 100;
    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);

    // Subs
    if ((rc = client.subscribe("paralixo/feeds/Led", MQTT::QOS2, messageArrived)) != 0)
    	printf("rc from MQTT subscribe is %d\r\n", rc);
    if ((rc = client.subscribe("paralixo/feeds/Humidité", MQTT::QOS2, messageArrived)) != 0)
        	printf("rc from MQTT subscribe is %d\r\n", rc);
    if ((rc = client.subscribe("paralixo/feeds/Temperature", MQTT::QOS2, messageArrived)) != 0)
        	printf("rc from MQTT subscribe is %d\r\n", rc);

    // main while
    while (true) {
		float humidity_percent = getHumidity();
		float temperature = getTemperature();

		sendData(client, "Humidité", humidity_percent);
		sendData(client, "Temperature", temperature);

		client.yield(100);
		wait(5);
    }

    // Disconnect client and socket & Bring down the 6LowPAN interface
    client.disconnect();
    mqttNetwork.disconnect();
    net->disconnect();
    printf("...MQTT connection ended\n");
}
