#include "Wire.h"
#include <MPU6050_6Axis_MotionApps20.h>
#include <Button.h>
#include <queue>
#include <WiFi.h>
#include <PubSubClient.h>
#include <LEDControl.h>
#include <ArduinoJson.h>
#include "MAX17043.h"

// GLOBAL VARIABLES
volatile bool drawingMode = true; // When true, send MPU data to Ultra96, else detect if there is spinning and thrusting to cast the spell
bool wandReady = false;
volatile bool otherReady = false;
volatile bool u96Ready = false;
int spinCount = 0;
volatile char spellType = 'U';

// Wi-Fi credentials
#define WAND true // True if Wand 1, False if Wand 2
// MQTT broker (use your laptop IP if running Mosquitto locally)
//Kan Wu

/*
const char* ssid = "OKW32";
const char* password = "151122Kanwu";
const char* mqtt_server = "172.20.10.4"; // replace with your laptop's IP
*/

const char* ssid = "SINGTEL-3FC0";
const char* password = "CmWEhyHqgKp3";
const char* mqtt_server = "192.168.1.12"; // replace with your laptop's IP 192.168.1.12

/*
const char* ssid = "shree";
const char* password = "shreedhee12";
const char* mqtt_server = "172.20.10.2"; // replace with your laptop's IP
*/

const int mqtt_port = 1883;
// 
const char* TOP_STATUS = WAND ? "wand1/status" : "wand2/status";
const char* TOP_BATT = WAND ? "wand1/batt" : "wand2/batt";
const char* TOP_MPU = WAND ? "wand1/mpu" : "wand2/mpu";
const char* TOP_CAST = WAND ? "wand1/cast" : "wand2/cast";
// Subscribe
const char* TOP_OTHER = WAND ? "wand2/status" : "wand1/status";
const char* TOP_U96 = "u96/status";
const char* TOP_SPELL = WAND ? "u96/wand1/spell" : "u96/wand2/spell";

WiFiClient espClient;
PubSubClient client(espClient);

// MPU6050, DMP
MPU6050 mpu;
#define ACCEL_SENS 16384.0
#define G 9.80655
#define MPU_INT_PIN D5  // using GPIO9
volatile bool mpuInterrupt = false;
int mpuCount = 0;
unsigned long previous = 0;

uint16_t packetSize;
uint8_t fifoBuffer[64];
VectorInt16 laBias = {0,0,0};
int N = 500; // number of samples (~0.5s if 1kHz DMP)
struct MpuPacket {
    uint8_t data[64]; // DMP packet size
};
std::queue<MpuPacket> mpuQueue;
bool dmpReady;

// Wand Button
Button myButton(D7, 50);
bool isButtonHeld;
bool isButtonReleased;

//LED 
LEDControl ledControl(D2, D3, D4, 5000, 8);


Colour charToColour(char c) {
  switch(c) {
    case 'I': return PURPLE;
    case 'W': return BLUE;
    case 'C': return RED;
    case 'T': return GREEN;
    case 'Z': return YELLOW;
    case 'S': return CYAN;
    default:  return WHITE;  // default value if unknown
  }
}

int calcStrength(int n) {
  if (n < 50) {
    return 1;
  }
  if (n < 100){
    return 2;
  }
  if (n < 150) {
    return 3;
  }
  if (n < 200) {
    return 4;
  }
  return 5;
}


