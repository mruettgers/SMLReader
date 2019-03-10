#include <Arduino.h>

#include <Homie.h>
#include <SoftwareSerial.h>
#include <FastCRC.h>

const bool DEBUG = true;
const int SENSOR_PIN = 4;

const byte START_SEQUENCE[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 };
const byte END_SEQUENCE[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A };
const int BUFFER_SIZE = 3840; // Max datagram duration 400ms at 9600 Baud

byte buffer[BUFFER_SIZE];
SoftwareSerial sensor(SENSOR_PIN, -1);
FastCRC16 CRC16;

// States
void wait_for_start_sequence();
void read_message();
void read_num_of_fillbytes_and_checksum();
void process_message();

int position = 0;
void (*state)(void) = wait_for_start_sequence;

bool data_available() {
	return sensor.available();
}

int data_read() {
	return sensor.read();
}

void log(const char *message) {
	Homie.getLogger() << message << endl;
}

void debug(const char *message) {
	if (DEBUG) {
		log(message);
	}
}

void dump_buffer(){
	HomieInternals::Logger logger = Homie.getLogger();
	logger << "----DATA----" << endl;
	for (int i = 0; i < position; i++) {
		logger.print("0x");
		logger.print(buffer[i], HEX);
		logger.print(" ");
	}
	logger << endl << "---END_OF_DATA---" << endl;
} 

// Start (over) and wait for the start sequence
void reset(const char *message = NULL) {
	position = 0;
	state = wait_for_start_sequence;
	if (message != NULL && strlen(message) > 0) {
		log(message);
	}
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
		// Check whether the buffer is still big enough to hold the number of fill bytes (1 byte) and the checksum (2 bytes)
		if ((position + 3) == BUFFER_SIZE) {
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



void setupHandler() {
    Homie.getLogger() << "Setting app application...";

	sensor.begin(9600);
	Homie.getLogger() << "Sensor has been initialized.";

	Homie.getLogger() << "Application setup finished.";
}

void loopHandler() {
	run_current_state();
}

void setup() {
	Serial.begin(115200);
	Serial << endl << endl;

	Homie_setFirmware("sml-reader", "1.0.0");
	Homie.setSetupFunction(setupHandler);
	Homie.setLoopFunction(loopHandler);
	Homie.setup();
}

void loop() {
	Homie.loop();	
}