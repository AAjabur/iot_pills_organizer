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
int servo_pos = 0;
int servo_angle = 0;
int servo_pin = 4; //D2
int next_pill = 1;
const char *int_to_str = "123456789";
const char *json_in_buffer;

// subscribed topics
const char *date_topic = "date/";
const char *debug_servo_angle_topic = "servo/debug/angle";
const char *debug_servo_pos_topic = "servo/debug/pos";

// MQTT Broker
const char *mqtt_broker = "broker.hivemq.com";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;

// NTP Client
const long utcOffsetInSeconds = -3*3600;
char daysOfTheWeek[7][12] = {"Domingo", "Segunda", "Terça", "Quarta", "Quinta", "Sexta", "Sábado"};
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
  client.subscribe(debug_servo_angle_topic);
  client.subscribe(debug_servo_pos_topic);

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
  servo_to_pos(servo_pos);
  if (passed_next_pill_time()) {
    Serial.println("Passou da hora");
    // if (botão apertado){
    //  Passa pro próximo remédio
    //  Publica falando que passou  
    // }
    // else {
    //  Se passar muito tempo publica falando que esqueceu
    // }
    //
    //
  }
}

void sub_callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    char msg[length];
    memcpy(msg, payload, length); 
    Serial.print(msg);

    if (strcmp(topic, date_topic) == 0){
      json_in_buffer = msg;  
      DeserializationError err = deserializeJson(dates, json_in_buffer);
      if (err) {
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(err.f_str());
      }
      else {
        next_pill = 1;
        String next_pill_char = String(next_pill);
      }
    }

    if(strcmp(topic, debug_servo_angle_topic) == 0){
      servo_angle = atoi(msg);
    }

    if(strcmp(topic, debug_servo_pos_topic) == 0){
      servo_pos = atoi(msg);
    }
    Serial.println();
    Serial.println("-----------------------");
}

int passed_next_pill_time() {
  if (!dates.isNull()){
    String next_pill_char = String(next_pill);
    const char *today = daysOfTheWeek[timeClient.getDay()];
    const char *pill_day = dates[next_pill_char]["weekday"];
    int right_day = strcmp(pill_day, today) == 0;
    int passed_hour = timeClient.getHours() >= dates[next_pill_char]["hour"].as<int>();
    int passed_minute = timeClient.getMinutes() >= dates[next_pill_char]["minute"].as<int>();
    return right_day && passed_hour && passed_minute;
  }
  else{
    return 0;
  }
}

void servo_to_pos(int pos){
  servo_angle = map(pos, 0, NUMBER_OF_POS-1, 0, 180);
  servo_to_angle(servo_angle);
}

void servo_to_angle(int angle){
  analogWrite(servo_pin, map(servo_angle, 0, 180, 9, 58));
}