// #include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <mqtt_client.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <nvs_flash.h>

#include <HardwareSerial.h>

#define VERSION "0.0.1";

Preferences preferences;

esp_mqtt_client_handle_t client;

bool connected = false;
String ducoOutput = "";

enum DucoCommands {
  FAN_SPEED = 0,
  FAN_PARAM_GET
};

const char *const ducoCommand[] = {
  [FAN_SPEED] = "fanspeed",
  [FAN_PARAM_GET] = "fanparaget",
};

String discoveryTopic;
String stateTopic;
String commandTopic;

HardwareSerial LocalLog(0);
HardwareSerial DucoConsole(1);

static void log_error_if_nonzero(const char *message, int error_code) {
  if (error_code != 0) {
    LocalLog.print(F("Error: "));
    LocalLog.println(message);
    LocalLog.print(F("Error Code: "));
    LocalLog.println(error_code);
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

static void mqtt_before_connect_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  LocalLog.println(F("MQTT Client initialized, about to connect."));
}

static void mqtt_connected_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  int msg_id = esp_mqtt_client_subscribe(client, commandTopic.c_str(), 0);
  LocalLog.println(F("MQTT Connected, subscribing to topic"));
  connected = true;
}

static void mqtt_disconnected_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  LocalLog.println(F("MQTT Disconnected"));
  connected = false;
}

static void mqtt_subscribed_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  LocalLog.println(F("MQTT subscribed to topic"));
}

static void mqtt_unsubscribed_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  LocalLog.println(F("MQTT subscribed to topic"));
}

static void mqtt_published_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  LocalLog.println(F("MQTT published to topic"));
}

static void mqtt_data_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = esp_mqtt_event_handle_t(event_data);
  char topic[event->topic_len];
  char data[event->data_len];
  memcpy(topic, event->topic, event->topic_len);
  memcpy(data, event->data, event->data_len);
  if (commandTopic.compareTo(topic)) {
    LocalLog.println("Sending data to console");
    while(!DucoConsole.availableForWrite()) {
      LocalLog.println("Waiting for console to be available for write...");
      delay(50);
    }
    DucoConsole.println(data);
    DucoConsole.flush();

    // if(strcmp(data, ducoCommand[FAN_SPEED]) == 0) {
    // Serial.write(ducoCommand[FAN_SPEED]);
    // Serial.write(data);
    // Serial.write(0x0d);
    // delay(25);
    // Serial.write(0x0a);
    // publishFanSpeed();
    // Serial.println(data);
    // }
  }
}

static void publishFanSpeed() {
  esp_mqtt_client_publish(client, stateTopic.c_str(), "{\"speed\":50}", strlen("{\"speed\":50}"), 0, 0);
}

static void publishStateMessage(char const *message) {
  esp_mqtt_client_publish(client, stateTopic.c_str(), message, strlen(message), 0, 0);
}

static void publishBirthMessage() {
  esp_mqtt_client_publish(client, stateTopic.c_str(), "{\"status\":\"available\"}", strlen("{\"status\":\"available\"}"), 0, 0);
}

static void mqtt_error_hdl(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = esp_mqtt_event_handle_t(event_data);
  LocalLog.println(F("MQTT Error"));
  if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
      log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
      log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
  }
}

void setup() {
  LocalLog.begin(115200);
  DucoConsole.begin(115200, SERIAL_8N1, 16, 17);

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

  LocalLog.print("ESP32 Board MAC Address:  ");
  LocalLog.println(WiFi.macAddress());

  LocalLog.printf("Discovery Topic: %s\n\r", discoveryTopic.c_str());
  LocalLog.printf("State Topic: %s\n\r", stateTopic.c_str());
  LocalLog.printf("Command Topic: %s\n\r", commandTopic.c_str());

  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    LocalLog.println("Connecting to WiFi..");
  }
  LocalLog.print("Station IP Address: ");
  LocalLog.println(WiFi.localIP());
  LocalLog.print("Wi-Fi Channel: ");
  LocalLog.println(WiFi.channel());

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

  LocalLog.println("MQTT Client started");
  LocalLog.print("Connecting to: ");
  LocalLog.print(mqttUri);
  LocalLog.print(":");
  LocalLog.println(mqttPort);

  while (!connected) {
    delay(500);
  }

  publishBirthMessage();
}

void loop() {
 while (DucoConsole.available()) {
    LocalLog.println("Serial available, reading string...");
    ducoOutput = DucoConsole.readString();
    LocalLog.println("Output received: ");
    LocalLog.println(ducoOutput);
  }

  if(ducoOutput.length() > 0) {
    publishStateMessage(ducoOutput.c_str());
    ducoOutput = "";
  }
}