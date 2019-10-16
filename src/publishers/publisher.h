#ifndef publisher_h
#define publisher_h

#include "config.h"
#include "debug.h"

class Publisher
{
  public:
    virtual void setup();
    virtual void loop();
    virtual void publish(metric_value *values);
    Publisher() {
    }
  private:
};

#endif