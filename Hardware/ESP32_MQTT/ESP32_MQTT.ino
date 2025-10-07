#include "Wire.h"
#include <MPU6050_6Axis_MotionApps20.h>
#include <Button.h>
#include <queue>
#include <WiFi.h>
#include <PubSubClient.h>
#include <LEDControl.h>
#include <ArduinoJson.h>

// GLOBAL VARIABLES
volatile bool drawingMode = true; // When true, send MPU data to Ultra96, else detect if there is spinning and thrusting to cast the spell
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
const char* mqtt_server = "172.20.10.5"; // replace with your laptop's IP
*/
const int mqtt_port = 1883;
const char* TOP_MPU = "wand/mpu";
const char* TOP_CAST = "wand/cast";
const char* TOP_SPELL = WAND ? "wand1/spell" : "wand2/spell";

WiFiClient espClient;
PubSubClient client(espClient);

// MPU6050, DMP
MPU6050 mpu;
#define ACCEL_SENS 16384.0
#define G 9.80655
#define MPU_INT_PIN D5  // using GPIO9
volatile bool mpuInterrupt = false;
int mpuCount = 0;

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

void reconnect()
{
  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    if (client.connect(WAND? "WAND1" : "WAND2"))
    {
      Serial.println("connected");
      client.subscribe(TOP_SPELL);
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
  if (strcmp(topic, TOP_SPELL) == 0) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    Serial.print("Message received: ");
    Serial.println(message);
  
    // Parse JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    if (!error) {
      const char* spellStr = doc["spell"];
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
}

void publish_cast_data(int strength) {
  String js =  String("{\"strength\":") + String(strength) +  
               ",\"spell_type\":" + "\"" + String(spellType) + "\"" +
               ",\"wand_id\":" + String(WAND ? 1 : 2) +
               ",\"topic\":" + "\"" + TOP_CAST + "\"" + "}";
  client.publish(TOP_CAST, js.c_str(), true);
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
                ",\"topic\":" + "\"" + TOP_MPU + "\"" +
                ",\"wand_id\":" + String(WAND? 1 : 2) +
                ",\"yaw\":"   + String(ypr[0] * 180 / M_PI) +
                ",\"pitch\":" + String(ypr[1] * 180 / M_PI) +
                ",\"roll\":" + String(ypr[2] * 180 / M_PI) +
                ",\"accelx\":" + String((accel.x - laBias.x) / ACCEL_SENS * G) +
                ",\"accely\":" + String((accel.y - laBias.y) / ACCEL_SENS * G) +
                ",\"accelz\":" + String((accel.z - laBias.z) / ACCEL_SENS * G) +
                "}";  
    client.publish(TOP_MPU, js.c_str(), true);
}

void send_MPU_data() {
  while (!mpuQueue.empty()) {
    MpuPacket pkt = mpuQueue.front();
    mpuQueue.pop();
    publish_MPU_data(pkt);
  }
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
  
  // Initialize MPU6050
  Wire.begin();
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
  /*
  mpu.setDMPEnabled(true);
  long real_accelx = 0;
  long real_accely = 0;
  long real_accelz = 0;
  for (int i = 0; i < N; i += 0) {
    if (mpuInterrupt) {
      mpuInterrupt = false;
      uint16_t fifoCount = mpu.getFIFOCount();
      if (fifoCount >= packetSize) {
        Serial.println(i);
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
        real_accelx += (long) accelReal.x;
        real_accely += (long) accelReal.y;
        real_accelz += (long) accelReal.z;
        i += 1;
      }
    }
  }
  mpu.setDMPEnabled(false);
  // Compute average bias
  laBias.x = (int16_t) (real_accelx / N);
  laBias.y = (int16_t) (real_accely / N);
  laBias.z = (int16_t) (real_accelz / N);
  Serial.println("Callibrate linear accel data");
  */
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
  // Ensure ESP32 connected to Laptop
  if (!client.connected()) {
    mpu.setDMPEnabled(false);
    mpu.resetFIFO();
    mpuInterrupt = false;
    reconnect();
    mpu.setDMPEnabled(true);
  }
  client.loop();

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
}
