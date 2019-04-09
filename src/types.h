#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include <vector>
#include <string.h>
#include <list>

struct metric
{
    const char *name;
    const std::vector<byte> pattern;
};

struct metric_value
{
    int64_t value;
    uint8_t unit;
    int8_t scaler;
};

#endif