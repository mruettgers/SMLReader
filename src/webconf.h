#ifndef WEBCONF_H
#define WEBCONF_H

#include <ESP8266WiFi.h>
#include <ESP8266HTTPUpdateServer.h>
#include <IotWebConf.h>
#include "debug.h"
#include "MqttPublisher.h"

const uint8_t MAX_SENSORS = 9;

struct GeneralWebConfig
{
	char numberOfSensors[2] = "0";
    char deepSleepInterval[5] = "";
};

struct MqttWebConfig
{
    char server[128] = "mosquitto";
    char port[8] = "1883";
    char username[128] = "";
    char password[128] = "";
    char topic[128] = "iot/smartmeter/";
    char jsonPayload[9] = "";
};

struct SensorWebConfig
{
    char pin[2] = { D2+'A','\0' };
    char name[32] = "sensor0";
    char numeric_only[9] = "selected";
    char status_led_enabled[9] = "selected";
    char status_led_inverted[9] = "selected";
    char status_led_pin[2] = { D4+'A','\0' };
    char interval[5] = "0";
};

struct WebConfGroups
{
    iotwebconf::ParameterGroup *generalGroup;
    iotwebconf::ParameterGroup *mqttGroup;
    iotwebconf::ParameterGroup *sensorGroups[MAX_SENSORS];
};

struct SensorStrings{
	char grpid[8] = "sensor0";
	char grpname[9] = "Sensor 0";
	char pin[6] = "s0pin";
	char name[7] = "s0name";
	char numOnly[10] = "s0numOnly";
    char ledEnabled[10] = "s0ledE";
	char ledInverted[10] = "s0ledI";
	char ledPin[10] = "s0ledP";
	char interval[9] = "s0int";
};

SensorStrings sensorStrings[MAX_SENSORS];
const uint8_t NUMBER_OF_PINS=9;
const char pinOptions[] = { D0+'A', '\0', D1+'A', '\0', D2+'A', '\0', D3+'A', '\0', D4+'A', '\0', D5+'A', '\0', D6+'A', '\0', D7+'A', '\0', D8+'A', '\0'};
const uint8_t PIN_LABEL_LENGTH = 3;
const char *pinNames[] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8"};


class WebConf
{
private:
    GeneralWebConfig general;
    MqttWebConfig mqtt;
    SensorWebConfig sensors[9];
    WebConfGroups groups;
    IotWebConf *iotWebConf;
    WebServer *webServer;
    DNSServer *dnsServer;
    ESP8266HTTPUpdateServer *httpUpdater;
    std::function<void(WebServer*)> status;

public:
    bool needReset = false;

    WebConf(std::function<void()> wifiConnected, std::function<void(WebServer*)> status){
        webServer = new WebServer();
        dnsServer = new DNSServer();
        httpUpdater = new ESP8266HTTPUpdateServer();
        iotWebConf = new IotWebConf(WIFI_AP_SSID, dnsServer, webServer, WIFI_AP_DEFAULT_PASSWORD, CONFIG_VERSION);

        this->status = status;

        DEBUG("Setting up WiFi and config stuff.");

        this->setupWebconf();

        iotWebConf->setConfigSavedCallback([this] { this->configSaved(); } );
        iotWebConf->setWifiConnectionCallback(wifiConnected);
        iotWebConf->setupUpdateServer(
            [this](const char* updatePath) { this->httpUpdater->setup(this->webServer, updatePath); },
            [this](const char* userName, char* password) { this->httpUpdater->updateCredentials(userName, password); }
        );

        webServer->on("/", [this] { this->iotWebConf->handleConfig(); });
        webServer->on("/status", [this] { this->status(this->webServer); } );
        webServer->on("/reset", [this] { needReset = true; });
        webServer->onNotFound([this] { this->iotWebConf->handleNotFound(); });
    }

    void doLoop(){
	    iotWebConf->doLoop();
    }

