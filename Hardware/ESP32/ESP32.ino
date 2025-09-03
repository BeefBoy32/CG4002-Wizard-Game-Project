#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include <Button.h>
#include <queue>

// MPU6050
MPU6050 mpu;
#define ACCEL_SENS 16384.0
#define MPU_INT_PIN D4  // using GPIO36 (VP)
volatile bool mpuInterrupt = false;
uint16_t packetSize;
uint8_t fifoBuffer[64];
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
  

  // Initialize MPU6050
  Wire.begin();
  // Cheap board used, testConnection will not work, but still able to receive correct data
  if (mpu.testConnection()) Serial.println("MPU6050 connection successful");

  uint8_t devStatus = mpu.dmpInitialize();
  if (devStatus == 0) {
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
    mpu.PrintActiveOffsets();
    mpu.setDMPEnabled(true);
    dmpReady = true;
    packetSize = mpu.dmpGetFIFOPacketSize();
  } else {
    dmpReady = false;
    Serial.print("DMP Initialization failed (code ");
    Serial.print(devStatus);
    Serial.println(")");
  }
  attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), dmpDataReady, RISING);
}

void loop() {
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
    mpu.setDMPEnabled(false);  // disables DMP and stops interrupts
    /* TODO 
     *  Calculate coordinates and send data through BLE
     *  Wait for Response of type of spell
     *  Flash LED light
     */
    
    // Get fused orientation
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
        float x_acc = accel.x / ACCEL_SENS;
        float y_acc = accel.y / ACCEL_SENS;
        float z_acc = accel.z / ACCEL_SENS;

        float x_racc = accelReal.x / ACCEL_SENS;
        float y_racc = accelReal.y / ACCEL_SENS;
        float z_racc = accelReal.z / ACCEL_SENS;
        
        // Output YPR + accelerometer
        Serial.print("YPR: ");
        Serial.print(ypr[0] * 180 / M_PI);
        Serial.print(", ");
        Serial.print(ypr[1] * 180 / M_PI);
        Serial.print(", ");
        Serial.print(ypr[2] * 180 / M_PI);
        Serial.print(" | ");
    
        Serial.print("Acc: ");
        Serial.print(x_acc);
        Serial.print(", ");
        Serial.print(y_acc);
        Serial.print(", ");
        Serial.println(z_acc);

        Serial.print("Real Acc: ");
        Serial.print(x_racc);
        Serial.print(", ");
        Serial.print(y_racc);
        Serial.print(", ");
        Serial.println(z_racc);
    }
    Serial.println("Releasing Button");
  }

  /* TODO
   *  Keep on receiving data from MPU and send data through BLE to Cemputer
   *  Detect for spin/thrust
   *  When spin, LED gets brighter
   *  When thrust, LED disappears (Spell casted)
   *  Restart Loop
   */
}
