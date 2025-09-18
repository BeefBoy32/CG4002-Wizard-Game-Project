#include "Wire.h"
#include <MPU6050_6Axis_MotionApps20.h>
#include <Button.h>
#include <queue>
#include <WiFi.h>
#include <PubSubClient.h>
#include <LEDControl.h>


// GLOBAL VARIABLES
bool drawingMode = true; // When true, send MPU data to Ultra96, else detect if there is spinning and thrusting to cast the spell
int spinCount = 0;

// Wi-Fi credentials
#define WAND true // True if Wand 1, False if Wand 2
// MQTT broker (use your laptop IP if running Mosquitto locally)
//Kan Wu
const char* ssid = "OKW32";
const char* password = "151122Kanwu";
const char* mqtt_server = "172.20.10.4"; // replace with your laptop's IP
/*
const char* ssid = "shree";
const char* password = "shreedhee12";
const char* mqtt_server = "172.20.10.5"; // replace with your laptop's IP
*/
const int mqtt_port = 1883;
const char* TOP_MPU = "wand/mpu";
const char* TOP_STATUS = "wand/status";
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

int calcStrength(int n) {
  if (n < 5) {
    return 1;
  }
  if (n < 10){
    return 2;
  }
  if (n < 15) {
    return 3;
  }
  if (n < 20) {
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
    if (client.connect("ESP32Client"))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void getLinearAccelInWorld(VectorInt16 *v, VectorInt16 *vReal, Quaternion *q) {
    float ax = (float)vReal->x;
    float ay = (float)vReal->y;
    float az = (float)vReal->z;

    float w = q->w;
    float x = q->x;
    float y = q->y;
    float z = q->z;

    // Rotation matrix multiply
    v->x = (int16_t)(
        ax * (1 - 2*y*y - 2*z*z) +
        ay * (2*x*y - 2*w*z) +
        az * (2*x*z + 2*w*y)
    );

    v->y = (int16_t)(
        ax * (2*x*y + 2*w*z) +
        ay * (1 - 2*x*x - 2*z*z) +
        az * (2*y*z - 2*w*x)
    );

    v->z = (int16_t)(
        ax * (2*x*z - 2*w*y) +
        ay * (2*y*z + 2*w*x) +
        az * (1 - 2*x*x - 2*y*y)
    );
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
                ",\"wand_id\":" + String(WAND) +
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
    MpuPacket pkt;
    while (fifoCount >= packetSize) {
      mpu.getFIFOBytes(pkt.data, packetSize);
      fifoCount -= packetSize;
    }
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
    if (((accelReal.x / ACCEL_SENS) >= 0.65) || ((accelReal.z / ACCEL_SENS) >= 0.65)) {
      Serial.println(spinCount);
      spinCount += 1;
    }
    if (accelReal.y / ACCEL_SENS >= 0.65 && spinCount >= 5) {
      int strength = calcStrength(spinCount);
      Serial.print("Thrust detected, Strength: ");
      Serial.println(strength);
      mpu.setDMPEnabled(false);
      drawingMode = true;
      spinCount = 0;
    }
  } 
}
