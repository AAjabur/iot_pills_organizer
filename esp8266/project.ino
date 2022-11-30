#include <NTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <strings_en.h>
#include <ESP8266WiFi.h>

// Define global variables
StaticJsonDocument<1024> dates;
const int NUMBER_OF_POS = 7;
int servo_pos;
int servo_angle = 0;
int servo_pin = 4; //D2

// subscribed topics
const char *date_topic = "date/";
const char *debug_servo_topic = "servo/debug/angle";

// MQTT Broker
const char *mqtt_broker = "broker.hivemq.com";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;

// NTP Client
const long utcOffsetInSeconds = -3*3600;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);


WiFiClient espClient;
PubSubClient client(espClient);

void setup() {

  Serial.begin(115200);
  Serial.println("Abrindo o wifi manager");

  WiFiManager wifiManager;
  // Start Wifi Manager
  wifiManager.autoConnect("ESP8266", "andre2000");

  Serial.println("Wifi manager conectado");

  // Configure MQTT broker
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(sub_callback);
  while (!client.connected()) {
      String client_id = "esp8266-client-";
      client_id += String(WiFi.macAddress());
      Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
      if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      } else {
          Serial.print("failed with state ");
          Serial.print(client.state());
          delay(2000);
      }
  }

  // Subscribe to topics
  client.subscribe(date_topic);
  client.subscribe(debug_servo_topic);

  //Start NTP Client
  timeClient.begin();

  // Normal IO defs
  pinMode(servo_pin, OUTPUT);
  analogWriteFreq(100);
  analogWriteRange(256);
  
}

void loop() {
  client.loop();
  timeClient.update();
  analogWrite(servo_pin, map(servo_angle, 0, 180, 9, 58));
  Serial.print(daysOfTheWeek[timeClient.getDay()]);
  Serial.print(", ");
  Serial.print(timeClient.getHours());
  Serial.print(":");
  Serial.print(timeClient.getMinutes());
  Serial.print(":");
  Serial.println(timeClient.getSeconds());
}

void sub_callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    char msg[length];
    memcpy(msg, payload, length); 
    Serial.print(msg);

    if (strcmp(topic, date_topic) == 0){
      DeserializationError err = deserializeJson(dates, msg);
      if (err) {
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(err.f_str());
      }
    }

    if(strcmp(topic, debug_servo_topic) == 0){
      servo_angle = atoi(msg);
    }
    Serial.println();
    Serial.println("-----------------------");
}
