#include "Wire.h"
#include <MPU6050_6Axis_MotionApps20.h>
#include <Button.h>
#include <queue>
#include <WiFi.h>
#include <LEDControl.h>
#include <ArduinoJson.h>
#include "MAX17043.h"
#include <espMqttClient.h>

// GLOBAL VARIABLES
volatile bool drawingMode = true; // When true, send MPU data to Ultra96, else detect if there is spinning and thrusting to cast the spell
volatile bool transitionMode = false;
bool wandReady = false;
volatile bool otherReady = false;
volatile bool u96Ready = false;
int spinCount = 0;
volatile char spellType = 'U';

// Wi-Fi credentials
#define WAND true // True if Wand 1, False if Wand 2
//Kan Wu

/*
const char* ssid = "OKW32";
const char* password = "151122Kanwu";
const char* mqtt_server = "172.20.10.4"; // replace with your laptop's IP
*/

/*
const char* ssid = "SINGTEL-3FC0";
const char* password = "CmWEhyHqgKp3";
const char* mqtt_server = "192.168.1.12"; // replace with your laptop's IP 192.168.1.12
*/

const char* ssid = "shree"; 
const char* password = "shreedhee12";
const char* mqtt_server = "172.20.10.5"; // replace with your laptop's IP


const int mqtt_port = 1883;
const char* WAND_CLIENT = WAND ? "wand1-client" : "wand2-client";
espMqttClient mqttClient;

// 
const char* TOP_STATUS = WAND ? "wand1/status" : "wand2/status";
const char* TOP_BATT = WAND ? "wand1/batt" : "wand2/batt";
const char* TOP_MPU = WAND ? "wand1/mpu" : "wand2/mpu";
const char* TOP_CAST = WAND ? "wand1/cast" : "wand2/cast";

// Subscribe
const char* TOP_OTHER = WAND ? "wand2/status" : "wand1/status";
const char* TOP_U96 = "u96/status";
const char* TOP_SPELL = WAND ? "u96/wand1/spell" : "u96/wand2/spell";

// MPU6050, DMP
MPU6050 mpu;
#define ACCEL_SENS 16384.0
#define G 9.80655
#define MPU_INT_PIN D7  // using GPIO9
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
  mqttClient.publish(TOP_BATT, 1, true, js.c_str());
}


void onMqttConnect(bool sessionPresent) {
  Serial.println("✅ MQTT Connected!");
  mqttClient.subscribe(TOP_SPELL, 2);
  mqttClient.subscribe(TOP_U96, 1);
  mqttClient.subscribe(TOP_OTHER, 1);
  String connect_message =  String("{\"ready\":") + String("true") + "}";
  mqttClient.publish(TOP_STATUS, 1, true, connect_message.c_str());
  publish_batt();
}

