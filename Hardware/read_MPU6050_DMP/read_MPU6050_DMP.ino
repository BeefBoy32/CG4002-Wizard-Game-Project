#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include <Button.h>

MPU6050 mpu;

#define ACCEL_SENS 16384.0

Button wandButton(D7, 50);
bool dmpReady = false;
uint16_t packetSize;
uint16_t fifoCount;
uint8_t fifoBuffer[64];

Quaternion q;  // [w, x, y, z]
VectorFloat gravity;
float ypr[3];  // [yaw, pitch, roll]

VectorInt16 accel;
VectorInt16 accelReal;
VectorInt16 accelWorld;

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

void setup() {
  Wire.begin();
  Serial.begin(115200);
  wandButton.InitializeButton();
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
  if (wandButton.CheckHold()){
    
    if (!dmpReady) return;
  
    fifoCount = mpu.getFIFOCount();
    if (fifoCount >= packetSize) {
      mpu.getFIFOBytes(fifoBuffer, packetSize);
  
      // Get fused orientation
      mpu.dmpGetQuaternion(&q, fifoBuffer);
      mpu.dmpGetGravity(&gravity, &q);
      mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
  
      mpu.dmpGetAccel(&accel, fifoBuffer);                     // raw accel (with gravity), LSB
      float x_acc = accel.x / ACCEL_SENS;
      float y_acc = accel.y / ACCEL_SENS;
      float z_acc = accel.z / ACCEL_SENS;
  
      Serial.print("YPR (deg): ");
      Serial.print(ypr[0] * 180.0 / M_PI); Serial.print(", ");
      Serial.print(ypr[1] * 180.0 / M_PI); Serial.print(", ");
      Serial.println(ypr[2] * 180.0 / M_PI);
  
      Serial.print("Acc (g): ");
      Serial.print(x_acc, 3);
      Serial.print(", ");
      Serial.print(y_acc, 3);
      Serial.print(", ");
      Serial.println(z_acc, 3);
      
      mpu.dmpGetLinearAccel(&accelReal , &accel, &gravity);             // gravity removed, body frame (LSB)
      float ax_b = accelReal.x / ACCEL_SENS;
      float ay_b = accelReal.y / ACCEL_SENS;
      float az_b = accelReal.z / ACCEL_SENS;
      Serial.print("Linear Accel BODY (g): ");
      Serial.print(ax_b, 3); Serial.print(", ");
      Serial.print(ay_b, 3); Serial.print(", ");
      Serial.println(az_b, 3);
    
      /*
      getLinearAccelInWorld(&accelWorld, &accelReal, &q);      // rotate to world frame (LSB)
    
      // Convert to m/s^2
      float x_acc = accel.x / ACCEL_SENS;
      float y_acc = accel.y / ACCEL_SENS;
      float z_acc = accel.z / ACCEL_SENS;
      
      float ax_b = (float)accelReal.x / ACCEL_SENS;
      float ay_b = (float)accelReal.y / ACCEL_SENS;
      float az_b = (float)accelReal.z / ACCEL_SENS;
    
      float ax_w = (float)accelWorld.x / ACCEL_SENS;
      float ay_w = (float)accelWorld.y / ACCEL_SENS;
      float az_w = (float)accelWorld.z / ACCEL_SENS;
    
      // Print
      Serial.print("YPR (deg): ");
      Serial.print(ypr[0] * 180.0 / M_PI); Serial.print(", ");
      Serial.print(ypr[1] * 180.0 / M_PI); Serial.print(", ");
      Serial.println(ypr[2] * 180.0 / M_PI);
  
      Serial.print("Acc (g): ");
      Serial.print(x_acc, 3);
      Serial.print(", ");
      Serial.print(y_acc, 3);
      Serial.print(", ");
      Serial.println(z_acc, 3);
    
      Serial.print("Linear Accel BODY (g): ");
      Serial.print(ax_b, 3); Serial.print(", ");
      Serial.print(ay_b, 3); Serial.print(", ");
      Serial.println(az_b, 3);
    
      Serial.print("Linear Accel WORLD (g): ");
      Serial.print(ax_w, 3); Serial.print(", ");
      Serial.print(ay_w, 3); Serial.print(", ");
      Serial.println(az_w, 3);
    
      Serial.println("-----------------------------");
      */
    }
  }
}
