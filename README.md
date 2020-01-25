# SMLReader

A smart meter (SML) to MQTT gateway

## Documentation

This documentation is a work in progress.
The previous version can be found [here](doc/old/README.md).

## About

The aim of this project is to read the meter readings of modern energy meters and make them available via MQTT.

The software was primarily developed and tested for the EMH ED300L electricity meter, but should also work with other energy meters that have an optical interface and communicate via the SML protocol.

SMLReader publishes the metrics read from the meter's optical unit to an MQTT broker configured via the provided web interface.




**Note:**  
The code parts that are responsible for publishing metrics via 1-Wire are not actively maintained.  
It is likely that support for 1-Wire will be dropped in the future as it is less reliable and data cannot be transmitted as fast as with MQTT.

### Screenshots
![WiFi and MQTT setup](doc/screenshot_setup.png)
![MQTT](doc/screenshot_mqtt.png)
![Grafana](doc/screenshot_grafana.png)

### Hardware

#### Reading head

The reading head consists of a phototransistor (BPW 40) and a 1 kΩ pull-up resistor connected to one of the GPIO pins of the microcontroller.
Other phototransistors or the use of an internal pull-up resistor will probably work, too.

The housing of my reading head has been 3D-printed using the [STL files](http://www.stefan-weigert.de/php_loader/sml.php) from [Stefan Weigert](http://www.stefan-weigert.de). 

A ring magnet (in my case dimensioned 27x21x3mm) ensures that the reading head keeps stuck on the meter.

The phototransistor has been fixed with hot glue within the housing.

![Reading Head](doc/assets/SMLReader_Img_ReadingHead_small.jpg "Reading Head") ![Reading Head](doc/assets/SMLReader_Img_ReadingHead_Close_small.jpg "Reading Head")

#### Schematic diagram
![Schematic diagram](doc/assets/SMLReader_Schema.png)

## Getting started

### Configuration

The configuration of the meter is done by editing `src/config.h` and adjusting the `metrics` variable before building and flashing SMLReader.

```c++
// EHM ED300L
static const metric METRICS[] = {
    {"power_in", {0x77, 0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF}},
    {"power_out", {0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x00, 0xFF}},
    {"power_current", {0x77, 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF}}};
```
The neccessary binary sequences start with `0x77 0x07` followed by the corresponding OBIS identifier. This identifier is probably  described in the meter's manual. If not, you can flash a debug build with verbose debugging enabled to see what identifiers are provided by your meter.
Verbose debugging can be enabled by setting `SERIAL_DEBUG_VERBOSE=true` in the `platformio.ini` file.


WiFi and MQTT are configured via the web interface provided by [IotWebConf](https://github.com/prampec/IotWebConf) and which can be reached after joining the WiFi network named SMLReader and heading to http://192.168.4.1.   
If the device has already been configured,  the web interface can be reached via the IP address obtained from your local network's DHCP server.


---

## Acknowledgements

### Third party libraries
* [FastCRC](https://github.com/FrankBoesing/FastCRC)
* [ESPSoftwareSerial](https://github.com/plerup/espsoftwareserial)
* [IotWebConf](https://github.com/prampec/IotWebConf)
* [MicroDebug](https://github.com/rlogiacco/MicroDebug)
* [MQTT](https://github.com/256dpi/arduino-mqtt)

### Links

* https://www.msxfaq.de/sonst/bastelbude/smartmeter_d0_sml_protokoll.htm
* https://www.photovoltaikforum.com/thread/78798-sml-pr%C3%BCfsummenberechnung-ich-verzweifle-an-crc/
* http://www.stefan-weigert.de/php_loader/sml.php

## Donate

<a href="https://www.buymeacoffee.com/fkqeNT2" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-green.png" alt="Buy Me A Coffee" height="51" width="217"></a>

## License

Distributed under the GPL v3 license.  
See [LICENSE](LICENSE) for more information.
