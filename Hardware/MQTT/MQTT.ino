#include <WiFi.h>
#include <PubSubClient.h>

// Wi-Fi credentials
// const char *ssid = "OKW32";
// const char *password = "151122Kanwu";

const char *ssid = "shree";
const char *password = "shreedhee12";

// MQTT broker (use your laptop IP if running Mosquitto locally)
// const char *mqtt_server = "172.20.10.4"; // replace with your laptop's IP
const char *mqtt_server = "172.20.10.5"; // replace with your laptop's IP

const char *TOP_CMD = "wand/cmd";

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi()
{
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
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

void on_cmd(char *topic, byte *payload, unsigned int len)
{
  // print raw
  Serial.print("[/cmd] ");
  for (unsigned i = 0; i < len; i++)
    Serial.print((char)payload[i]);
  Serial.println();

  // extract spell_id (super light parse)
  String s;
  s.reserve(len);
  for (unsigned i = 0; i < len; i++)
    s += (char)payload[i];
  int k = s.indexOf("\"spell_id\"");
  if (k >= 0)
  {
    int c = s.indexOf(':', k);
    if (c >= 0)
    {
      int e = s.indexOf(',', c);
      if (e < 0)
        e = s.indexOf('}', c);
      int spell = s.substring(c + 1, e).toInt();
      Serial.print("=> spell_id = ");
      Serial.println(spell);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(on_cmd);
  // after you successfully connect:
  client.subscribe(TOP_CMD, 1);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  // publish message every 5 sec
  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000)
  {
    lastMsg = millis();
    String payload = "Hello from ESP32!";
    client.publish("esp32/test", payload.c_str());
    Serial.println("Published: " + payload);
  }
}
