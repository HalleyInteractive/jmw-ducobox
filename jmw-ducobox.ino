// #include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <mqtt_client.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <nvs_flash.h>

#define VERSION "0.0.1";

Preferences preferences;

esp_mqtt_client_handle_t client;

bool connected = false;
bool birthMessagePublished = false;
unsigned long previousMillis = 0;

enum DucoCommands {
  FAN_SPEED = 0,
  FAN_PARAM_GET
};

const char * const ducoCommand[] = {
  [FAN_SPEED] = "fanspeed",
  [FAN_PARAM_GET] = "fanparaget",
};

String discoveryTopic;
String stateTopic;
String commandTopic;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
      // Serial.print(F("Error: "));
      // Serial.println(message);
      // Serial.print(F("Error Code: "));
      // Serial.println(error_code);
    }
}

// static void publishDiscoveryConfiguration() {
//   String payload;
//   StaticJsonDocument<200> doc;
//   doc["name"] = "Ducobox";
//   doc["state_topic"]   = stateTopic;
//   doc["command_topic"] = commandTopic;
//   doc["object_id"] = "ducobox_1_object_id";
//   // doc["percentage_state_topic"] = percentageStateTopic;
//   doc["unique_id"] = "ducobox_1_unique_id";
//   doc["expire_after"] = 1000 * 60 * 60;
//   doc["dev"]["sw_version"] = VERSION;
//   doc["dev"]["identifiers"] = "ducobox_1_identifier";
//   doc["dev"]["name"] = "Ducobox";
//   doc["dev"]["suggested_area"] = "Basement";
//   doc["dev"]["manufacturer"] = "Halley Interactive";

//   serializeJson(doc, payload);
//   serializeJsonPretty(doc, Serial);
//   esp_mqtt_client_publish(client, discoveryTopic.c_str(), payload.c_str(), strlen(payload.c_str()), 0, 0);
// }

static void mqtt_before_connect_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  // Serial.println(F("MQTT Client initialized, about to connect."));
}

static void mqtt_connected_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  int msg_id = esp_mqtt_client_subscribe(client, commandTopic.c_str(), 0);
  // Serial.println(F("MQTT Connected, subscribing to topic"));
  connected = true;
}

static void mqtt_disconnected_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  // Serial.println(F("MQTT Disconnected"));
  connected = false;
}

static void mqtt_subscribed_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  // Serial.println(F("MQTT subscribed to topic"));
}

static void mqtt_unsubscribed_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  // Serial.println(F("MQTT subscribed to topic"));
}

static void mqtt_published_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  // Serial.println(F("MQTT published to topic"));
}

static void mqtt_data_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  esp_mqtt_event_handle_t event = esp_mqtt_event_handle_t(event_data);
  char topic[event->topic_len];
  char data[event->data_len];
  memcpy(topic, event->topic, event->topic_len);
  memcpy(data, event->data, event->data_len);
  if(commandTopic.compareTo(topic)) {
    if(strcmp(data, ducoCommand[FAN_SPEED]) == 0) {
      Serial.println(ducoCommand[FAN_SPEED]);
      // publishFanSpeed();
    }
  }
}

static void publishFanSpeed() {
  esp_mqtt_client_publish(client, stateTopic.c_str(), "{\"speed\":50}", strlen("{\"speed\":50}"), 0, 0);
}

static void publishStateMessage(char* message) {
  esp_mqtt_client_publish(client, stateTopic.c_str(), message, strlen(message), 0, 0);
}

static void publishBirthMessage() {
  birthMessagePublished = true;
  esp_mqtt_client_publish(client, stateTopic.c_str(), "{\"status\":\"available\"}", strlen("{\"status\":\"available\"}"), 0, 0);
}

static void mqtt_error_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  esp_mqtt_event_handle_t event = esp_mqtt_event_handle_t(event_data);
    // Serial.println(F("MQTT Error"));
    // if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
    //     log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
    //     log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
    //     log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
    // }
}

void setup()
{
  Serial.begin(115200);

    preferences.begin("sensor-hub", false);

    String ssid = preferences.getString("ssid");
    String password = preferences.getString("password");
    String mqttUri = preferences.getString("mqtt-uri");
    unsigned int mqttPort = preferences.getUInt("mqtt-port");
    String mqttUser = preferences.getString("mqtt-user");
    String mqttPass = preferences.getString("mqtt-pass");
    discoveryTopic = preferences.getString("discovery-tpc");
    stateTopic = preferences.getString("state-tpc");
    commandTopic = preferences.getString("command-tpc");

    preferences.end();

    WiFi.mode(WIFI_STA);
    
    // Serial.print("ESP32 Board MAC Address:  ");
    // Serial.println(WiFi.macAddress());

    // Serial.printf("Discovery Topic: %s\n\r", discoveryTopic.c_str());
    // Serial.printf("State Topic: %s\n\r", stateTopic.c_str());
    // Serial.printf("Command Topic: %s\n\r", commandTopic.c_str());

    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      // Serial.println("Connecting to WiFi..");
    }
    // Serial.print("Station IP Address: ");
    // Serial.println(WiFi.localIP());
    // Serial.print("Wi-Fi Channel: ");
    // Serial.println(WiFi.channel());

    esp_mqtt_client_config_t config = {};
    config.uri = mqttUri.c_str();
    config.port = mqttPort;
    config.username = mqttUser.c_str();
    config.password = mqttPass.c_str();
    client = esp_mqtt_client_init(&config);

    esp_mqtt_client_register_event(client, MQTT_EVENT_BEFORE_CONNECT, mqtt_before_connect_hdl, NULL);
    esp_mqtt_client_register_event(client, MQTT_EVENT_CONNECTED, mqtt_connected_hdl, NULL);
    esp_mqtt_client_register_event(client, MQTT_EVENT_DISCONNECTED, mqtt_disconnected_hdl, NULL);
    esp_mqtt_client_register_event(client, MQTT_EVENT_SUBSCRIBED, mqtt_subscribed_hdl, NULL);
    esp_mqtt_client_register_event(client, MQTT_EVENT_UNSUBSCRIBED, mqtt_unsubscribed_hdl, NULL);
    esp_mqtt_client_register_event(client, MQTT_EVENT_PUBLISHED, mqtt_published_hdl, NULL);
    esp_mqtt_client_register_event(client, MQTT_EVENT_DATA, mqtt_data_hdl, NULL);
    esp_mqtt_client_register_event(client, MQTT_EVENT_ERROR, mqtt_error_hdl, NULL);

    esp_mqtt_client_start(client);
    // Serial.println("MQTT Client started");
    // Serial.print("Connecting to: ");
    // Serial.print(mqttUri);
    // Serial.print(":");
    // Serial.println(mqttPort);

}

const unsigned int MAX_MESSAGE_LENGTH = 1024;

void loop()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > 1000)
  {
    if (connected && !birthMessagePublished) {
      publishBirthMessage();
    }
    previousMillis = currentMillis;
  }

  while (Serial.available() > 0) {
    static char message[MAX_MESSAGE_LENGTH];
    static unsigned int message_pos = 0;
    char inByte = Serial.read();

    if ( inByte != '\n' && (message_pos < MAX_MESSAGE_LENGTH - 1) ) {
      message[message_pos] = inByte;
      message_pos++;
    } else {
      message[message_pos] = '\0';
      publishStateMessage(message);
      message_pos = 0;
    }
  }
}