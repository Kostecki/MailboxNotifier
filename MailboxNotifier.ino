#include <MicroWakeupper.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "config.h"

// Enable/Disable debug-mode
#define DEBUG false
#define DEBUG_SERIAL \
  if (DEBUG)         \
  Serial

WiFiClient espClient;           // WiFi
PubSubClient client(espClient); // MQTT
MicroWakeupper microWakeupper;  // MicroWakeupper

void setup()
{
#ifdef DEBUG
  Serial.begin(9600);
  delay(100);
#endif

  // WiFi
  WiFi.begin(wifi_ssid, wifi_pass);
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.print("Connecting to WiFi ");

  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(500);
    DEBUG_SERIAL.print(".");
  }

  DEBUG_SERIAL.println();
  DEBUG_SERIAL.print("IP address: ");
  DEBUG_SERIAL.println(WiFi.localIP());

  // MQTT
  client.setServer(mqtt_broker, mqtt_port);
  while (!client.connected())
  {
    String client_id = "esp8266-client-";
    client_id += String(WiFi.macAddress());

    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      DEBUG_SERIAL.print("Connected to MQTT topic: ");
      DEBUG_SERIAL.println(topic);
    }
  }

  microWakeupper.begin();
}

void loop()
{
  DEBUG_SERIAL.println();

  const float batteryVoltage = microWakeupper.readVBatt();

  DEBUG_SERIAL.print("Current Battery Voltage: ");
  DEBUG_SERIAL.println(batteryVoltage);

  // MQTT
  DEBUG_SERIAL.println("Publishing to MQTT..");
  client.publish(topic, "true");
  if (batteryVoltage <= batteryLowVoltage)
  {
    DEBUG_SERIAL.println("Low battery");
    client.publish(lowVoltageTopic, "true");
  }
  else
  {
    client.publish(lowVoltageTopic, "false");
  }
  client.disconnect();
  espClient.flush();

  microWakeupper.reenable();

  ESP.deepSleep(0);
}
