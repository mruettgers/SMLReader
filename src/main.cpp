#include "config.h"
#include "debug.h"

// Third party dependencies
#include <SoftwareSerial.h>
#include <FastCRC.h>

#ifdef MODE_ONEWIRE
#include "publishers/onewire_publisher.h"
#else
#include <IotWebConf.h>
#include "publishers/mqtt_publisher.h"
#include "EEPROM.h"
#endif

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

// Serial device
SoftwareSerial sensor(SENSOR_PIN, -1, false, BUFFER_SIZE, BUFFER_SIZE);

#ifdef MODE_ONEWIRE
OneWirePublisher publisher;
#else
void wifiConnected();
void configSaved();

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;
WiFiClient net;

MqttConfig mqttConfig;
MqttPublisher publisher;

/**   const char* label, const char* id, char* valueBuffer, int length,
    const char* type = "text", const char* placeholder = NULL,
    const char* defaultValue = NULL, const char* customHtml = NULL,
    boolean visible = true);
	**/
IotWebConf iotWebConf("SMLReader", &dnsServer, &server, "", VERSION);
IotWebConfParameter params[] = {
	IotWebConfParameter("MQTT server", "mqttServer", mqttConfig.server, sizeof(mqttConfig.server), "text", NULL, mqttConfig.server, NULL, true),
	IotWebConfParameter("MQTT port", "mqttPort", mqttConfig.port, sizeof(mqttConfig.port), "text", NULL, mqttConfig.port, NULL, true),
	IotWebConfParameter("MQTT username", "mqttUsername", mqttConfig.username, sizeof(mqttConfig.username), "text", NULL, mqttConfig.mqttUsername, NULL, true),
	IotWebConfParameter("MQTT password", "mqttPassword", mqttConfig.password, sizeof(mqttConfig.password), "password", NULL, mqttConfig.password, NULL, true),
	IotWebConfParameter("MQTT topic", "mqttTopic", mqttConfig.topic, sizeof(mqttConfig.topic)), "text", NULL, mqttConfig.topic, NULL, true};

boolean needReset = false;
boolean connected = false;

#endif

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
void init()
{
	set_state(wait_for_start_sequence);
}

// Start over and wait for the start sequence
void reset(const char *message = NULL)
{
	if (message != NULL && strlen(message) > 0)
	{
		DEBUG(message);
	}
	init();
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
			reset("Buffer will overflow, starting over.");
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
		reset("Checksum mismatch, starting over.");
		return;
	}

	// Parse
	void *found_at;
	metric_value values[NUM_OF_METRICS];
	byte *cp;
	byte len, type;
	uint64_t uvalue;
	for (uint8_t i = 0; i < NUM_OF_METRICS; i++)
	{
		size_t pattern_size = METRICS[i].pattern.size();
		found_at = memmem_P(buffer, position, &(METRICS[i].pattern.front()), pattern_size);

		if (found_at != NULL)
		{
			DEBUG("Found metric '%s.'", METRICS[i].name);
			cp = (byte *)(found_at) + pattern_size;

			// Ingore status byte
			cp += (*cp & 0x0f);

			// Ignore time byte
			cp += (*cp & 0x0f);

			// Save unit
			len = *cp & 0x0f;
			values[i].unit = *(cp + 1);
			cp += len;

			// Save scaler
			len = *cp & 0x0f;
			values[i].scaler = *(cp + 1);
			cp += len;

			// Save value
			type = *cp & 0x70;
			len = *cp & 0x0f;
			cp++;

			uvalue = 0;
			uint8_t nlen = len;
			while (--nlen)
			{
				uvalue <<= 8;
				uvalue |= *cp++;
			}

			values[i].value = (type == 0x50 ? (int64_t)uvalue : uvalue);
		}
	}

	publisher.publish(values);

	// Start over
	reset();
}

void run_current_state()
{
	if (state != NULL)
	{

		if ((millis() - last_state_reset) > (READ_TIMEOUT * 1000))
		{
			DEBUG("Did not receive an SML message within %d seconds, starting over.", READ_TIMEOUT);
			reset();
		}
		state();
	}
}

void setup()
{
	// Setup debugging stuff
	SERIAL_DEBUG_SETUP(9600);

	// Setup reading head
	sensor.begin(9600);
	sensor.enableTx(false);
	sensor.enableRx(true);
	sensor.enableIntTx(false);

	// Initialize publisher
#ifdef MODE_ONEWIRE
	// Setup 1wire publisher
	publisher.setup(ONEWIRE_PIN);
	init();
#else
	// Setup WiFi and config stuff
	DEBUG("Setting up WiFi and config stuff.");
	delay(2000);
	iotWebConf.setStatusPin(STATUS_PIN);
	for (uint8_t i = 0; i < sizeof(params) / sizeof(params[0]); i++)
	{
		DEBUG("Added parameter %s.", params[i].label);
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
		strcpy(mqttConfig.topic, defaults.topics);
	}
	else
	{
		// Setup MQTT publisher
		publisher.setup(mqttConfig);
	}

	server.on("/", [] { iotWebConf.handleConfig(); });
	server.onNotFound([]() { iotWebConf.handleNotFound(); });

#endif
	DEBUG("Setup done.");
}

void loop()
{
	// Publisher
	publisher.loop();

#ifdef MODE_ONEWIRE
	// SMLReader state machine
	run_current_state();
#else
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
	}
	iotWebConf.doLoop();
#endif
}

#ifndef MODE_ONEWIRE
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
	init();
}

#endif