#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "constants.h"

int second = 0; 
int temp = 0;
int lamp_mode = 0; //1-auto 0-manual
int pump_mode = 0; //1-auto 0-manual
//----------------------------------------------------------
//Sensors
OneWire oneWire(constant_device::TempPin);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor1;
//----------------------------------------------------------
//Wifi
WiFiClient wifiClient;
//----------------------------------------------------------
//NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
//----------------------------------------------------------

//----------------------------------------------------------
//Pump Control
void pump_off();
void pump_on();
//----------------------------------------------------------
//Lamp Control
void lamp_off();
void lamp_on();
//----------------------------------------------------------
//void publishTopic(const char* topic, String msg);
void callback(char* topic, byte* payload, unsigned int length);

PubSubClient client(constant_mqtt::server, constant_mqtt::PORT,callback, wifiClient);
//MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  unsigned int i = 0;
  char message_buff[100];
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  String messageTopic = String(message_buff);
  if (strcmp(topic,constant_device::LampTopic)==0) {
    Serial.println(messageTopic);
    if(strcmp(message_buff,"off")==0){
      lamp_off();
      lamp_mode = 0;
      client.publish(constant_device::LampTopicAutoStatus, "0", true);
    }
    else if(strcmp(message_buff,"on")==0 ){  
      lamp_on();
      lamp_mode = 0;      
      client.publish(constant_device::LampTopicAutoStatus, "0", true);
    }
    if (strcmp(message_buff,"autolamp")==0 ){
      Serial.println("auto"); 
      lamp_mode = 1;
      client.publish(constant_device::LampTopicAutoStatus, "1", true);
    }  
  }  

  if (strcmp(topic,constant_device::PumpTopic)==0) {
    if(messageTopic == "off"){
      pump_off();
      Serial.println("off");
      pump_mode = 0;
      client.publish(constant_device::PumpTopicAutoStatus, "0", true);    
    }
    else if(messageTopic == "on"){
      pump_on();
      Serial.println("on");
      pump_mode = 0;
      client.publish(constant_device::PumpTopicAutoStatus, "0", true);    
    }  
    if (messageTopic == "autopump"){
      Serial.println("pump");
      pump_mode = 1;
      client.publish(constant_device::PumpTopicAutoStatus, "1", true);    
    }   
  }
} 

//----------------------------------------------------------
//MQTT - Functions
void subscribeMQTT(){
  client.subscribe(constant_device::LampTopic);
  client.subscribe(constant_device::PumpTopic);
  client.subscribe(constant_device::TempTopic);
}

bool checkMqttConnection(){
  if (!client.connected()) {
    client.connect(constant_commom::HOSTNAME.c_str(),constant_mqtt::MQTT_USERNAME,constant_mqtt::MQTT_PASSWORD,constant_mqtt::WILL_TOPIC, 0, 0,constant_mqtt::WILL_MESSAGE, 0);
    subscribeMQTT();
  }
  return client.connected();
}
//----------------------------------------------------------
//Sensors - Functions
void locate_Sensors()
{
  Serial.println("Locatting sensors...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" sensors.");
  if (!sensors.getAddress(sensor1,0)) 
     Serial.println("Sensors not found!"); 
  Serial.print("Sensor address: ");
}

void show_sensor_address()
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (sensor1[i] < 16) Serial.print("0");
    Serial.print(sensor1[i], HEX);
  }
}
//----------------------------------------------------------
//Wifi - Functions
void connectWifi(){
  WiFi.begin(constant_wifi::SSID,constant_wifi::PASSWORD);
  WiFi.mode(WIFI_STA);
  Serial.println(".");
  // Aguarda até estar ligado ao Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.hostname(constant_commom::HOSTNAME);
  Serial.println("");
  Serial.print("Ligado a ");
  Serial.println(constant_wifi::SSID);
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}
//----------------------------------------------------------
//Pump Control
void pump_off(){
  digitalWrite(constant_device::PumpRelayPin, LOW);
  client.publish(constant_device::PumpTopicStatus, "off", true);
}

void pump_on(){
  digitalWrite(constant_device::PumpRelayPin, HIGH);
  client.publish(constant_device::PumpTopicStatus, "on", true);
}
//----------------------------------------------------------
//Lamp Control
void lamp_off(){
  digitalWrite(constant_device::LampRelayPin, LOW);
  client.publish(constant_device::LampTopicStatus, "off", true);
}

void lamp_on(){
  digitalWrite(constant_device::LampRelayPin, HIGH);
  client.publish(constant_device::LampTopicStatus, "on", true);
}
//----------------------------------------------------------

void autoModeLamp(){
  timeClient.update();
  if( (timeClient.getHours() >= 12 ) && (timeClient.getHours() < 17) ){
    lamp_on();
  }
  else{
    lamp_off();
  }
}

void autoModePump(){
  timeClient.update();
  if( (timeClient.getHours() >= 9 ) && (timeClient.getHours() < 22) ){
    pump_on();
  }
  else{
    pump_off();
  }
}

void setup() {
  pinMode(constant_device::PumpRelayPin,OUTPUT);
  pinMode(constant_device::LampRelayPin,OUTPUT);  
  pinMode(constant_device::TempPin,INPUT);  
  Serial.begin(115200);
  timeClient.begin();
  connectWifi();
  checkMqttConnection();
  delay(3000);
  subscribeMQTT();
  locate_Sensors();
  show_sensor_address();
  pump_on();
  lamp_off();  
}

void loop() {
   if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WIFi Connected");
    if (checkMqttConnection()){
      Serial.println("MQTT Connected");
      client.loop();
      second++;
      if(second == 300 ){
        second = 0;
        sensors.requestTemperatures();
        temp = sensors.getTempC(sensor1);
        String temperature = String(temp);
        char payload[temperature.length()+1];
        temperature.toCharArray(payload, temperature.length()+1); 
        client.publish(constant_device::TempTopic, payload , true);
      }
      if(lamp_mode == 1){
        autoModeLamp();
      }
      if(pump_mode == 1){
        autoModePump();
      }  
      delay(1000);
    }else{
      Serial.println("MQTT not Connected");
      pump_on();
      lamp_off();
    }
  }else{
    Serial.println("Wifi not Connected");
    pump_on();
    lamp_off();
  }
}