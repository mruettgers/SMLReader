#include <Arduino.h>

#include <SoftwareSerial.h>

#include <FastCRC.h>

const bool DEBUG = true;
const int SENSOR_PIN = 4;

const byte START_SEQUENCE[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 };
const byte END_SEQUENCE[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A };
const int BUFFER_SIZE = 1024;

byte buffer[BUFFER_SIZE];
int position = 0;
SoftwareSerial sensor(SENSOR_PIN, -1);
FastCRC16 CRC16;

// States
void wait_for_start_sequence();
void read_message();
void read_num_of_fillbytes_and_checksum();
void process_message();
bool data_available();
int data_read();


void (*state)(void) = NULL;


// Print to serial console
void log(const char *message) {
	if (message != NULL && strlen(message) > 0) {
		Serial.println(message);
	}
}


void debug(const char *message) {
	if (!DEBUG) {
		return;
	}
	log(message);
}

void dump_buffer(){
	Serial.println("----DATA----");
	for (int i = 0; i < position; i++) {
		Serial.print("0x");
		Serial.print(buffer[i], HEX);
		Serial.print(" ");
	} 
	Serial.println();
	Serial.println("---END_OF_DATA---");
} 

bool data_available() {
	return sensor.available();
}

int data_read() {
	return sensor.read();
}


// Start (over) and wait for the start sequence
void reset(const char *message = NULL) {
	position = 0;
	state = wait_for_start_sequence;
	log(message);
}


// Wait for the start_sequence to appear
void wait_for_start_sequence() {
	while(data_available()) {
		buffer[position] = data_read();
		position = (buffer[position] == START_SEQUENCE[position]) ? (position + 1) : 0;

		if (position == sizeof(START_SEQUENCE)) {
			// Start sequence has been found
			debug("Start sequence has been found, handing over to 'read_message'.");
			state = read_message;
			return;
		}
	}
}


// Read the rest of the message
void read_message() {
	while(data_available()) {
		// Check whether the buffer still big enough to hold the number of fill bytes (2 bytes) and the checksum (4 bytes)
		if ((position + 6) == BUFFER_SIZE) {
			reset("Buffer will overflow, starting over.");
			return;
		}
		buffer[position++] = data_read();

		// Check for end sequence
		int last_index_of_end_seq = sizeof(END_SEQUENCE) -1;
		for (int i = 0; i <= last_index_of_end_seq; i++){
			if (END_SEQUENCE[last_index_of_end_seq-i] != buffer[position-(i+1)]){
				break;
			}
			if (i == last_index_of_end_seq) {
				debug("End sequence has been found, handing over to 'read_num_of_fillbytes_and_checksum'.");
				state = read_num_of_fillbytes_and_checksum;
				return;
			}
		} 

	}

}

// Read the number of fillbytes and the checksum
void read_num_of_fillbytes_and_checksum() {
	// Wait for the next 3 bytes to appear on the line
	int counter = 0;
	while (counter < 3) {
		while(data_available()) {
			buffer[position++] = data_read();
			counter++;
		}
	}
	debug("Last bytes have been read, handing over to 'process_message'.");
	state = process_message;
}

void process_message() {
	// Verify by checksum
	int calculated_checksum = CRC16.x25(buffer, (position-2));
	// Swap the bytes
	int given_checksum = (buffer[position-1]<<8) | buffer[position-2];

	if (calculated_checksum != given_checksum) {
		reset("Checksum mismatch, starting over.");
		return;
	} 

	// Parse

	// Publish

	dump_buffer();

	// Start over
	reset();
}

void run_current_state() {
	if (state != NULL){
	    state();
    }
}

void setup() {
	Serial.begin(115200);
	log("Setting up application...");

	sensor.begin(9600);
	log("Sensor has been initialized.");
	
	log("Application setup finished.");
	reset();
}

void loop() {
	run_current_state();
}