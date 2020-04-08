#include "config.h"
#include "debug.h"

// Third party dependencies
#include <SoftwareSerial.h>
#include <FastCRC.h>
#include <sml/sml_file.h>

#include <IotWebConf.h>
#include "publishers/mqtt_publisher.h"
#include "EEPROM.h"
#include <ESP8266WiFi.h>

// SML constants
const byte START_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01};
const byte END_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x1A};
const size_t BUFFER_SIZE = 3840; // Max datagram duration 400ms at 9600 Baud
const uint8_t READ_TIMEOUT = 30;

// States
void wait_for_start_sequence();
void read_message();
void read_checksum();
void process_message();

// Serial sensor device
SoftwareSerial sensor;

void wifiConnected();
void configSaved();

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;
WiFiClient net;

MqttConfig mqttConfig;
MqttPublisher publisher;

IotWebConf iotWebConf("SMLReader", &dnsServer, &server, "", CONFIG_VERSION);
IotWebConfParameter params[] = {
	IotWebConfParameter("MQTT server", "mqttServer", mqttConfig.server, sizeof(mqttConfig.server), "text", NULL, mqttConfig.server, NULL, true),
	IotWebConfParameter("MQTT port", "mqttPort", mqttConfig.port, sizeof(mqttConfig.port), "text", NULL, mqttConfig.port, NULL, true),
	IotWebConfParameter("MQTT username", "mqttUsername", mqttConfig.username, sizeof(mqttConfig.username), "text", NULL, mqttConfig.username, NULL, true),
	IotWebConfParameter("MQTT password", "mqttPassword", mqttConfig.password, sizeof(mqttConfig.password), "password", NULL, mqttConfig.password, NULL, true),
	IotWebConfParameter("MQTT topic", "mqttTopic", mqttConfig.topic, sizeof(mqttConfig.topic), "text", NULL, mqttConfig.topic, NULL, true)};

boolean needReset = false;
boolean connected = false;

// Helpers
FastCRC16 CRC16;
byte buffer[BUFFER_SIZE];
size_t position = 0;
unsigned long last_state_reset = 0;
uint8_t bytes_until_checksum = 0;
uint8_t loop_counter = 0;
void (*state)() = NULL;

// Wrappers for sensor access
int data_available()
{
	return sensor.available();
}
int data_read()
{
	return sensor.read();
}

// Set state
void set_state(void (*new_state)())
{
	if (new_state == wait_for_start_sequence)
	{
		DEBUG("State is 'wait_for_start_sequence'.");
		last_state_reset = millis();
		position = 0;
	}
	else if (new_state == read_message)
	{
		DEBUG("State is 'read_message'.");
	}
	else if (new_state == read_checksum)
	{
		DEBUG("State is 'read_checksum'.");
		bytes_until_checksum = 3;
	}
	else if (new_state == process_message)
	{
		DEBUG("State is 'process_message'.");
	};
	state = new_state;
}

// Initialize state machine
void init_state()
{
	set_state(wait_for_start_sequence);
}

// Start over and wait for the start sequence
void reset_state(const char *message = NULL)
{
	if (message != NULL && strlen(message) > 0)
	{
		DEBUG(message);
	}
	init_state();
}

// Wait for the start_sequence to appear
void wait_for_start_sequence()
{
	while (data_available())
	{
		buffer[position] = data_read();
		yield();

		position = (buffer[position] == START_SEQUENCE[position]) ? (position + 1) : 0;
		if (position == sizeof(START_SEQUENCE))
		{
			// Start sequence has been found
			DEBUG("Start sequence found.");
			set_state(read_message);
			return;
		}
	}
}

// Read the rest of the message
void read_message()
{
	while (data_available())
	{
		// Check whether the buffer is still big enough to hold the number of fill bytes (1 byte) and the checksum (2 bytes)
		if ((position + 3) == BUFFER_SIZE)
		{
			reset_state("Buffer will overflow, starting over.");
			return;
		}
		buffer[position++] = data_read();
		yield();

		// Check for end sequence
		int last_index_of_end_seq = sizeof(END_SEQUENCE) - 1;
		for (int i = 0; i <= last_index_of_end_seq; i++)
		{
			if (END_SEQUENCE[last_index_of_end_seq - i] != buffer[position - (i + 1)])
			{
				break;
			}
			if (i == last_index_of_end_seq)
			{
				DEBUG("End sequence found.");
				set_state(read_checksum);
				return;
			}
		}
	}
}

