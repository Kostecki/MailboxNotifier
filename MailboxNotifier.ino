#include <MicroWakeupper.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "config.h"

// Enable/Disable debug-mode
#define DEBUG false
#define DEBUG_SERIAL \
  if (DEBUG)         \
  Serial

WiFiClient espClient;
HTTPClient https;
PubSubClient client(espClient);
MicroWakeupper microWakeupper;

DynamicJsonDocument doc(4096);
StaticJsonDocument<32> voltageDoc;
StaticJsonDocument<64> notificationDoc;

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
        const char *key = elem["key"];
        const bool enabled = elem["enabled"];

        if (strcmp(key, "mqtt-broker") == 0)
        {
          if (enabled)
          {
            mqtt_broker = elem["value"];
          }
        }
        else if (strcmp(key, "mqtt-port") == 0)
        {
          if (enabled)
          {
            mqtt_port = elem["value"];
          }
        }
        else if (strcmp(key, "mqtt-username") == 0)
        {
          if (enabled)
          {
            mqtt_username = elem["value"];
          }
        }
        else if (strcmp(key, "mqtt-password") == 0)
        {
          if (enabled)
          {
            mqtt_password = elem["value"];
          }
        }
        else if (strcmp(key, "mqtt-topic") == 0)
        {
          if (enabled)
          {
            mqtt_topic = elem["value"];
          }
        }
        else if (strcmp(key, "low-voltage-topic") == 0)
        {
          if (enabled)
          {
            mqtt_topic_low_voltage = elem["value"];
          }
        }
        else if (strcmp(key, "low-voltage-threshold") == 0)
        {
          if (enabled)
          {
            low_voltage_threshold = elem["value"];
          }
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

    DEBUG_SERIAL.println("Sending ntfy notification");
    if (https.begin(espClient, ntfyURL))
    {
      https.addHeader("Authorization", ntfyAuth);
      https.addHeader("Tags", "battery");
      https.addHeader("Title", "Postkasse: Lavt batteriniveau");
      int httpCode = https.POST(String(battery_voltage) + " Volt. Tid til at oplade!");
      DEBUG_SERIAL.println("Response code: " + String(httpCode));
      https.end();
    }
    else
    {
      DEBUG_SERIAL.printf("[HTTP] Unable to connect\n");
    }
  }
  else
  {
    client.publish(mqtt_topic_low_voltage, "false");
  }
  client.disconnect();
  espClient.flush();

  // Update Voltage
  DEBUG_SERIAL.println("Updating project deviceVoltage");
  voltageDoc["projectId"] = projectId;
  voltageDoc["reading"] = battery_voltage;
  String payload;
  serializeJson(voltageDoc, payload);

  if (https.begin(espClient, voltageUpdateURL))
  {
    https.addHeader("Content-Type", "application/json");
    int httpCode = https.PUT(payload);
    DEBUG_SERIAL.println("Response code: " + String(httpCode));
    https.end();
  }
  else
  {
    DEBUG_SERIAL.printf("[HTTP] Unable to connect\n");
  }

  microWakeupper.reenable();

  ESP.deepSleep(0);
}
