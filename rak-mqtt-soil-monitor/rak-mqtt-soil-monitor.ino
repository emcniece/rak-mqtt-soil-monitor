/**
   @file rak-mqtt-soil-monitor.ino
   @author github.com/emcniece
   @brief RAK11200 ESP32 WisBlock transmits temperature, humidity, battery health to a mqtt broker.
   @version 1.0
   @date 2022-11-05

   Attributions:
   - bernd.giesecke@rakwireless.com, taylor.lee@rakwireless.com
   - https://github.com/RAKWireless/WisBlock/blob/master/examples/RAK11200/solutions/mqtt_subscribe_publish/mqtt_subscribe_publish.ino
   - https://github.com/knolleary/pubsubclient
**/

// Locals
#include "arduino_secrets.h"
#include "shtc3.h"
#include "battery.h"
#include "wifi.h"
#include "mqtt.h"

float temperature = 0;
float humidity = 0;
long lastMsg = 0;

void setup() {
  Serial.begin(115200);

  setup_wifi();
  setup_shtc3();
  setup_battery();
  setup_mqtt();
}

void publish_temperature(){
  shtc3_read_data();
  temperature = g_shtc3.toDegC();

  // Convert the value to a char array
  char temperatureString[16] = { 0 };
  snprintf(temperatureString, sizeof(temperatureString), "%.1f", temperature);
  Serial.print("Temperature: ");
  Serial.println(temperatureString);

  // publish temperature
  mqttClient.publish("RAK11200/temperature", temperatureString);

  humidity = g_shtc3.toPercent();

  // Convert the value to a char array
  char humidityString[16] = { 0 };
  snprintf(humidityString, sizeof(humidityString), "%.1f", humidity);
  Serial.print("Humidity: ");
  Serial.println(humidityString);
  mqttClient.publish("RAK11200/humidity", humidityString);
}

void publish_battery(){
  float vbat_mv = read_battery_mv();
  char vbatMvString[16] = { 0 };
  snprintf(vbatMvString, sizeof(vbatMvString), "%.1f", vbat_mv);
  mqttClient.publish("RAK11200/battery_voltage", vbatMvString);
  Serial.print("Battery millivolts: ");
  Serial.println(vbatMvString);

  uint8_t vbat_pc = read_battery_percent();
  char vbatPcString[16] = { 0 };
  snprintf(vbatPcString, sizeof(vbatPcString), "%d", vbat_pc);
  mqttClient.publish("RAK11200/battery_percent", vbatPcString);
  Serial.print("Battery percentage: ");
  Serial.println(vbatPcString);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  long now = millis();
  if (now - lastMsg > 20000){  // publish every 20s
    lastMsg = now;

    publish_temperature();
    publish_battery();
  }
}
