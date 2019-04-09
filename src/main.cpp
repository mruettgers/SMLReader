#include "config.h"
#include "debug.h"

// Third party dependencies
#include <SoftwareSerial.h>
#include <FastCRC.h>
#include "OneWireHub.h"
#include "BAE910.h"

// Constants
const byte START_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01};
const byte END_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x1A};
const uint8_t NUM_OF_METRICS = sizeof(METRICS) / sizeof(metric);
const size_t BUFFER_SIZE = 3840; // Max datagram duration 400ms at 9600 Baud
const uint8_t READ_TIMEOUT = 30;

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
OneWireHub owHub = OneWireHub(ONEWIRE_PIN);
std::list<BAE910 *> owSlaveDevices;

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

// Start over and wait for the start sequence
void reset(const char *message = NULL)
{
	if (message != NULL && strlen(message) > 0)
	{
		DEBUG(message);
	}
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

	// Publish
	int32_t value;
	std::list<BAE910 *>::iterator owSlaveDevice = owSlaveDevices.begin();
	uint8_t slot = 0;
	for (uint8_t i = 0; i < NUM_OF_METRICS; i++)
	{

		value = (uint32_t)((values[i].value * (pow(10, values[i].scaler))) * 1000);
		switch (slot++)
		{
		case 0:
			(*owSlaveDevice)->memory.field.userm = value;
			break;
		case 1:
			(*owSlaveDevice)->memory.field.usern = value;
			break;
		case 2:
			(*owSlaveDevice)->memory.field.usero = value;
			break;
		case 3:
			(*owSlaveDevice)->memory.field.userp = value;
			break;
		default:
			break;
		}

		DEBUG("Published metric '%s':", METRICS[i].name);
		DEBUG("  Value: %ld", (long)values[i].value);
		DEBUG("  Unit: %d", (int)values[i].unit);
		DEBUG("  Scaler: %d", (int)values[i].scaler);
		DEBUG("  Device: %02X%02X%02X%02X%02X%02X%02X%02X",
			  (*owSlaveDevice)->ID[0], (*owSlaveDevice)->ID[1],
			  (*owSlaveDevice)->ID[2], (*owSlaveDevice)->ID[3],
			  (*owSlaveDevice)->ID[4], (*owSlaveDevice)->ID[5],
			  (*owSlaveDevice)->ID[6], (*owSlaveDevice)->ID[7]);
		DEBUG("  Slot: %d", (int)slot);

		if ((i + 1) % 4 == 0)
		{
			// Available slots reached, use next device
			owSlaveDevice++;
			slot = 0;
		}
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
			DEBUG("Did not receive a message within %d seconds, starting over.", READ_TIMEOUT);
			reset();
		}
		state();
	}
}

void setup()
{
	// Setup debugging stuff
	SERIAL_DEBUG_SETUP(115200);

	// Setup reading head
	sensor.begin(9600);
	sensor.enableTx(false);
	sensor.enableRx(true);
	sensor.enableIntTx(false);

	// Setup OneWire
	// Because we have only 4 32 bit registers we will add multiple slave devices to the hub if required
	BAE910 *owDevice;
	for (int i = 0; NUM_OF_METRICS > 0 && i <= ((NUM_OF_METRICS - 1) / 4); i++)
	{
		owDevice = new BAE910(BAE910::family_code, 'S', 'M', i, chip_id.fields[0], chip_id.fields[1], chip_id.fields[2]);
		owSlaveDevices.push_back(owDevice);
		owDevice->memory.field.SW_VER = 0x01;
		owDevice->memory.field.BOOTSTRAP_VER = 0x01;
		owHub.attach(*owDevice);
	}

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