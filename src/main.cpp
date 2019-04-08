#include <Arduino.h>

#include <SoftwareSerial.h>
#include <FastCRC.h>
#include <string.h>
#include <vector>
#include <list>
#include "OneWireHub.h"
#include "BAE910.h"

#ifdef ENABLE_SERIAL_DEBUG
const bool VERBOSE = true;
#endif
const uint8_t SENSOR_PIN = 4;
const uint8_t ONEWIRE_PIN = 0;

struct metric
{
	const char *name;
	const std::vector<byte> pattern;
};

const metric METRICS[] = {
	{"power_in", {0x77, 0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF}},
	{"power_out", {0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x00, 0xFF}},
	{"power_current", {0x77, 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF}}};

const byte START_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01};
const byte END_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x1A};
const uint8_t NUM_OF_METRICS = sizeof(METRICS) / sizeof(metric);
const size_t BUFFER_SIZE = 3840; // Max datagram duration 400ms at 9600 Baud
const uint8_t READ_TIMEOUT = 30;

struct metric_value
{
	int64_t value;
	uint8_t unit;
	int8_t scaler;
};


// States
void wait_for_start_sequence();
void read_message();
void read_checksum();
void process_message();

// Serial device
SoftwareSerial sensor(SENSOR_PIN, -1, false, BUFFER_SIZE, BUFFER_SIZE);

// Helpers
FastCRC16 CRC16;
byte buffer[BUFFER_SIZE];
size_t position = 0;
unsigned long last_state_reset = 0;
uint8_t bytes_until_checksum = 0;
uint8_t loop_counter = 0;
void (*state)() = NULL;

// Get ESP's chip ID for use in the BAE910's unique address
union {
	uint32_t value;
	uint8_t fields[4];
} const chip_id = {ESP.getChipId()};

// OneWire
auto owHub = OneWireHub(ONEWIRE_PIN);
auto owBae910 = BAE910(BAE910::family_code, 'S', 'M', 'L', chip_id.fields[0], chip_id.fields[1], chip_id.fields[2]);

// Wrappers for sensor access
int data_available()
{
	return sensor.available();
}
int data_read()
{
	return sensor.read();
}

// Debug stuff
#ifdef ENABLE_SERIAL_DEBUG

void dump_buffer()
{
	Serial.println("----DATA----");
	for (size_t i = 0; i < position; i++)
	{
		Serial.print("0x");
		Serial.print(buffer[i], HEX);
		Serial.print(" ");
	}
	Serial.println();
	Serial.println("---END_OF_DATA---");
}
#endif

// Set state
void set_state(void (*new_state)())
{
	if (new_state == wait_for_start_sequence)
	{
#ifdef ENABLE_SERIAL_DEBUG
		Serial.println("State is 'wait_for_start_sequence'.");
#endif
		last_state_reset = millis();
		position = 0;
	}
	else if (new_state == read_message)
	{
#ifdef ENABLE_SERIAL_DEBUG
		Serial.println("State is 'read_message'.");
#endif
	}
	else if (new_state == read_checksum)
	{
#ifdef ENABLE_SERIAL_DEBUG
		Serial.println("State is 'read_checksum'.");
#endif
		bytes_until_checksum = 3;
	}
	else if (new_state == process_message)
	{
#ifdef ENABLE_SERIAL_DEBUG
		Serial.println("State is 'process_message'.");
#endif
	};
	state = new_state;
}

// Start over and wait for the start sequence
void reset(const char *message = NULL)
{
#ifdef ENABLE_SERIAL_DEBUG
	if (message != NULL && strlen(message) > 0)
	{
		Serial.println(message);
	}
#endif
	set_state(wait_for_start_sequence);
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
#ifdef ENABLE_SERIAL_DEBUG
			if (VERBOSE)
				Serial.println("Start sequence found.");
#endif
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
#ifdef ENABLE_SERIAL_DEBUG

				if (VERBOSE)
					Serial.println("End sequence found.");
#endif
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
#ifdef ENABLE_SERIAL_DEBUG

		if (VERBOSE)
		{
			Serial.println("Message has been read.");
			dump_buffer();
		}
#endif
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
#ifdef ENABLE_SERIAL_DEBUG

			Serial.print("Found metric ");
			Serial.print(METRICS[i].name);
			Serial.println(".");
#endif
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

	// Publish
	int32_t value;
	for (uint8_t i = 0; i < NUM_OF_METRICS; i++)
	{
		value = (uint32_t)((values[i].value * (pow(10, values[i].scaler))) * 1000);
		switch (i)
		{
		case 0:
			owBae910.memory.field.userm = value;
			break;
		case 1:
			owBae910.memory.field.usern = value;
			break;
		case 2:
			owBae910.memory.field.usero = value;
			break;
		case 3:
			owBae910.memory.field.userp = value;
			break;
		default:
#ifdef ENABLE_SERIAL_DEBUG

			Serial.print("Error: Num of metrics exceeds the num of available 32 bit slots of the BAE910.");
			Serial.print(" Ignoring metric ");
			Serial.print(METRICS[i].name);
			Serial.println(".");
#endif
			continue;
		}

#ifdef ENABLE_SERIAL_DEBUG

		Serial.print("Published metric ");
		Serial.print(METRICS[i].name);
		Serial.print(" with value ");
		Serial.print((long)values[i].value);
		Serial.print(", unit ");
		Serial.print((int)values[i].unit);
		Serial.print(" and scaler ");
		Serial.print((int)values[i].scaler);
		Serial.println(".");
#endif
	}

	// Start over
	reset();
}

void run_current_state()
{
	if (state != NULL)
	{

		if ((millis() - last_state_reset) > (READ_TIMEOUT * 1000))
		{
#ifdef ENABLE_SERIAL_DEBUG

			Serial.print("Did not receive a message within ");
			Serial.print(READ_TIMEOUT);
			Serial.println(" seconds, starting over.");
#endif
			reset();
		}
		//Serial.print("State runner: "); Serial.println(millis());

		state();
	}
}

void setup()
{
// Setup serial stuff
#ifdef ENABLE_SERIAL_DEBUG
	Serial.begin(115200);
#endif
	sensor.begin(9600);
	sensor.enableTx(false);
	sensor.enableRx(true);
	sensor.enableIntTx(false);

	// OneWire
	owHub.attach(owBae910);
	owBae910.memory.field.SW_VER = 0x01;
	owBae910.memory.field.BOOTSTRAP_VER = 0x01;

	// Set initial state
	set_state(wait_for_start_sequence);
}

void loop()
{
	// OneWire
	owHub.poll();

	// SMLReader
	run_current_state();
}