    void setupWebconf(){
        GeneralWebConfig &generalConfig = this->general;
        MqttWebConfig &mqttConfig = this->mqtt;

        using namespace iotwebconf;
        iotWebConf->getApTimeoutParameter()->visible = true;

        ParameterGroup* &generalGroup = this->groups.generalGroup = new ParameterGroup("general","General");    
        static char numOfSensorsValidator[] = "min='0' max='0'";
        numOfSensorsValidator[13] = MAX_SENSORS + '0';
        generalGroup->addItem(new NumberParameter("Number of sensors", "numOfSensors", generalConfig.numberOfSensors, sizeof(generalConfig.numberOfSensors), generalConfig.numberOfSensors, nullptr, numOfSensorsValidator));    
        generalGroup->addItem(new NumberParameter("Deep sleep interval (s)", "deepSleep", generalConfig.deepSleepInterval, sizeof(generalConfig.deepSleepInterval), generalConfig.deepSleepInterval, nullptr, "min='0' max='3600'"));
        iotWebConf->addParameterGroup(generalGroup);

        ParameterGroup* &mqttGroup = this->groups.mqttGroup = new ParameterGroup("mqtt","MQTT");
        mqttGroup->addItem(new TextParameter("MQTT server", "mqttServer", mqttConfig.server, sizeof(mqttConfig.server), mqttConfig.server));
        mqttGroup->addItem(new TextParameter("MQTT port", "mqttPort", mqttConfig.port, sizeof(mqttConfig.port), mqttConfig.port));
        mqttGroup->addItem(new TextParameter("MQTT username", "mqttUsername", mqttConfig.username, sizeof(mqttConfig.username), mqttConfig.username));
        mqttGroup->addItem(new PasswordParameter("MQTT password", "mqttPassword", mqttConfig.password, sizeof(mqttConfig.password), mqttConfig.password));
        mqttGroup->addItem(new TextParameter("MQTT topic", "mqttTopic", mqttConfig.topic, sizeof(mqttConfig.topic), mqttConfig.topic));
        mqttGroup->addItem(new CheckboxParameter("MQTT JSON Payload", "mqttJsonPayload", mqttConfig.jsonPayload, sizeof(mqttConfig.jsonPayload), mqttConfig.jsonPayload));
        iotWebConf->addParameterGroup(mqttGroup);

        for(byte i=0; i<MAX_SENSORS; i++){
            char sensorIdChar = i + '0';
            SensorStrings &strs = sensorStrings[i];
            strs.grpid[6] = sensorIdChar;
            strs.grpname[7] = sensorIdChar;
            strs.pin[1] = sensorIdChar;
            strs.name[1] = sensorIdChar;
            strs.numOnly[1] = sensorIdChar;
            strs.ledEnabled[1] = sensorIdChar;
            strs.ledInverted[1] = sensorIdChar;
            strs.ledPin[1] = sensorIdChar;
            strs.interval[1] = sensorIdChar;
            SensorWebConfig &cfg = this->sensors[i];
            
            ParameterGroup* &sensorGroup = this->groups.sensorGroups[i] = new ParameterGroup(strs.grpid, strs.grpname);
            sensorGroup->visible = false;
            sensorGroup->addItem(new SelectParameter("Pin", strs.pin, cfg.pin, sizeof(cfg.pin), pinOptions, *pinNames, NUMBER_OF_PINS, PIN_LABEL_LENGTH, cfg.pin));
            sensorGroup->addItem(new TextParameter("Name", strs.name, cfg.name, sizeof(cfg.name), cfg.name));
            sensorGroup->addItem(new CheckboxParameter("Numeric Values Only", strs.numOnly, cfg.numeric_only, sizeof(cfg.numeric_only), cfg.numeric_only));
            sensorGroup->addItem(new CheckboxParameter("Led Enabled", strs.ledEnabled, cfg.status_led_enabled, sizeof(cfg.status_led_enabled), cfg.status_led_enabled));
            sensorGroup->addItem(new CheckboxParameter("Led Inverted", strs.ledInverted, cfg.status_led_inverted, sizeof(cfg.status_led_inverted), cfg.status_led_inverted));
            sensorGroup->addItem(new SelectParameter("Led Pin", strs.ledPin, cfg.status_led_pin, sizeof(cfg.status_led_pin), pinOptions, *pinNames, NUMBER_OF_PINS, PIN_LABEL_LENGTH, cfg.status_led_pin));
            sensorGroup->addItem(new NumberParameter("Interval (s)", strs.interval, cfg.interval, sizeof(cfg.interval), cfg.interval));
            iotWebConf->addParameterGroup(sensorGroup);
        }
    }

    void loadWebconf(MqttConfig &mqttConfig, SensorConfig sensorConfigs[MAX_SENSORS], uint8_t &numOfSensors, uint16_t &deepSleepInterval){
        boolean validConfig = iotWebConf->init();
        if (!validConfig)
        {
            DEBUG("Missing or invalid config. MQTT publisher disabled.");
            MqttConfig defaults;
            // Resetting to default values
            strcpy(mqttConfig.server, defaults.server);
            strcpy(mqttConfig.port, defaults.port);
            strcpy(mqttConfig.username, defaults.username);
            strcpy(mqttConfig.password, defaults.password);
            strcpy(mqttConfig.topic, defaults.topic);

            numOfSensors = 0;
            deepSleepInterval = 0;

            for (uint8_t i = 0; i < MAX_SENSORS; i++)
            {
                this->groups.sensorGroups[i]->visible = false;
            }
        }
        else
        {
            strcpy(mqttConfig.jsonPayload, this->mqtt.jsonPayload);
            strcpy(mqttConfig.password, this->mqtt.password);
            strcpy(mqttConfig.port, this->mqtt.port);
            strcpy(mqttConfig.server, this->mqtt.server);
            strcpy(mqttConfig.topic, this->mqtt.topic);
            strcpy(mqttConfig.username, this->mqtt.username);

            numOfSensors = this->general.numberOfSensors[0] - '0';
            numOfSensors = numOfSensors < MAX_SENSORS ? numOfSensors : MAX_SENSORS;
            for (uint8_t i = 0; i < numOfSensors; i++)
            {
                this->groups.sensorGroups[i]->visible = i < numOfSensors;
                sensorConfigs[i].interval = atoi(this->sensors[i].interval);
                sensorConfigs[i].name = this->sensors[i].name;
                sensorConfigs[i].numeric_only = this->sensors[i].numeric_only[0] == 's';
                sensorConfigs[i].pin = this->sensors[i].pin[0] - 'A';
                sensorConfigs[i].status_led_enabled = this->sensors[i].status_led_enabled[0] == 's';
                sensorConfigs[i].status_led_inverted = this->sensors[i].status_led_inverted[0] == 's';
                sensorConfigs[i].status_led_pin = this->sensors[i].status_led_pin[0] - 'A';
            }

            deepSleepInterval = atoi(this->general.deepSleepInterval);
        }
    }
    
    void configSaved()
    {
        DEBUG("Configuration was updated.");
        needReset = true;
    }
};

#endif