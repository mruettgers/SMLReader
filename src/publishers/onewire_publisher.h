#ifndef onewire_publisher_h
#define onewire_publisher_h

#include "publisher.h"

#include "OneWireHub.h"
#include "BAE910.h"

// Get ESP's chip ID for use in the BAE910's unique address
union {
	uint32_t value;
	uint8_t fields[4];
} const chip_id = {ESP.getChipId()};

class OneWirePublisher : Publisher
{
  public:
    OneWirePublisher(int pin): Publisher() {
      owHub = new OneWireHub(pin);
    }
    void setup() {
       // Because we have only 4 32 bit registers we will add multiple slave devices to the hub if required
       BAE910 *owDevice;
       for (int i = 0; NUM_OF_METRICS > 0 && i <= ((NUM_OF_METRICS - 1) / 4); i++)
       {
               owDevice = new BAE910(BAE910::family_code, 'S', 'M', i, chip_id.fields[0], chip_id.fields[1], chip_id.fields[2]);
               owSlaveDevices.push_back(owDevice);
               owDevice->memory.field.SW_VER = 0x01;
               owDevice->memory.field.BOOTSTRAP_VER = 0x01;
               owHub->attach(*owDevice);
       }
    }
    void loop() {
      owHub->poll();
    }
    void publish(metric_value *values) {
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
    }
  private:
    OneWireHub *owHub;
    std::list<BAE910 *> owSlaveDevices;
};


#endif