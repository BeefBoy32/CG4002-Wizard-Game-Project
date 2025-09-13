#include <WiFi.h>
#include <PubSubClient.h>

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

void setup()
{
    Serial.begin(115200);
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(on_cmd);
}

void loop()
{
    if (!client.connected())
        reconnect();
    client.loop();

    // Optional: heartbeat so you see something in the monitor
    static unsigned long last = 0;
    if (millis() - last > 3000)
    {
        last = millis();
        Serial.println("alive...");
    }
    // --- DEMO imu7 every 2s ---
    // static unsigned long lastImu = 0;
    // if (millis() - lastImu > 2000) {
    //   lastImu = millis();
    //   publish_imu7_demo();
    // }

    // --- If ARMED, simulate spin strength then one thrust:true ---
    static unsigned long lastCast = 0;
    static uint8_t strength = 0;

    if (ARMED && millis() - lastCast > 500)
    {
        lastCast = millis();
        if (strength < 5)
            strength++;                // climb 1..5
        bool thrust = (strength == 4); // one-shot at 4 for demo
        publish_cast(strength, thrust);
        if (thrust)
        { // reset after casting
            ARMED = false;
            strength = 0;
            Serial.println("CAST SENT, disarmed");
        }
    }
}
