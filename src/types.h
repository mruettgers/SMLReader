#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include <vector>
#include <string.h>
#include <list>
#include <SoftwareSerial.h>

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

struct sensor_config
{
    const uint8_t pin;
    const char *name;
    const bool numeric_only;
};

struct sensor_state
{
    SoftwareSerial serial;
	const sensor_config *config;
	byte buffer[BUFFER_SIZE];
	size_t position = 0;
	unsigned long last_state_reset = 0;
	uint8_t bytes_until_checksum = 0;
	uint8_t loop_counter = 0;
	void (*state)() = NULL;
};

#endif