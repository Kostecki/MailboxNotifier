#include <MicroWakeupper.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "config.h"

// Enable/Disable debug-mode
#define DEBUG true
#define DEBUG_SERIAL \
  if (DEBUG)         \
  Serial

WiFiClient espClient;
HTTPClient https;
PubSubClient client(espClient);
MicroWakeupper microWakeupper;

DynamicJsonDocument doc(4096);

// Configs
const char *mqtt_broker;
int mqtt_port;
const char *mqtt_username;
const char *mqtt_password;
const char *mqtt_topic;
const char *mqtt_topic_low_voltage;
float low_voltage_threshold;

void setupWiFi()
{
  WiFi.begin(wifi_ssid, wifi_password);
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.print("Connecting to WiFi: ");
  DEBUG_SERIAL.print(wifi_ssid);

  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(500);
    DEBUG_SERIAL.print(".");
  }

  DEBUG_SERIAL.println();
  DEBUG_SERIAL.print("IP address: ");
  DEBUG_SERIAL.println(WiFi.localIP());
}

void setupConfig()
{
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println("Fetching config from: " + configURL);

  if (https.begin(espClient, configURL))
  {
    https.addHeader("update-last-seen", "true");
    int httpCode = https.GET();
    DEBUG_SERIAL.println("Response code: " + String(httpCode));
    if (httpCode > 0)
    {
      String JSONConfig = https.getString();
      DeserializationError error = deserializeJson(doc, JSONConfig);

      if (error)
      {
        DEBUG_SERIAL.print(F("deserializeJson() failed: "));
        DEBUG_SERIAL.println(error.f_str());
        return;
      }

      for (JsonObject elem : doc.as<JsonArray>())
      {
        const int id = elem["id"];
        const bool enabled = elem["enabled"];

        switch (id)
        {
        case 8:
        {
          if (enabled)
          {
            mqtt_port = elem["value"];
          }
          break;
        }

        case 9:
        {
          if (enabled)
          {
            mqtt_username = elem["value"];
          }
          break;
        }

        case 10:
        {
          if (enabled)
          {
            mqtt_password = elem["value"];
          }
          break;
        }

        case 11:
        {
          if (enabled)
          {
            mqtt_topic = elem["value"];
          }
          break;
        }

        case 12:
        {
          if (enabled)
          {
            mqtt_topic_low_voltage = elem["value"];
          }
          break;
        }

        case 13:
        {
          if (enabled)
          {
            low_voltage_threshold = elem["value"];
          }
          break;
        }

        case 14:
        {
          if (enabled)
          {
            mqtt_broker = elem["value"];
          }
          break;
        }

        default:
          break;
        }
      }
    }
    https.end();
  }
  else
  {
    DEBUG_SERIAL.printf("[HTTP] Unable to connect\n");
  }
}

void setupMQTT()
{
  client.setServer(mqtt_broker, mqtt_port);
  while (!client.connected())
  {
    String client_id = "esp8266-client-";
    client_id += String(WiFi.macAddress());

    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      DEBUG_SERIAL.println();
      DEBUG_SERIAL.print("Connected to MQTT topic: ");
      DEBUG_SERIAL.println(mqtt_topic);
    }
  }
}

void setup()
{
#ifdef DEBUG
  Serial.begin(9600);
  delay(100);
#endif

  setupWiFi();
  setupConfig();
  setupMQTT();

  microWakeupper.begin();
}

void loop()
{
  const float battery_voltage = microWakeupper.readVBatt();

  DEBUG_SERIAL.print("Current Battery Voltage: ");
  DEBUG_SERIAL.println(battery_voltage);

  // MQTT
  DEBUG_SERIAL.println("Publishing to MQTT");
  client.publish(mqtt_topic, "true");
  if (battery_voltage <= low_voltage_threshold)
  {
    DEBUG_SERIAL.println("Low battery");
    client.publish(mqtt_topic_low_voltage, "true");
  }
  else
  {
    client.publish(mqtt_topic_low_voltage, "false");
  }
  client.disconnect();
  espClient.flush();

  microWakeupper.reenable();

  ESP.deepSleep(0);
}
