#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include <Button.h>
#include <queue>
#include <WiFi.h>
#include <PubSubClient.h>


// GLOBAL VARIABLES
bool drawingMode = true; // When true, send MPU data to Ultra96, else detect if there is spinning and thrusting to cast the spell
int spinCount = 0;
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


// Wi-Fi credentials
const char* ssid = "OKW32";
const char* password = "151122Kanwu";
#define WAND true
// MQTT broker (use your laptop IP if running Mosquitto locally)
const char* mqtt_server = "172.20.10.4"; // replace with your laptop's IP
WiFiClient espClient;
PubSubClient client(espClient);
void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

// MPU6050, DMP
MPU6050 mpu;
#define ACCEL_SENS 16384.0
#define MPU_INT_PIN D5  // using GPIO9
volatile bool mpuInterrupt = false;
uint16_t packetSize;
uint8_t fifoBuffer[64];
VectorInt16 laBias = {0,0,0};
int N = 500; // number of samples (~0.5s if 1kHz DMP)
struct MpuPacket {
    uint8_t data[64]; // DMP packet size
};
std::queue<MpuPacket> mpuQueue;
bool dmpReady;
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

void sendVector(VectorInt16 v, bool endBit, bool drawingMode) {
    uint8_t buffer[7];
    /* 1st byte 5 bits used
     * bit 0: Only for when drawingMode 0 means end bit
     * bit 1: Drawing mode (1) or Casting mode (0)
     * bit 2: Which wand it comes from (wand1: 0, wand2: 1)
      */
    buffer[0] = ((WAND << 2) | (drawingMode << 1) | !endBit) & 0xFF;
    
    buffer[1] = v.x & 0xFF;
    buffer[2] = (v.x >> 8) & 0xFF;

    buffer[3] = v.y & 0xFF;
    buffer[4] = (v.y >> 8) & 0xFF;

    buffer[5] = v.z & 0xFF;
    buffer[6] = (v.z >> 8) & 0xFF;

    client.publish("esp32/test", buffer, 7);
}

void send_MPU_data() {
  while (!mpuQueue.empty()) {
    MpuPacket pkt = mpuQueue.front();
    mpuQueue.pop();
    Quaternion q;  // [w, x, y, z]
    VectorFloat gravity;
    float ypr[3];  // [yaw, pitch, roll]
    VectorInt16 accel;
    VectorInt16 accelReal;
    VectorInt16 accelWorld;
    mpu.dmpGetQuaternion(&q, pkt.data);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    mpu.dmpGetAccel(&accel, pkt.data);
    mpu.dmpGetLinearAccel(&accelReal , &accel, &gravity);
    accelReal.x -= laBias.x;
    accelReal.y -= laBias.y;
    accelReal.z -= laBias.z;
    getLinearAccelInWorld(&accelWorld, &accelReal, &q);
    sendVector(accelWorld, mpuQueue.empty(), true);
  }
}

void read_MPU_data() {
  while (!mpuQueue.empty()) {
    MpuPacket pkt = mpuQueue.front();
    mpuQueue.pop();
    Quaternion q;  // [w, x, y, z]
    VectorFloat gravity;
    float ypr[3];  // [yaw, pitch, roll]
    VectorInt16 accel;
    VectorInt16 accelReal;
    VectorInt16 accelWorld;
    mpu.dmpGetQuaternion(&q, pkt.data);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    mpu.dmpGetAccel(&accel, pkt.data);
    mpu.dmpGetLinearAccel(&accelReal , &accel, &gravity);
    accelReal.x -= laBias.x;
    accelReal.y -= laBias.y;
    accelReal.z -= laBias.z;
    getLinearAccelInWorld(&accelWorld, &accelReal, &q);
    
    float x_acc = accel.x / ACCEL_SENS;
    float y_acc = accel.y / ACCEL_SENS;
    float z_acc = accel.z / ACCEL_SENS;
    
    float x_racc = accelReal.x / ACCEL_SENS;
    float y_racc = accelReal.y / ACCEL_SENS;
    float z_racc = accelReal.z / ACCEL_SENS;
    
    float x_wacc = accelWorld.x / ACCEL_SENS;
    float y_wacc = accelWorld.y / ACCEL_SENS;
    float z_wacc = accelWorld.z / ACCEL_SENS;
    
    
    // Output YPR + accelerometer
    Serial.print("YPR: ");
    Serial.print(ypr[0] * 180 / M_PI);
    Serial.print(", ");
    Serial.print(ypr[1] * 180 / M_PI);
    Serial.print(", ");
    Serial.print(ypr[2] * 180 / M_PI);
    Serial.print(" | ");
    
    Serial.print("Acc: ");
    Serial.print(x_acc, 5);
    Serial.print(", ");
    Serial.print(y_acc, 5);
    Serial.print(", ");
    Serial.println(z_acc, 5);
    
    Serial.print("Real Acc: ");
    Serial.print(x_racc);
    Serial.print(", ");
    Serial.print(y_racc);
    Serial.print(", ");
    Serial.println(z_racc);
    

    /*
    Serial.print("World Acc: ");
    Serial.print(x_wacc);
    Serial.print(", ");
    Serial.print(y_wacc);
    Serial.print(", ");
    Serial.println(z_wacc);
    */
  }
}