void setup_wifi()
{
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void publish_batt() {
  float volts = FuelGauge.voltage();
  float pcnt = FuelGauge.percent();
  String js =  String("{\"voltage\":") + String(volts) +  
               ",\"percent\":" + String(pcnt) + "}";
  client.publish(TOP_BATT, js.c_str(), true);
}

void reconnect()
{
  String disconnect_message =  String("{\"ready\":") + String("false") + "}";
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();
  }

  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    if (client.connect(WAND? "WAND1" : "WAND2", NULL, NULL, TOP_STATUS, 1, true, disconnect_message.c_str()))
    {
      Serial.println("connected");
      client.subscribe(TOP_SPELL, 1);
      client.subscribe(TOP_OTHER, 1);
      client.subscribe(TOP_U96, 1);
      String connect_message =  String("{\"ready\":") + String("true") + "}";
      client.publish(TOP_STATUS, connect_message.c_str(), true);
      publish_batt();
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to String
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message received: ");
  Serial.println(message);
  if (strcmp(topic, TOP_SPELL) == 0) { 
    // Parse JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    if (!error) {
      String spellStr = doc["spell_type"];
      spellType = 'U';
      if (spellStr != nullptr && spellStr[0] != '\0') {
        spellType = spellStr[0];
      }
      Serial.print("Spell Type: ");
      Serial.println(spellType);
      drawingMode = false;
      mpu.resetFIFO();
      mpuInterrupt = false;
      ledControl.on_spell_light(charToColour(spellType), calcStrength(spinCount));
    } else {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
    }
  }
  else if (strcmp(topic, TOP_U96) == 0) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    if (!error) {
      u96Ready = doc["ready"].as<bool>();
      const char* state = WAND ? "wand1_state" : "wand2_state";
      drawingMode = doc[state]["drawingMode"].as<bool>();
      String spellStr = doc[state]["spell"];
      spellType = 'U';
      if (spellStr != nullptr && spellStr[0] != '\0') {
        spellType = spellStr[0];
      }
    } else {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
    }
  }
  else if (strcmp(topic, TOP_OTHER) == 0) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    if (!error) {
      otherReady = doc["ready"].as<bool>();
    } else {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
    }
  }
  else {
    Serial.print(topic);
  }
}




void publish_cast_data(int strength) {
  String js =  String("{\"strength\":") + String(strength) +  
               ",\"spell_type\":" + "\"" + String(spellType) + "\"" + "}";
  client.publish(TOP_CAST, js.c_str(), false);
}

void publish_MPU_data(const MpuPacket pkt) {
    Quaternion q;  // [w, x, y, z]
    VectorFloat gravity;
    float ypr[3];  // [yaw, pitch, roll]
    VectorInt16 accel;
    // VectorInt16 accelReal;
    mpu.dmpGetQuaternion(&q, pkt.data);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    mpu.dmpGetAccel(&accel, pkt.data);
    // mpu.dmpGetLinearAccel(&accelReal , &accel, &gravity);
    // Output YPR + accelerometer
    Serial.print("YPR: ");
    Serial.print(ypr[0] * 180 / M_PI);
    Serial.print(", ");
    Serial.print(ypr[1] * 180 / M_PI);
    Serial.print(", ");
    Serial.print(ypr[2] * 180 / M_PI);
    Serial.print(" | ");
    
    Serial.print("Acc: ");
    Serial.print((accel.x - laBias.x) / ACCEL_SENS * G);
    Serial.print(", ");
    Serial.print((accel.y - laBias.y) / ACCEL_SENS * G);
    Serial.print(", ");
    Serial.println((accel.z - laBias.z) / ACCEL_SENS * G);
    String js =  String("{\"ts\":") + String(millis()) +
                ",\"yaw\":"   + String(ypr[0] * 180 / M_PI) +
                ",\"pitch\":" + String(ypr[1] * 180 / M_PI) +
                ",\"roll\":" + String(ypr[2] * 180 / M_PI) +
                ",\"accelx\":" + String((accel.x - laBias.x) / ACCEL_SENS * G) +
                ",\"accely\":" + String((accel.y - laBias.y) / ACCEL_SENS * G) +
                ",\"accelz\":" + String((accel.z - laBias.z) / ACCEL_SENS * G) +
                "}";  
    client.publish(TOP_MPU, js.c_str(), false);
}

void IRAM_ATTR dmpDataReady() {
    mpuInterrupt = true;
}