// Read the number of fillbytes and the checksum
void read_checksum()
{
	while (bytes_until_checksum > 0 && data_available())
	{
		buffer[position++] = data_read();
		bytes_until_checksum--;
		yield();
	}

	if (bytes_until_checksum == 0)
	{
		DEBUG("Message has been read.");
		DEBUG_DUMP_BUFFER(buffer, position);
		set_state(process_message);
	}
}

void process_message()
{
	// Verify by checksum
	uint16_t calculated_checksum = CRC16.x25(buffer, (position - 2));
	// Swap the bytes
	uint16_t given_checksum = (buffer[position - 1] << 8) | buffer[position - 2];

	if (calculated_checksum != given_checksum)
	{
		reset_state("Checksum mismatch, starting over.");
		return;
	}


	// Parse
	sml_file *file = sml_file_parse(buffer + 8, position - 16);

	//DEBUG_SML_FILE(file);

	publisher.publish(file);

	// free the malloc'd memory
	sml_file_free(file);
		
	// Start over
	reset_state();
}

void run_current_state()
{
	if (state != NULL)
	{

		if ((millis() - last_state_reset) > (READ_TIMEOUT * 1000))
		{
			DEBUG("Did not receive an SML message within %d seconds, starting over.", READ_TIMEOUT);
			reset_state();
		}
		state();
	}
}

void setup()
{
	// Setup debugging stuff
	SERIAL_DEBUG_SETUP(115200);

	// Setup reading head
	sensor.begin(9600, SWSERIAL_8N1, SENSOR_PIN, -1, false, BUFFER_SIZE, BUFFER_SIZE);
	sensor.enableTx(false);
	sensor.enableRx(true);
	sensor.enableIntTx(false);

	// Initialize publisher
	// Setup WiFi and config stuff
	DEBUG("Setting up WiFi and config stuff.");
	DEBUG("Setting status pin to %d.", STATUS_PIN);
	iotWebConf.setStatusPin(STATUS_PIN);
	for (uint8_t i = 0; i < sizeof(params) / sizeof(params[0]); i++)
	{
		DEBUG("Adding parameter %s.", params[i].label);
		iotWebConf.addParameter(&params[i]);
	}
	iotWebConf.setConfigSavedCallback(&configSaved);
	iotWebConf.setWifiConnectionCallback(&wifiConnected);
	iotWebConf.setupUpdateServer(&httpUpdater);

	boolean validConfig = iotWebConf.init();
	if (!validConfig)
	{
		DEBUG("Missing or invalid config. MQTT publisher disabled.");
		MqttConfig defaults;
		// Resetting to default values
		strcpy(mqttConfig.server, defaults.server);
		strcpy(mqttConfig.port, defaults.port);
		strcpy(mqttConfig.username, defaults.username);
		strcpy(mqttConfig.password, defaults.password);
		strcpy(mqttConfig.topic, defaults.topic);
	}
	else
	{
		// Setup MQTT publisher
		publisher.setup(mqttConfig);
	}

	server.on("/", [] { iotWebConf.handleConfig(); });
	server.onNotFound([]() { iotWebConf.handleNotFound(); });

	DEBUG("Setup done.");
}

void loop()
{
	// Publisher
	publisher.loop();
	yield();

	if (needReset)
	{
		// Doing a chip reset caused by config changes
		DEBUG("Rebooting after 1 second.");
		delay(1000);
		ESP.restart();
	}
	if (connected)
	{
		// SMLReader state machine
		run_current_state();
		yield();
	}
	iotWebConf.doLoop();
	yield();
}

void configSaved()
{
	DEBUG("Configuration was updated.");
	needReset = true;
}

void wifiConnected()
{
	DEBUG("WiFi connection established.");
	connected = true;
	publisher.connect();
	init_state();
}
