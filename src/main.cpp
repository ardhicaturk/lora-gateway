#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>  
#include "SSD1306.h" 
#include <LoRa.h>
#include <SD.h> 
#include <WiFi.h>
#include <ArduinoJson.h>
#include <DS3231.h>
#include <EEPROM.h>

#define countof(a) (sizeof(a) / sizeof(a[0]))
#define SCK     5    // GPIO5  -- SX1278's SCK
#define MISO    19   // GPIO19 -- SX1278's MISO
#define MOSI    27   // GPIO27 -- SX1278's MOSI
#define SS      18   // GPIO18 -- SX1278's CS
#define RST     14   // GPIO14 -- SX1278's RESET
#define DI0     26   // GPIO26 -- SX1278's IRQ(Interrupt Request)
#define BAND 433E6


DS3231 Rtc;
SSD1306 display(0x3c,4,15);
String rssi = "RSSI --";
String packSize = "--";
unsigned long timeout;
unsigned long time1;
bool bTimeout;
String packet ;
char ssidAP[50] = "ESP32-AP";
char passwordAP[50] = "10101010";
uint32_t tanggal;

struct Config {
  float v1;
  float v2;
  float a1;
  float a2;
};
Config config;

typedef struct{
    float v1;
    float a1;
    float v2;
    float a2;
} dta;

typedef struct{
  int counter;
  float sum;
} eepromSimpan;
dta data;
void loadJson(const char *filename) {
  File file = SD.open(filename);
  DynamicJsonBuffer jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(file);
  if (!root.success())
    Serial.println(F("Failed to read file, using default configuration"));
  // config.port = root["port"];
  // strcpy(config.hostname,root["hostname"]);          // <- destination's capacity

  file.close();
}
void saveJson(String filename, const Config &config) {
  File file;
  if(!SD.exists(filename)){
    file = SD.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println(F("Failed to create file"));
      return;
    } 
    DynamicJsonBuffer jsonBuffer(512);
    JsonObject &root = jsonBuffer.createObject();
    JsonArray& v1 = root.createNestedArray("v1");
    JsonArray& v2 = root.createNestedArray("v2");
    JsonArray& a1 = root.createNestedArray("a1");
    JsonArray& a2 = root.createNestedArray("a2");
    v1.add(config.v1);
    v2.add(config.v2);
    a1.add(config.a1);
    a2.add(config.a2);

    if (root.printTo(file) == 0) {
      Serial.println(F("Failed to write to file"));
    }
    file.close();
  } else {
    file = SD.open(filename);
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(file);
    JsonObject &root2 = jsonBuffer.createObject();
    JsonArray& v1 = root2.createNestedArray("v1");
    JsonArray& v2 = root2.createNestedArray("v2");
    JsonArray& a1 = root2.createNestedArray("a1");
    JsonArray& a2 = root2.createNestedArray("a2");
    for (int i = 0; i < root["v1"].size(); i++){
      v1.add(root["v1"][i]);
      v2.add(root["v2"][i]);
      a1.add(root["a1"][i]);
      a2.add(root["a2"][i]);
    } 
    v1.add(config.v1);
    v2.add(config.v2);
    a1.add(config.a1);
    a2.add(config.a2);
    File file2 = SD.open(filename, FILE_WRITE);
    if (root2.printTo(file2) == 0) {
      Serial.println(F("Failed to write to file"));
    }
    file.close();
    file2.close();
  }

}

void printFile(String filename) {
  File file = SD.open(filename);
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();
  file.close();
}
void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}


void cbk(int packetSize) {
  packet ="";
  packSize = String(packetSize,DEC);
  LoRa.readBytes((uint8_t *)&data, packetSize);
  rssi = "RSSI " + String(LoRa.packetRssi(), DEC) ;
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, rssi);
  display.drawString(0, 12, "Voltage1: ");
  display.drawString(50, 12, String(data.v1) + 'V');
  display.drawString(0, 24, "Current1: ");
  display.drawString(50, 24, String(data.a1) + 'A');
  display.drawString(0, 36, "Voltage2: ");
  display.drawString(50, 36, String(data.v2) + 'V');
  display.drawString(0, 48, "Current2: ");
  display.drawString(50, 48, String(data.a2) + 'A');
  display.display();
  
}

void setup() {
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high、
  
  Serial.begin(115200);
// INIT LORA ===========================================>
  Serial.println();
  Serial.println("LoRa Receiver");
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI0);  
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  //LoRa.onReceive(cbk);
  LoRa.receive();
  Serial.println("init ok");
  display.init();
  display.flipScreenVertically();  
  display.setFont(ArialMT_Plain_10);

// INIT RTC ===========================================>
  Rtc.begin();
  Rtc.setAlarm2(0, 0, 0, DS3231_EVERY_MINUTE);
  Rtc.setAlarm1(0, 0, 1, 0,DS3231_MATCH_M_S);
// INIT WIFI AP ===========================================>
  WiFi.mode(WIFI_OFF);
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssidAP, passwordAP);
  Serial.println(WiFi.softAPIP());  

// INIT SD CARD ===========================================>
  File root;
  if (!SD.begin(23)) {
      Serial.println("initialization failed!");
      while (1);
  }
  Serial.println("initialization done.");
  root = SD.open("/");
  printDirectory(root, 0);
  //loadJson(filename, config);
  String a;
  a = "/";
  a += String(12082018);
  a += ".txt";
  config.v1 = 223.7;
  config.v2 = 218.9;
  config.a1 = 3.5;
  config.a2 = 1.1;
  //saveJson(a, config);
  Serial.println(F("Print config file..."));
  printFile("/1.txt");

  //EEPROM ===========================================>
  if (!EEPROM.begin(128))
  {
    Serial.println("failed to initialise EEPROM");
  }
  eepromSimpan sim;
  sim.counter = 0;
  sim.sum = 0;
  EEPROM.put(0,sim);
  delay(1000);
}
float buffer;
int counter;
void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) { 
    cbk(packetSize);  
    bTimeout = false;
    counter++;
    buffer+=data.v1;
    timeout = millis();
    }   else { 
      if(millis() - timeout > 5000){
        timeout = millis();
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_10);
        display.drawString(0 , 0 , "Disconnected");
        display.display();
      }
    }
  if (Rtc.isAlarm2())
  {
    Serial.print("Rata-rata per-menit : ");
    float z = buffer/counter;
    Serial.println(z);
    buffer = 0;
    counter = 0;
    eepromSimpan sim;
    EEPROM.get(0, sim);
    int counter2 = sim.counter;
    counter2++;
    sim.counter = counter2;
    float buff2 = sim.sum;
    buff2+=z;
    sim.sum = buff2;
    EEPROM.put(0, sim);
  }
  if (Rtc.isAlarm1()){
    eepromSimpan sim;
    EEPROM.get(0, sim);
    float buff = sim.sum/sim.counter;
    sim.sum = 0;
    sim.counter = 0;
    EEPROM.put(0,sim);
    Config cfg;
    cfg.v1 = buff;
    cfg.v2 = buff;
    cfg.a1 = 0.2;
    cfg.a2 = 0.4;
    saveJson("/1.txt",cfg);
    printFile("/1.txt");
  }
  // if (millis() - time1 > 3000){
  //   time1 = millis();
  //   File root;
  //   Serial.println("initialization done.");
  //   root = SD.open("/");
  //   printDirectory(root, 0);
  //   Serial.println("done!");
  // }
}
