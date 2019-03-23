#include <Arduino.h>

#include <SoftwareSerial.h>
#include <FastCRC.h>
#include <string.h>
#include "OneWireHub.h"
#include "BAE910.h"

const bool DEBUG = false;
const uint8_t SENSOR_PIN = 4;
const uint8_t ONEWIRE_PIN = 0;

const byte START_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01};
const byte END_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x1A};
const int BUFFER_SIZE = 3840; // Max datagram duration 400ms at 9600 Baud

struct metric_value
{
	int64_t value;
	uint8_t unit;
	int8_t scaler;
};

struct metric
{
	const size_t pattern_length;
	const byte *pattern;
	const char *name;
};

const metric METRICS[] = {
	{8,
	 (byte[]){0x77, 0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF},
	 "power_in"},
	{8,
	 (byte[]){0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x00, 0xFF},
	 "power_out"},
	{8,
	 (byte[]){0x77, 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF},
	 "power_current"}};

byte buffer[BUFFER_SIZE];
const size_t METRIC_LENGTH = sizeof(METRICS) / sizeof(metric);
const size_t START_SEQUENCE_LENGTH = sizeof(START_SEQUENCE) / sizeof(byte);
const size_t END_SEQUENCE_LENGTH = sizeof(END_SEQUENCE) / sizeof(byte);

SoftwareSerial sensor(SENSOR_PIN, -1);
FastCRC16 CRC16;

// States
void wait_for_start_sequence();
void read_message();
void read_checksum();
void process_message();

// Helpers
int position = 0;
uint8_t bytes_until_checksum = 0;
void (*state)(void) = wait_for_start_sequence;

// OneWire
auto owHub = OneWireHub(ONEWIRE_PIN);
auto owBae910 = BAE910(BAE910::family_code, 0x00, 0x00, 0x00, 0x00, 0xAC, 0xDC);

// Wrappers for sensor access
bool data_available()
{
	return sensor.available();
}
int data_read()
{
	return sensor.read();
}

// Debug stuff
void dump_buffer()
{
	Serial.println("----DATA----");
	for (int i = 0; i < position; i++)
	{
		Serial.print("0x");
		Serial.print(buffer[i], HEX);
		Serial.print(" ");
	}
	Serial.println();
	Serial.println("---END_OF_DATA---");
}

// Start (over) and wait for the start sequence
void reset(const char *message = NULL)
{
	position = 0;
	state = wait_for_start_sequence;
	if (message != NULL && strlen(message) > 0)
	{
		Serial.println(message);
	}
}

// Wait for the start_sequence to appear
void wait_for_start_sequence()
{
	while (data_available())
	{
		buffer[position] = data_read();
		position = (buffer[position] == START_SEQUENCE[position]) ? (position + 1) : 0;

		if (position == START_SEQUENCE_LENGTH)
		{
			// Start sequence has been found
			if (DEBUG)
				Serial.println("Start sequence found.");

			state = read_message;
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

		// Check for end sequence
		int last_index_of_end_seq = END_SEQUENCE_LENGTH - 1;
		for (int i = 0; i <= last_index_of_end_seq; i++)
		{
			if (END_SEQUENCE[last_index_of_end_seq - i] != buffer[position - (i + 1)])
			{
				break;
			}
			if (i == last_index_of_end_seq)
			{
				if (DEBUG)
					Serial.println("End sequence found.");

				bytes_until_checksum = 3;
				state = read_checksum;
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
	}

	if (bytes_until_checksum == 0)
	{
		if (DEBUG)
		{
			Serial.println("Message has been read.");
			dump_buffer();
		}
		state = process_message;
	}
}

void process_message()
{
	// Verify by checksum
	int calculated_checksum = CRC16.x25(buffer, (position - 2));
	// Swap the bytes
	int given_checksum = (buffer[position - 1] << 8) | buffer[position - 2];

	if (calculated_checksum != given_checksum)
	{
		reset("Checksum mismatch, starting over.");
		return;
	}

	// Parse
	void *found_at;
	metric_value values[METRIC_LENGTH];
	byte *cp;
	byte len, type;
	uint64_t uvalue;
	for (uint8_t i = 0; i < METRIC_LENGTH; i++)
	{
		size_t pattern_size = METRICS[i].pattern_length * sizeof(byte);
		found_at = memmem_P(buffer, position * sizeof(byte), METRICS[i].pattern, pattern_size);

		if (found_at != NULL)
		{
			Serial.print("Found metric ");
			Serial.print(METRICS[i].name);
			Serial.println(".");
			cp = (byte *)(found_at) + METRICS[i].pattern_length;

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
	for (uint8_t i = 0; i < METRIC_LENGTH; i++)
	{
		// We have 32 bit available
		value = (values[i].value * (pow(10, values[i].scaler))) * 1000;
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
			Serial.print("Error: Num of metrics exceeds the num of available 32 bit slots of the BAE910.");
			Serial.print(" Ignoring metric ");
			Serial.print(METRICS[i].name);
			Serial.println(".");
			continue;
		}
		Serial.print("Published metric ");
		Serial.print(METRICS[i].name);
		Serial.print(" with value ");
		Serial.print((long)values[i].value);
		Serial.print(", unit ");
		Serial.print((int)values[i].unit);
		Serial.print(" and scaler ");
		Serial.print((int)values[i].scaler);
		Serial.println(".");
	}

	// Start over
	reset();
}

void run_current_state()
{
	if (state != NULL)
	{
		state();
	}
}

void setup()
{
	Serial.begin(115200);
	sensor.begin(9600);

	// OneWire
	owHub.attach(owBae910);
	owBae910.memory.field.SW_VER = 0x01;
	owBae910.memory.field.BOOTSTRAP_VER = 0x01;
}

void loop()
{
	// Run application's current state
	run_current_state();

	// OneWire
	owHub.poll();
	if (owHub.hasError())
		owHub.printError();
}