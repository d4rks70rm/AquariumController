#include <ESP8266WiFi.h> 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Define pin numbers
#define ONE_WIRE_BUS D4
#define PumpRelay D2
#define LampRelay D3
#define Temp D4

//Wireless Info
const char* ssid = "<SSID>";
const char* password = "<PASSWORD>"; 

//MQTT Info
char* LampTopic = "<LIGHT_TOPIC>";
char* PumpTopic = "<PUMP_TOPIC>";
char* TempTopic = "<TEMP_TOPIC>";
const char* server = "<SERVER_NAME>";
String clientName = "<CLIENT_NAME>";
char message_buff[100]; 

//NTP Info
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


int temp=0;
int lamp_mode = 0; //0-auto 1-manual
int pump_mode = 0; //0-auto 1-manual
//Define a OneWire object to communicate
OneWire oneWire(ONE_WIRE_BUS);


DallasTemperature sensors(&oneWire);
DeviceAddress sensor1;


void show_sensor_address(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

//Pump Control
void pump_off(){
  digitalWrite(PumpRelay, LOW);
}

void pump_on(){
  digitalWrite(PumpRelay, HIGH);
}
//Lamp Control
void lamp_off(){
  digitalWrite(LampRelay, LOW);
}

void lamp_on(){
  digitalWrite(LampRelay, HIGH);
}

//CallBack MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  int i = 0;

  Serial.println("Message arrived:  topic: " + String(topic));
  Serial.println("Length: " + String(length,DEC));
  
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  
  String msgString = String(message_buff);
  
  Serial.println("Payload: " + msgString);
  Serial.println(msgString);

  if (strcmp(topic,LampTopic)==0) {
    if(msgString == "off"){
      lamp_off();
      lamp_mode = 1;
    }
    else if(msgString == "on"){
      lamp_on();
      lamp_mode = 1;      
    }
    if (msgString == "autolamp"){
      Serial.println("auto"); 
      lamp_mode = 0;          
    }  
  }  

  if (strcmp(topic,PumpTopic)==0) {
    if(msgString == "off"){
      pump_off();
      pump_mode = 1;
    }
    else if(msgString == "on"){
      pump_on();
      pump_mode = 1;      
    }  
    if (msgString == "autopump"){
      Serial.println("auto"); 
      pump_mode = 0;    
    }   
  } 
}

//Inicialization
WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

void setup() {
  pinMode(PumpRelay,OUTPUT);
  pinMode(LampRelay,OUTPUT);  
  pinMode(Temp,INPUT);  
  Serial.begin(115200);
  sensors.begin();
  timeClient.begin();

  //Wireless Connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //MQTT Connection
  if (client.connect((char*) clientName.c_str())) {
    Serial.println("connected: ");
    client.subscribe(PumpTopic);   
    client.subscribe(LampTopic);   
  }  

  //Locating sensors
  Serial.println("Locatting sensors...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" sensors.");
  if (!sensors.getAddress(sensor1, 0)) 
     Serial.println("Sensors not found!"); 
  Serial.print("Sensor address: ");
  show_sensor_address(sensor1);  
}

void autoModePump(){
  timeClient.update();
  if( (timeClient.getHours() >= 9 ) && (timeClient.getHours() < 23) ){
    pump_on();
  }
  else{
    pump_off();
  }

}

void autoModeLamp(){
  timeClient.update();
  if( (timeClient.getHours() >= 12 ) && (timeClient.getHours() < 17) ){
    lamp_on();
  }
  else{
    lamp_off();
  }

}



void loop() {
  client.loop();
  sensors.requestTemperatures();
  temp = sensors.getTempC(sensor1);
  String temperature = String(temp);
  char payload[temperature.length()+1];
  temperature.toCharArray(payload, temperature.length()+1); 
  client.publish( TempTopic, payload );
  if(lamp_mode == 0){
    autoModeLamp();
  }
  if(pump_mode == 0){
    autoModePump();
  }  
  delay(1000);
}