void IRAM_ATTR dmpDataReady() {
    mpuInterrupt = true;
}

// Wand Button
Button myButton(D7, 50);
bool isButtonHeld;
bool isButtonReleased;

void setup() {
  Serial.begin(115200);
  myButton.InitializeButton();

  /*
  // Wifi
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  */
  
  // Initialize MPU6050
  Wire.begin();
  // Cheap board used, testConnection will not work, but still able to receive correct data
  if (mpu.testConnection()) Serial.println("MPU6050 connection successful");

  delay(2000); // Let mgyro and accelerometer reading stabilise
  uint8_t devStatus = mpu.dmpInitialize();
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
  attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), dmpDataReady, RISING);
  mpu.setDMPEnabled(true);
  long real_accelx = 0;
  long real_accely = 0;
  long real_accelz = 0;
  for (int i = 0; i < N; i += 0) {
    if (mpuInterrupt) {
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
        real_accelx += (long) accelReal.x;
        real_accely += (long) accelReal.y;
        real_accelz += (long) accelReal.z;
        i++;
      }
    }
  }
  mpu.setDMPEnabled(false);
  // Compute average bias
  laBias.x = (int16_t) (real_accelx / N);
  laBias.y = (int16_t) (real_accely / N);
  laBias.z = (int16_t) (real_accelz / N);
  mpu.setDMPEnabled(true);
}

void loop() {
  if (drawingMode) {
    /*
    if (!client.connected()) {
      reconnect();
    }
    
    client.loop();
    */
    isButtonHeld = myButton.CheckHold();
    
    if (myButton.IsInitialHold()){
      Serial.println("Holding Button");
  
      // Enable DMP Interrupt to constantly get DMP readings
      mpu.resetFIFO();
      mpu.setDMPEnabled(true);   // enables DMP and interrupt generation
      do {
        isButtonHeld = myButton.CheckHold();
        isButtonReleased = myButton.CheckRelease();
        if (mpuInterrupt) {
          mpuInterrupt = false;
          uint16_t fifoCount = mpu.getFIFOCount();
          if (fifoCount >= packetSize) {
            MpuPacket pkt;
            mpu.getFIFOBytes(pkt.data, packetSize);
            mpuQueue.push(pkt);
          }
        }
      } while (!myButton.IsInitialRelease());
      // Disable interrupt to stop getting DMP readings
      mpuInterrupt = false;
      mpu.setDMPEnabled(false);  // disables DMP and stops interrupts
      /* TODO 
       *  Calculate coordinates and send data through BLE
       *  Wait for Response of type of spell
       *  Flash LED light
       */
  
  
      read_MPU_data();
      // Send fused orientation
      // send_MPU_data();
      Serial.println("Releasing Button");
      drawingMode = false;
      mpu.resetFIFO();
      mpu.setDMPEnabled(true);     
    }
  } else{
    if (mpuInterrupt) {
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
  } 
  /* TODO
   *  Keep on receiving data from MPU and send data through BLE to Cemputer
   *  Detect for spin/thrust
   *  When spin, LED gets brighter
   *  When thrust, LED disappears (Spell casted)
   *  Restart Loop
   */
}
