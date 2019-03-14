#include <Arduino.h>

#include <Homie.h>
#include <SoftwareSerial.h>
#include <FastCRC.h>
#include <string.h>

const bool DEBUG = false;
const int SENSOR_PIN = 4;

const byte START_SEQUENCE[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 };
const byte END_SEQUENCE[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A };
const int BUFFER_SIZE = 3840; // Max datagram duration 400ms at 9600 Baud

struct metric_value {
	int64_t value;
	uint8_t unit;
	int8_t scaler;
};

struct metric {
	const size_t pattern_length;
	const byte *pattern;
	const char *name;
	const HomieNode node;
};

const metric METRICS[] = {
	{
		8,
		(byte[]){ 0x77, 0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF },
		"power_in",
		HomieNode("power_in", "power")
	},
	{
		8,
		(byte[]){ 0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x00, 0xFF },
		"power_out",
		HomieNode("power_out", "power")
	},
	{
		8,
		(byte[]){ 0x77, 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF },
		"power_current",
		HomieNode("power_current", "power")
	}
};

byte buffer[BUFFER_SIZE];
const size_t METRIC_LENGTH = sizeof(METRICS) / sizeof(metric);
const size_t START_SEQUENCE_LENGTH = sizeof(START_SEQUENCE) / sizeof(byte);
const size_t END_SEQUENCE_LENGTH = sizeof(END_SEQUENCE) / sizeof(byte);

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

		if (position == START_SEQUENCE_LENGTH) {
			// Start sequence has been found
			debug("Start sequence found.");
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
		int last_index_of_end_seq = END_SEQUENCE_LENGTH -1;
		for (int i = 0; i <= last_index_of_end_seq; i++){
			if (END_SEQUENCE[last_index_of_end_seq-i] != buffer[position-(i+1)]){
				break;
			}
			if (i == last_index_of_end_seq) {
				debug("End sequence found.");
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
	debug("Message has been read.");
	if (DEBUG) {
		dump_buffer();
	} 
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
	void *found_at;
	metric_value values[METRIC_LENGTH];
	byte *cp;
	byte len,type;
	uint64_t uvalue;
	for (uint8_t i = 0; i < METRIC_LENGTH; i++) {
		size_t pattern_size = METRICS[i].pattern_length * sizeof(byte);
		found_at = memmem_P(buffer,position * sizeof(byte), METRICS[i].pattern, pattern_size);
		
		if (found_at != NULL) {
			Homie.getLogger() << "Found metric " << METRICS[i].name << "." << endl;
			cp = (byte*)(found_at) + METRICS[i].pattern_length;

			// Ingore status byte
			cp += (*cp & 0x0f);

			// Ignore time byte
			cp += (*cp & 0x0f);

			// Save unit
			len = *cp & 0x0f;
			values[i].unit = *(cp+1);
			cp += len;

			// Save scaler
			len = *cp & 0x0f;
			values[i].scaler = *(cp+1);
			cp += len;

			// Save value
		    type=*cp&0x70;
    		len=*cp&0x0f;
    		cp++;

			uvalue=0;
        	uint8_t nlen=len;
        	while (--nlen) {
            	uvalue<<=8;
           		uvalue|=*cp++;
        	}

			values[i].value = (type == 0x50 ? (int64_t) uvalue : uvalue);
		}
	}

	// Publish
	for (uint8_t i = 0; i < METRIC_LENGTH; i++) {
		METRICS[i].node
			.setProperty("value")
			.send(String((double)(values[i].value * (pow(10,values[i].scaler)))));
		Homie.getLogger() << "Published metric " << METRICS[i].name  
			<< " with value " << (long)values[i].value 
			<< ", unit " << (int)values[i].unit
			<< " and scaler " << (int)values[i].scaler << "." << endl;
	}

	// Start over
	reset();
}

void run_current_state() {
	if (state != NULL){
	    state();
    }
}



void setupHandler() {
    debug("Setting up application...");

	sensor.begin(9600);
	debug("Sensor has been initialized.");

	debug("Application setup finished.");
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