void setup() {
  Serial.begin(115200);

  ledControl.initializeLED();
  delay(2000);
  ledControl.on_initialize_light();
  
  myButton.InitializeButton();
  Serial.println("Button initialized");

  Wire.begin();
  //Initialize Fuel Gauge
  if (!FuelGauge.begin(&Wire)) {
    Serial.println("MAX17043 NOT found.\n");
    while (true) {}
  }
  FuelGauge.reset();
  delay(250);
  FuelGauge.quickstart();
  delay(125);
  
  // Initialize MPU6050
  // Cheap board used, testConnection will not work, but still able to receive correct data
  if (mpu.testConnection()) Serial.println("MPU6050 connection successful");
 
  uint8_t devStatus = mpu.dmpInitialize();
  delay(2000); // Let mgyro and accelerometer reading stabilise
  if (devStatus == 0) {
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
    mpu.PrintActiveOffsets(); 
    packetSize = mpu.dmpGetFIFOPacketSize();
  } else {
    dmpReady = false;
    Serial.print("DMP Initialization failed (code ");
    Serial.print(devStatus);
    Serial.println(")");
  }
  Serial.println("Callibrate raw accel data");
  
  attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), dmpDataReady, RISING);
 
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  if (!client.connected()) reconnect();
  mpu.setDMPEnabled(true);
  delay(100);
  mpu.resetFIFO();
  mpuInterrupt = false;
  ledControl.off_light();
}

void loop() {
  // Ensure ESP32 connected to Laptop and wifi
  if (!client.connected()) {
    ledControl.on_initialize_light();
    mpu.setDMPEnabled(false);
    mpu.resetFIFO();
    reconnect();
    mpu.setDMPEnabled(true);
    delay(100);
    mpu.resetFIFO();
    mpuInterrupt = false;
    mpuCount = 0;
    ledControl.off_light();
  }
  client.loop();

  //Pause here if other wand disconnect or U96 disconnect
  wandReady = otherReady && u96Ready;
  if(!wandReady) {
    ledControl.on_initialize_light();
    mpu.setDMPEnabled(false);
    mpu.resetFIFO();
    while (!wandReady) {
      wandReady = otherReady && u96Ready;
      client.loop();
    }
    mpu.setDMPEnabled(true);
    delay(100);
    mpu.resetFIFO();
    mpuInterrupt = false;
    mpuCount = 0;
    ledControl.off_light();
  }

  if (drawingMode && mpuInterrupt) {
      mpuInterrupt = false;
      uint16_t fifoCount = mpu.getFIFOCount();
      if (fifoCount >= packetSize) {
        MpuPacket pkt;
        mpu.getFIFOBytes(pkt.data, packetSize);
        if (mpuCount == 5) {
          publish_MPU_data(pkt);
          mpuCount = 0;
        } else {
          mpuCount += 1;
        }
      }
      /* TODO: Have a function that runs when ESP32 receive data
       *  1.RESET FIFO
       *  2.Change drawingMode to False
       *  3.Flash LED light based on spell casted
       */
  }
  
  else if (!drawingMode && mpuInterrupt){
    mpuInterrupt = false;
    uint16_t fifoCount = mpu.getFIFOCount();
    if (fifoCount >= packetSize) {
      MpuPacket pkt;
      mpu.getFIFOBytes(pkt.data, packetSize);
      Quaternion q;  // [w, x, y, z]
      VectorFloat gravity;
      float ypr[3];  // [yaw, pitch, roll]
      VectorInt16 accel;
      VectorInt16 accelReal;
      mpu.dmpGetQuaternion(&q, pkt.data);
      mpu.dmpGetGravity(&gravity, &q);
      mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
      mpu.dmpGetAccel(&accel, pkt.data);
      mpu.dmpGetLinearAccel(&accelReal , &accel, &gravity);
      if ((fabs(accelReal.x / ACCEL_SENS) >= 0.65) || (fabs(accelReal.z / ACCEL_SENS) >= 0.65)) {
        Serial.println(spinCount);
        spinCount += 1;
        ledControl.on_spell_light(charToColour(spellType), calcStrength(spinCount));
      }
      if (accelReal.y / ACCEL_SENS <= -0.65 && spinCount >= 5) {
        int strength = calcStrength(spinCount);
        Serial.print("Thrust detected, Strength: ");
        Serial.println(strength);
        drawingMode = true;
        spinCount = 0;
        ledControl.off_light();
        publish_cast_data(strength);
        mpu.resetFIFO();
      }
    }
  }

  unsigned long currentTime = millis();
  if (currentTime - previous >= 30000){
    publish_batt();
    previous = currentTime;
  }
}
