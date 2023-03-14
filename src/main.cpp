#include <list>
#include "config.h"
#include "debug.h"
#include <sml/sml_file.h>
#include "Sensor.h"
#include <IotWebConf.h>
#include "MqttPublisher.h"
#include "EEPROM.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

std::list<Sensor *> *sensors = new std::list<Sensor *>();

void wifiConnected();
void configSaved();

DNSServer dnsServer;
WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient net;

MqttConfig mqttConfig;
MqttPublisher publisher;

IotWebConf iotWebConf(WIFI_AP_SSID, &dnsServer, &server, WIFI_AP_DEFAULT_PASSWORD, CONFIG_VERSION);

iotwebconf::TextParameter mqttServerParam = iotwebconf::TextParameter("MQTT server", "mqttServer", mqttConfig.server, sizeof(mqttConfig.server), nullptr, mqttConfig.server);
iotwebconf::NumberParameter mqttPortParam = iotwebconf::NumberParameter("MQTT port", "mqttPort", mqttConfig.port, sizeof(mqttConfig.port), nullptr, mqttConfig.port);
iotwebconf::TextParameter mqttUsernameParam = iotwebconf::TextParameter("MQTT username", "mqttUsername", mqttConfig.username, sizeof(mqttConfig.username), nullptr, mqttConfig.username);
iotwebconf::PasswordParameter mqttPasswordParam = iotwebconf::PasswordParameter("MQTT password", "mqttPassword", mqttConfig.password, sizeof(mqttConfig.password), nullptr, mqttConfig.password);
iotwebconf::TextParameter mqttTopicParam = iotwebconf::TextParameter("MQTT topic", "mqttTopic", mqttConfig.topic, sizeof(mqttConfig.topic), nullptr, mqttConfig.topic);
iotwebconf::ParameterGroup paramGroup = iotwebconf::ParameterGroup("MQTT Settings", "");

boolean needReset = false;

void process_message(byte *buffer, size_t len, Sensor *sensor)
{
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

	// Setup reading heads
	DEBUG("Setting up %d configured sensors...", NUM_OF_SENSORS);
	const SensorConfig *config = SENSOR_CONFIGS;
	for (uint8_t i = 0; i < NUM_OF_SENSORS; i++, config++)
	{
		Sensor *sensor = new Sensor(config, process_message);
		sensors->push_back(sensor);
	}
	DEBUG("Sensor setup done.");

	// Initialize publisher
	// Setup WiFi and config stuff
	DEBUG("Setting up WiFi and config stuff.");

	paramGroup.addItem(&mqttServerParam);
	paramGroup.addItem(&mqttPortParam);
	paramGroup.addItem(&mqttUsernameParam);
	paramGroup.addItem(&mqttPasswordParam);
	paramGroup.addItem(&mqttTopicParam);

	iotWebConf.addParameterGroup(&paramGroup);

	iotWebConf.setConfigSavedCallback(&configSaved);
	iotWebConf.setWifiConnectionCallback(&wifiConnected);


	WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
      publisher.disconnect();
    });

	// -- Define how to handle updateServer calls.
	iotWebConf.setupUpdateServer(
		[](const char *updatePath)
		{ httpUpdater.setup(&server, updatePath); },
		[](const char *userName, char *password)
		{ httpUpdater.updateCredentials(userName, password); });

	boolean validConfig = iotWebConf.init();
	if (!validConfig)
	{
		DEBUG("Missing or invalid config. MQTT publisher disabled.");
	}
	else
	{
		// Setup MQTT publisher
		publisher.setup(mqttConfig);
	}

	server.on("/", []() { iotWebConf.handleConfig(); });
	server.on("/reset", []() { needReset = true; });
	server.onNotFound([]() { iotWebConf.handleNotFound(); });

	DEBUG("Setup done.");
}

void loop()
{
	if (needReset)
	{
		// Doing a chip reset caused by config changes
		DEBUG("Rebooting after 1 second.");
		delay(1000);
		ESP.restart();
	}

	// Execute sensor state machines
	for (std::list<Sensor*>::iterator it = sensors->begin(); it != sensors->end(); ++it){
		(*it)->loop();
	}
	iotWebConf.doLoop();
	yield();
}

void configSaved()
{
	DEBUG("Configuration was updated.");
	needReset = true;
}

void wifiConnected()
{
	DEBUG("WiFi connection established.");
	publisher.connect();
}