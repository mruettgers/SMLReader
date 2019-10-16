#ifndef mqtt_publisher_h
#define mqtt_publisher_h

#include "publisher.h"

class MqttPublisher : Publisher
{
  public:
    MqttPublisher(): Publisher() {
    }
    void setup() {
    }
    void loop() {
    }
    void publish(metric_value *values) {
      	// Publish
        int32_t value;
        for (uint8_t i = 0; i < NUM_OF_METRICS; i++)
        {

          value = (uint32_t)((values[i].value * (pow(10, values[i].scaler))) * 1000);

          DEBUG("Published metric '%s':", METRICS[i].name);
          DEBUG("  Value: %ld", (long)values[i].value);
          DEBUG("  Unit: %d", (int)values[i].unit);
          DEBUG("  Scaler: %d", (int)values[i].scaler);
        }
    }
  private:
};


#endif