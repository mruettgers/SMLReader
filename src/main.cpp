#include <list>
#include <sml/sml_file.h>
#include <IotWebConf.h>
#include "debug.h"
#include "MqttPublisher.h"
#include "config.h"
#include "webconf.h"
#include "Sensor.h"

void wifiConnected();
void status(WebServer*);
void configSaved();

WebConf *webConf;

MqttConfig mqttConfig;
MqttPublisher publisher;

uint8_t numOfSensors;
SensorConfig sensorConfigs[MAX_SENSORS];
Sensor *sensors[MAX_SENSORS];

uint16_t deepSleepInterval;

uint64_t lastMessageTime = 0;

bool connected = false;

void process_message(byte *buffer, size_t len, Sensor *sensor)
{
    lastMessageTime = millis64();
    // Parse
    sml_file *file = sml_file_parse(buffer + 8, len - 16);

    DEBUG_SML_FILE(file);

    publisher.publish(sensor, file);

    // free the malloc'd memory
    sml_file_free(file);
}

void setup()
{
    // Setup debugging stuff
    SERIAL_DEBUG_SETUP(115200);

#ifdef DEBUG
    // Delay for getting a serial console attached in time
    delay(2000);
#endif
    webConf = new WebConf(&wifiConnected, &status);

    webConf->loadWebconf(mqttConfig, sensorConfigs, numOfSensors, deepSleepInterval);

    // Setup MQTT publisher
    publisher.setup(mqttConfig);

    DEBUG("Setting up %d configured sensors...", numOfSensors);
    const SensorConfig *config  = sensorConfigs;
    for (uint8_t i = 0; i < numOfSensors; i++, config++)
    {
        Sensor *sensor = new Sensor(config, process_message);
        sensors[i] = sensor;
    }
    DEBUG("Sensor setup done.");

    DEBUG("Setup done.");
}

void loop()
{	
    if (webConf->needReset)
    {
        // Doing a chip reset caused by config changes
        DEBUG("Rebooting after 1 second.");
        delay(1000);
        ESP.restart();
    }

    bool allSensorsProcessedMessage=true;
    // Execute sensor state machines
    for (uint8_t i = 0; i < numOfSensors; i++)
    {
        sensors[i]->loop();
        allSensorsProcessedMessage&=sensors[i]->hasProcessedMessage();
    }

    webConf->doLoop();
    
    if(connected && deepSleepInterval > 0 && allSensorsProcessedMessage)
    {
        if(publisher.isConnected())
        {
            DEBUG("Disconnecting MQTT before deep sleep.");
            publisher.disconnect();
        }
        else
        {
            DEBUG("Going to deep sleep for %d seconds.", deepSleepInterval);
            ESP.deepSleep(deepSleepInterval * 1000000);
        }
    }
    delay(1);
}

void status(WebServer* server)
{
    char buffer[1024], *b = buffer;
    b+=sprintf(b, "{\n");
    b+=sprintf(b, "  \"chipId\":\"%08X\",\n", ESP.getChipId());
    b+=sprintf(b, "  \"uptime64\":%llu,\n", millis64());
    b+=sprintf(b, "  \"uptime\":%lu,\n", millis());
    b+=sprintf(b, "  \"lastMessageTime\":%llu,\n", lastMessageTime);
    b+=sprintf(b, "  \"mqttConnected\":%u,\n", publisher.isConnected());
    b+=sprintf(b, "  \"version\":\"%s\"\n", VERSION);
    b+=sprintf(b, "}");
    server->send(200, "application/json", buffer);
}

void wifiConnected()
{
    DEBUG("WiFi connection established.");
    connected = true;
    publisher.connect();
}