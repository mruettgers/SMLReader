#include "config.h"
#include "debug.h"

// Third party dependencies
#include <sml/sml_file.h>

#include <IotWebConf.h>
#include "publishers/mqtt_publisher.h"
#include "EEPROM.h"
#include <ESP8266WiFi.h>

sensor_state sensors[NUM_OF_SENSORS];
sensor_state *sensor;

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

// Wrappers for sensor access
int data_available()
{
	return sensor->serial.available();
}
int data_read()
{
	return sensor->serial.read();
}

// Set state
void set_state(void (*new_state)())
{
	DEBUG("Sensor: %s", sensor->config->name);
	if (new_state == wait_for_start_sequence)
	{
		DEBUG("State is 'wait_for_start_sequence'.");
		sensor->last_state_reset = millis();
		sensor->position = 0;
	}
	else if (new_state == read_message)
	{
		DEBUG("State is 'read_message'.");
	}
	else if (new_state == read_checksum)
	{
		DEBUG("State is 'read_checksum'.");
		sensor->bytes_until_checksum = 3;
	}
	else if (new_state == process_message)
	{
		DEBUG("State is 'process_message'.");
	};
	sensor->state = new_state;
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
		sensor->buffer[sensor->position] = data_read();
		yield();

		sensor->position = (sensor->buffer[sensor->position] == START_SEQUENCE[sensor->position]) ? (sensor->position + 1) : 0;
		if (sensor->position == sizeof(START_SEQUENCE))
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
		if ((sensor->position + 3) == BUFFER_SIZE)
		{
			reset_state("Buffer will overflow, starting over.");
			return;
		}
		sensor->buffer[sensor->position++] = data_read();
		yield();

		// Check for end sequence
		int last_index_of_end_seq = sizeof(END_SEQUENCE) - 1;
		for (int i = 0; i <= last_index_of_end_seq; i++)
		{
			if (END_SEQUENCE[last_index_of_end_seq - i] != sensor->buffer[sensor->position - (i + 1)])
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
	while (sensor->bytes_until_checksum > 0 && data_available())
	{
		sensor->buffer[sensor->position++] = data_read();
		sensor->bytes_until_checksum--;
		yield();
	}

	if (sensor->bytes_until_checksum == 0)
	{
		DEBUG("Message has been read.");
		DEBUG_DUMP_BUFFER(sensor->buffer, sensor->position);
		set_state(process_message);
	}
}

void process_message()
{
	// Parse
	sml_file *file = sml_file_parse(sensor->buffer + 8, sensor->position - 16);

	DEBUG_SML_FILE(file);

	publisher.publish(sensor, file);

	// free the malloc'd memory
	sml_file_free(file);
		
	// Start over
	reset_state();
}

void run_current_state()
{
	if (sensor->state != NULL)
	{

		if ((millis() - sensor->last_state_reset) > (READ_TIMEOUT * 1000))
		{
			DEBUG("Did not receive an SML message within %d seconds, starting over.", READ_TIMEOUT);
			reset_state();
		}
		sensor->state();
	}
}

void setup()
{
	// Setup debugging stuff
	SERIAL_DEBUG_SETUP(115200);

	// Setup reading heads
	DEBUG("%d sensors configured.", NUM_OF_SENSORS);
	for (uint8_t i = 0; i < NUM_OF_SENSORS; i++) {
		DEBUG("Initializing sensor %s...", SENSOR_CONFIGS[i].name);
		sensors[i].config = &(SENSOR_CONFIGS[i]);
		
		sensors[i].serial.begin(9600, SWSERIAL_8N1, sensors[i].config->pin, -1, false, BUFFER_SIZE, BUFFER_SIZE);
		sensors[i].serial.enableTx(false);
		sensors[i].serial.enableRx(true);
		sensors[i].serial.enableIntTx(false);

	}
	
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
		for (uint8_t i = 0; i < NUM_OF_SENSORS; i++) {
			sensor = &sensors[i];
			run_current_state();
			yield();
		}
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

	// Initialize state machines
	for (uint8_t i = 0; i < NUM_OF_SENSORS; i++) {
		DEBUG("Initializing state of sensor %s...", SENSOR_CONFIGS[i].name);
		sensor = &sensors[i];
		init_state();
	}
}
