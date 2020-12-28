#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>

byte mac[6];
char json[200];
bool blink = 0;
int fade = 0;
bool faded = 0;

// Account data
const char *influx = "";
const char *mqtt_server = "";
const char *ssid = "";
const char *password = "";

// Led config

#define DATA_PIN D4
#define NUM_LEDS 36
CRGB leds[NUM_LEDS];
int ledstate[NUM_LEDS];
/* 
0 = off
1 = solid color
2 = fade
3 = blink
*/
int ledcolorr[NUM_LEDS];
int ledcolorg[NUM_LEDS];
int ledcolorb[NUM_LEDS];

int ledintensity[NUM_LEDS];

void ledrandcolor(int dot, int step = 5);
int fader(int from, int to, int step);

// Clients
WiFiClient client;
PubSubClient mqttclient(client);

void mqttconnect()
{
  // Loop until we're reconnected
  while (!mqttclient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttclient.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttclient.publish("outTopic", "hello world");
      // ... and resubscribe
      mqttclient.subscribe("Anzeige");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  StaticJsonDocument<300> root;
  deserializeJson(root, payload);
  for (int dot = 0; dot < NUM_LEDS; dot++)
  {
    char achar[5];
    itoa(dot, achar, 10);
    if (root.containsKey(achar))
    {
      Serial.print("Update Led ");
      Serial.print(dot);
      Serial.print(" state: ");
      Serial.print(ledstate[dot]);
      Serial.print(" colorr: ");
      Serial.print(ledcolorr[dot]);
      Serial.print(" colorg: ");
      Serial.print(ledcolorb[dot]);
      Serial.print(" colorb: ");
      Serial.print(ledcolorb[dot]);
      Serial.print(" int: ");
      Serial.print(ledintensity[dot]);
      ledstate[dot] = root[achar][0];
      ledcolorr[dot] = root[achar][1];
      ledcolorg[dot] = root[achar][2];
      ledcolorb[dot] = root[achar][3];
      ledintensity[dot] = root[achar][4]; //not used
      Serial.print(" -> ");
      Serial.print(" state: ");
      Serial.print(ledstate[dot]);
      Serial.print(" colorr: ");
      Serial.print(ledcolorr[dot]);
      Serial.print(" colorg: ");
      Serial.print(ledcolorb[dot]);
      Serial.print(" colorb: ");
      Serial.print(ledcolorb[dot]);
      Serial.print(" int: ");
      Serial.print(ledintensity[dot]);
      Serial.println();

      // Disable and enable can be handled directly
      if (ledstate[dot] == 0)
      {
        ledrandcolor(dot,100);
        //leds[dot] = CRGB::Black;
      }
      if (ledstate[dot] >= 1)
      { // Also set effect colors for faster response
        leds[dot].setRGB(ledcolorr[dot], ledcolorg[dot], ledcolorb[dot]);
      }
    }
  }
  FastLED.show();

  Serial.println();
}

void setup()
{
  // put your setup code here, to run once:
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  for (int dot = 0; dot < NUM_LEDS; dot++)
  {
    ledrandcolor(dot, 100);
    //leds[dot] = CRGB::Black;
    ledcolorr[dot] = 0;
    ledcolorg[dot] = 0;
    ledcolorb[dot] = 0;
    ledstate[dot] = 0;
    ledintensity[dot] = 0;
  }
  leds[35] = CRGB::Green;
  FastLED.show();

  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    leds[35] = CRGB::Red;
    FastLED.show();
    delay(5000);
    ESP.restart();
  }
  randomSeed(micros());
  leds[35] = CRGB::Green;
  FastLED.show();
  mqttclient.setServer(mqtt_server, 1883);
  mqttclient.setCallback(callback);
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("led-display");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  WiFi.macAddress(mac);
  ledrandcolor(35, 100);
  FastLED.show();
}

void loop()
{
  EVERY_N_SECONDS(1)
  {
    // state 3 -> blink
    for (int dot = 0; dot < NUM_LEDS; dot++)
    {
      if (ledstate[dot] == 3)
      {
        if (blink == 0)
        {
          leds[dot].fadeToBlackBy(200);
        }
        else
        {
          leds[dot].setRGB(ledcolorr[dot], ledcolorg[dot], ledcolorb[dot]);
        }
      }
    }
    if (blink == 0)
    {
      blink = 1;
    }
    else
    {
      blink = 0;
    }
  }

  EVERY_N_MILLISECONDS(50)
  {
    // state 2 -> fade
    if (faded == 0)
    {
      fade = fade - 2;
    }
    else
    {
      fade = fade + 2;
    }
    for (int dot = 0; dot < NUM_LEDS; dot++)
    {
      if (ledstate[dot] == 2)
      {
        leds[dot].setRGB(ledcolorr[dot], ledcolorg[dot], ledcolorb[dot]);
        leds[dot].fadeToBlackBy(fade);
      }
    }
    if (fade <= 0)
    {
      faded = 1;
    }
    else if (fade >= 200)
    {
      faded = 0;
    }
  }

  
  EVERY_N_SECONDS(5)
  {
    // Get some slight color change when led is not used..
    int randled = random(0, NUM_LEDS);
    if (ledstate[randled] == 0)
    {
      ledrandcolor(randled, 25);
    }
  }

  FastLED.show();

  ArduinoOTA.handle();

  if (!mqttclient.connected())
  {
    mqttconnect();
  }
  else
  {
    //  Serial.println("Still connected");
  }
  if (!mqttclient.loop())
  {
    mqttconnect();
  }
}

void ledrandcolor(int dot, int step)
{
  leds[dot].setRGB(0, fader(leds[dot].green, random(8, 255), step), fader(leds[dot].blue, random(8, 255), step));
}

int fader(int from, int to, int step)
{
  int ret;
  if (from < to)
  {
    int s = to - from;
    s = scale8_video(s, step);
    ret = from + s;
  }
  else
  {
    int s = from - to;
    s = scale8_video(s, step);
    ret = from - s;
  }
  return ret;
}
