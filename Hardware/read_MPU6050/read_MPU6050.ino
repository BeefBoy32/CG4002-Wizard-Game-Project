#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"

MPU6050 mpu;

bool dmpReady = false;
uint16_t packetSize;
uint16_t fifoCount;
uint8_t fifoBuffer[64];

Quaternion q;  // [w, x, y, z]
VectorFloat gravity;
float ypr[3];  // [yaw, pitch, roll]

VectorInt16 accel;


void setup() {
  Wire.begin();
  Serial.begin(115200);

  mpu.initialize();
  
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
    Serial.print("DMP Initialization failed (code ");
    Serial.print(devStatus);
    Serial.println(")");
  }
}

void loop() {
  if (!dmpReady) return;

  fifoCount = mpu.getFIFOCount();
  if (fifoCount >= packetSize) {
    mpu.getFIFOBytes(fifoBuffer, packetSize);

    // Get fused orientation
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

    mpu.dmpGetAccel(&accel, fifoBuffer);

    float x_acc = accel.x / 16384.0;
    float y_acc = accel.y / 16384.0;
    float z_acc = accel.z / 16384.0;

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
  }
}
