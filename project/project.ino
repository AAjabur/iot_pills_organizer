#include <NTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <strings_en.h>
#include <ESP8266WiFi.h>
#include <user_interface.h>

#define _IDLE 0
#define _FINISHED 1
#define _WAITING_TIME 2
#define _RELOADING 3

// Define global variables
StaticJsonDocument<1060> dates; // Stores the schedule to take the pills
int number_of_pills = 0; // How many pills can be stored at the same time
const int NUMBER_OF_POS = 8; // Number of servo positions
int servo_pos = 0; // Current servo position, changes in this variable makes servo move
int servo_angle = 0; // Current servo_angle, just for debug
int next_pill = 1; // When the pill box is loaded this variable stores the number of the next pill to be taked, in chronological order
int reloading_pill_n = 1; // When the pill box is reloading this variable represents the number of the bill being reloaded
const char *json_in_buffer; // When JsonDocuments desirialize some string, the string needs to be alocated, so this variable stores the string
int state = _IDLE; // Represent the current state that the pill box are

// pins
int servo_pin = 4; // D2
int button_pin = 14; // D5
int buzzer_pin = 12; // D6

// subscribed topics
const char *date_topic = "date/";
const char *reloading_pill_topic = "reload/pill_number";
const char *verification_topic = "verification/esp";
const char *debug_servo_angle_topic = "servo/debug/angle";
const char *debug_servo_pos_topic = "servo/debug/pos";
const char *set_state_topic = "state/set";

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

/**
* @brief Setup function
*/
void setup() {

  Serial.begin(115200);
  Serial.println("Abrindo o wifi manager");

  WiFiManager wifiManager;
  // Start Wifi Manager
  wifiManager.autoConnect("pill_box", "pill_box2022");

  Serial.println("Wifi manager conectado");

  // Configure MQTT broker
  client.setBufferSize(600); // Set max size of callback msg to 600
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
  client.subscribe(reloading_pill_topic);
  client.subscribe(verification_topic);
  client.subscribe(set_state_topic);

  //Start NTP Client
  timeClient.begin();

  // Normal IO defs
  pinMode(servo_pin, OUTPUT);
  analogWriteFreq(100);
  analogWriteRange(256);
  pinMode(button_pin, INPUT);
  pinMode(buzzer_pin, OUTPUT);
}

/**
* @brief Main loop
*/
void loop() {
  client.loop();
  timeClient.update();
  servo_to_pos(servo_pos);
  btn_was_pressed(); // Need to call to update button state

  switch(state)
  {
    case _IDLE:
    break;

    case _FINISHED:
      finished_state();
    break;

    case _WAITING_TIME:
      waiting_time_state();
    break;

    case _RELOADING:
      reloading_state();
    break;
  }
}

/**
* @brief Function that is called when a topic arrive in some subscribed topic
*/
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
      next_pill = reloading_pill_n;
    }
    number_of_pills = 7;
    state = _WAITING_TIME;
  }

  if(strcmp(topic, debug_servo_angle_topic) == 0){
    servo_angle = atoi(msg);
  }
  if(strcmp(topic, debug_servo_pos_topic) == 0){
    servo_pos = atoi(msg);
  }
  if(strcmp(topic, reloading_pill_topic) == 0){
    reloading_pill_n = atoi(msg);
  }
  if(strcmp(topic, verification_topic) == 0){
    if(atoi(msg)==0){ client.publish(verification_topic, "1"); }
  }
  if(strcmp(topic, set_state_topic) == 0){
    state = atoi(msg);
    if (state==_WAITING_TIME){
      servo_pos = reloading_pill_n-1;
    }
  }

  Serial.println();
  Serial.println("-----------------------");
}

/**
* @brief Routine to execute when in the waiting time state
*/
void waiting_time_state(){
  if (passed_next_pill_time()){
    digitalWrite(buzzer_pin, 1);
    if (btn_was_pressed() == 1){
      inc_servo_pos();
      if(!free_next_pill()){
        state = _FINISHED;
      }
    }
  }
  else {
    digitalWrite(buzzer_pin, 0);
  }
}

/**
* @brief Routine to execute when in the finished state
*/
void finished_state(){
  digitalWrite(buzzer_pin, 0);
  next_pill = 1;
}

/**
* @brief Routine to execute when in the reloading state
*/
void reloading_state(){
  servo_pos = reloading_pill_n;  
}

/**
* @brief Compare if passed the time to take the pill represented by
* the global variable next_pill
*
* @return Return 1 if the time has passed, else return 0
*/
int passed_next_pill_time() {
  if (!dates.isNull()){
    String next_pill_char = String(next_pill);
    const char *today = daysOfTheWeek[timeClient.getDay()];
    const char *pill_day = dates[next_pill_char]["weekday"];
    int right_day = strcmp(pill_day, today) == 0;
    int passed_hour = timeClient.getHours() > dates[next_pill_char]["hour"].as<int>();
    int right_hour = timeClient.getHours() == dates[next_pill_char]["hour"].as<int>();
    int passed_minute = timeClient.getMinutes() >= dates[next_pill_char]["minute"].as<int>();
    return right_day && passed_hour || right_hour && passed_minute;
  }
  else{
    return 0;
  }
}

/**
* @brief Rotate the servo to the given position
* currently there are 8 possible positions 0-7
*
* @param pos: between from 0 to 7
*/
void servo_to_pos(int pos){
  int pos_in_angle[] = {0, 23, 42, 64, 85, 109, 132, 157};
  servo_angle = pos_in_angle[pos];
  servo_to_angle(servo_angle);
}

/**
* @brief Rotate servo approximately to the given angle
*
* @param angle: angle to rotate servo, currently from 0 to 180
*/
void servo_to_angle(int angle){
  analogWrite(servo_pin, map(servo_angle, 0, 180, 9, 58));
}

/**
* @brief Return the state of the button
*
* @return int: 1 if button is pressed, else 0
*/
int btn_is_pressed(){
  return digitalRead(button_pin);
}

/**
* @brief This function return 1 for 1s after a button is pressed, after 1s without been
* pressed, the function starts to return 0
*
* @return int: 1 if pressed in the last second, else 0
*/
int btn_was_pressed(){
  static int was_pressed;
  static int was_unpressed;
  static unsigned long unpressed_time;
  unsigned long now = millis();

  if (btn_is_pressed() == 1){
    was_pressed = 1;
    was_unpressed = 0;
    unpressed_time = now;
  }
  else if(was_pressed == 1 && !was_unpressed) {
    unpressed_time = now;
    was_unpressed = 1;
  }
  if (was_pressed == 1 && (now - unpressed_time) > 1000){
    was_pressed = 0;
  }

  return was_pressed;
}

/**
* @brief Increment the global variable that represent the servo position,
* if the position goes beyond the max number of position, reset the position
*/
void inc_servo_pos(){
  static unsigned long inc_time;
  if(millis() - inc_time > 3000){
    servo_pos += 1;
    if (servo_pos > NUMBER_OF_POS-1 ){
      servo_pos = 0;
    }
    inc_time = millis();
  }
}

/**
* @brief Increment the global variable that represent the next pill number,
* if the variable goes beyond the current max number of pills, set the next pill to 1 and return 0
*
* @return Return 0 if number of pills goes beyond max number of pills, else return 1
*/
int free_next_pill(){
  // Return 1 if has next_pill and return 0 if not
  next_pill += 1;
  if(next_pill > number_of_pills){
    next_pill = 1;
    return 0;
  }
  return 1;
}