void onMqttMessage(
  const espMqttClientTypes::MessageProperties& properties, 
  const char* topic,
  const uint8_t* payload,
  size_t len,
  size_t index,
  size_t total){
  char msg[len + 1];
  memcpy(msg, payload, len);
  msg[len] = '\0';
  Serial.print("Received on topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  Serial.println(msg);
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (!error) {
    if (strcmp(topic, TOP_SPELL) == 0){
      const char* spellStr = doc["spell_type"];
      spellType = spellStr[0];
      ledControl.on_spell_light(charToColour(spellType), calcStrength(spinCount));
      transitionMode = true;
      drawingMode = false;
    }
    else if (strcmp(topic, TOP_U96) == 0) {
      const char* state = WAND ? "wand1_state" : "wand2_state";
      drawingMode = doc[state]["drawingMode"];
      const char* spellStr = doc[state]["spell"];
      spellType = spellStr[0];
      // Set last to ensure values have been initialised
      u96Ready = doc["ready"];
    }
    else if (strcmp(topic, TOP_OTHER) == 0) {
      otherReady = doc["ready"];
    }
  } else {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
  }
}

void publish_cast_data(int strength) {
  String js =  String("{\"strength\":") + String(strength) +  
               ",\"spell_type\":" + "\"" + String(spellType) + "\"" + "}";
  mqttClient.publish(TOP_CAST, 2, false, js.c_str());
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
    mqttClient.publish(TOP_MPU, 0, false, js.c_str());
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
  mqttClient.setWill(TOP_STATUS, 1, true, (String("{\"ready\":") + String("false") + String("}")).c_str()); // topic, message, retain, QoS
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setClientId(WAND_CLIENT);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.connect();
  
  mpu.setDMPEnabled(true);
  delay(100);
  mpu.resetFIFO();
  mpuInterrupt = false;
  ledControl.off_light();
}

void reconnect() {
  ledControl.on_initialize_light();
  mpu.setDMPEnabled(false);
  mpu.resetFIFO();
  while(!mqttClient.connected()){
    Serial.println("Attempting MQTT connection...");
    if (WiFi.status() != WL_CONNECTED) {
      setup_wifi();
    }
    mqttClient.connect();
    delay(1000);
  };
  mpu.setDMPEnabled(true);
  delay(100);
  mpu.resetFIFO();
  mpuInterrupt = false;
  mpuCount = 0;
  if (drawingMode) {
    ledControl.off_light();
  } else {
    ledControl.on_spell_light(charToColour(spellType), calcStrength(spinCount));
  }
}

void loop() {
  // Ensure ESP32 connected to Laptop and wifi
  if (!mqttClient.connected()) {
    reconnect();
  }

  //Pause here if other wand disconnect or U96 disconnect
  wandReady = otherReady && u96Ready;
  if(!wandReady) {
    ledControl.on_initialize_light();
    mpu.setDMPEnabled(false);
    mpu.resetFIFO();
    while (!wandReady) {
      if (!mqttClient.connected()) {
        reconnect();
      }
      wandReady = otherReady && u96Ready;
    }
    mpu.setDMPEnabled(true);
    delay(100);
    mpu.resetFIFO();
    mpuInterrupt = false;
    mpuCount = 0;
    if (drawingMode) {
      ledControl.off_light();
    } else {
      ledControl.on_spell_light(charToColour(spellType), calcStrength(spinCount));
    }
    
  }
  
  if (transitionMode) {
    mpu.resetFIFO();
    mpuInterrupt = false;
    transitionMode = false;
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
  
  else if (!drawingMode && mpuInterrupt && !transitionMode){
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
      /*
      Serial.print("YPR: ");
      Serial.print(ypr[0] * 180 / M_PI);
      Serial.print(", ");
      Serial.print(ypr[1] * 180 / M_PI);
      Serial.print(", ");
      Serial.print(ypr[2] * 180 / M_PI);
      Serial.print(" | ");
      
      Serial.print("Acc: ");
      Serial.print(accelReal.x / ACCEL_SENS);
      Serial.print(", ");
      Serial.print(accelReal.y / ACCEL_SENS);
      Serial.print(", ");
      Serial.println(accelReal.z / ACCEL_SENS);
      */
      if ((fabs(accelReal.x / ACCEL_SENS) >= 0.65) || (fabs(accelReal.z / ACCEL_SENS) >= 0.65)) {
        Serial.println(spinCount);
        spinCount += 1;
        ledControl.on_spell_light(charToColour(spellType), calcStrength(spinCount));
      }
      if (accelReal.y / ACCEL_SENS <= -0.65 && calcStrength(spinCount) >= 2) {
        int strength = calcStrength(spinCount);
        Serial.print("Thrust detected, Strength: ");
        Serial.println(strength);
        drawingMode = true;
        spinCount = 0;
        ledControl.off_light();
        publish_cast_data(strength);
        mpu.resetFIFO();
        mpuInterrupt = false;
        mpuCount = 0;
      }
    }
  }

  unsigned long currentTime = millis();
  if (currentTime - previous >= 30000){
    publish_batt();
    previous = currentTime;
  }
}
