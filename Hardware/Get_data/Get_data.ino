#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include <Button.h>
#include <queue>
#include <WiFi.h>
#include <PubSubClient.h>

//Wifi Variables
const char *ssid = "shree";
const char *password = "shreedhee12";

const char *mqtt_server = "172.20.10.5";
const int mqtt_port = 1884; // <-- match your mosquitto

const char *TOP_CMD = "wand/cmd";
const char *TOP_IMU7 = "wand/imu7";
const char *TOP_CAST = "wand/cast";
const char *TOP_STATUS = "wand/status";

volatile bool ARMED = false;
volatile uint8_t SPELL_ID = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(400);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected, IP=" + WiFi.localIP().toString());
}

void on_cmd(char *topic, byte *payload, unsigned int len)
{
    // Copy payload to String and print it so we always see what arrived
    String s;
    s.reserve(len);
    for (unsigned i = 0; i < len; i++)
        s += (char)payload[i];
    Serial.print("[/cmd] ");
    Serial.println(s);

    // Find "spell_id" (quoted or not), then parse the next integer
    int k = s.indexOf("spell_id");
    int spell = -1;
    if (k >= 0)
    {
        // find the ':' after spell_id
        int c = s.indexOf(':', k);
        if (c >= 0)
        {
            // advance to first digit or minus sign
            int i = c + 1;
            while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t'))
                i++;
            bool neg = (i < (int)s.length() && s[i] == '-');
            if (neg)
                i++;
            // accumulate digits
            int val = 0, start = i;
            while (i < (int)s.length() && isdigit((unsigned char)s[i]))
            {
                val = val * 10 + (s[i] - '0');
                i++;
            }
            if (i > start)
                spell = neg ? -val : val;
        }
    }

    if (spell >= 0)
    {
        Serial.print("=> spell_id = ");
        Serial.println(spell);
        // TODO: store it & arm state machine if you want:
        SPELL_ID = (uint8_t)spell;
        ARMED = true;
        Serial.println("ARMED");
    }
    else
    {
        Serial.println("=> couldn't parse spell_id");
    }
}

void publish_imu7_demo()
{
    // flags: drawing_mode=1 (bit1), more-data=1 (bit0) -> 0b00000011
    uint8_t b[7];
    b[0] = (1 << 1) | (1 << 0);
    int16_t ax = 100, ay = 200, az = 300; // demo numbers for now
    b[1] = ax & 0xFF;
    b[2] = (ax >> 8) & 0xFF;
    b[3] = ay & 0xFF;
    b[4] = (ay >> 8) & 0xFF;
    b[5] = az & 0xFF;
    b[6] = (az >> 8) & 0xFF;
    client.publish(TOP_IMU7, b, 7, false);
}

void publish_cast(uint8_t strength, bool thrust)
{
    static uint16_t seq = 0;
    seq++;
    String js = String("{\"ts\":") + String(millis()) +
                ",\"seq\":" + String(seq) +
                ",\"spell_id\":" + String(SPELL_ID) +
                ",\"strength\":" + String(strength) +
                ",\"thrust\":" + (thrust ? "true" : "false") +
                ",\"flags\":{\"armed\":" + String(ARMED ? "true" : "false") + "}}";
    client.publish(TOP_CAST, js.c_str(), true);
}

void reconnect()
{
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32Client",
                           nullptr, nullptr,    // no user/pass
                           TOP_STATUS, 0, true, // Last Will (topic, QoS, retain)
                           "{\"state\":\"offline\"}"))
        { // LWT payload
            Serial.println("connected");
            client.subscribe(TOP_CMD, 1);
            client.publish(TOP_STATUS, "{\"state\":\"online\"}", true); // retained
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" retry in 2s");
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

void publish_MPU_data(MpuPacket pkt) {
    Quaternion q;  // [w, x, y, z]
    VectorFloat gravity;
    float ypr[3];  // [yaw, pitch, roll]
    VectorInt16 accel;
    mpu.dmpGetQuaternion(&q, pkt.data);
    mpu.dmpGetGravity(&gravity, &q);
    mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    mpu.dmpGetAccel(&accel, pkt.data);
    String js = ",\"yaw\":" + String(ypr[0] * 180 / M_PI) +
                ",\"pitch\":" + String(ypr[1] * 180 / M_PI) +
                ",\"roll\":" + String(ypr[2] * 180 / M_PI) +
                ",\"accelx\":" + String(accel.x / ACCEL_SENS) +
                ",\"accely\":" + String(accel.y / ACCEL_SENS) +
                ",\"accelz\":" + String(accel.z / ACCEL_SENS) +
                ",\"flags\":{\"armed\":" + String(drawingMode) + "}}";  
    client.publish(TOP_CAST, js.c_str(), true);
}

void send_MPU_data() {
  while (!mpuQueue.empty()) {
    MpuPacket pkt = mpuQueue.front();
    mpuQueue.pop();
    publish_MPU_data(pkt);
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

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(on_cmd);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
  
  isButtonHeld = myButton.CheckHold();
  
  if (myButton.IsInitialHold()){
    Serial.println("Holding Button");

    // Enable DMP Interrupt to constantly get DMP readings
    mpu.resetFIFO();
    mpu.setDMPEnabled(true);   // enables DMP and interrupt generation
    while (mpuQueue.size() < 300) {
      if (mpuInterrupt) {
        mpuInterrupt = false;
        uint16_t fifoCount = mpu.getFIFOCount();
        if (fifoCount >= packetSize) {
          MpuPacket pkt;
          mpu.getFIFOBytes(pkt.data, packetSize);
          mpuQueue.push(pkt);
        }
      }
    }
    // Disable interrupt to stop getting DMP readings
    mpuInterrupt = false;
    mpu.setDMPEnabled(false);  // disables DMP and stops interrupts
    /* TODO 
     *  Calculate coordinates and send data through BLE
     *  Wait for Response of type of spell
     *  Flash LED light
     */
    send_MPU_data();
    // Send fused orientation
    // send_MPU_data();
    Serial.println("Releasing Button");
    mpu.resetFIFO();
    mpu.setDMPEnabled(true);     
  }
}
