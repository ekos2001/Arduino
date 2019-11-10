#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SoftEasyTransfer.h>
#include <SoftwareSerial.h>

#define WLAN_SSID       "..."
#define WLAN_PASS       "..."

int d1 = 5;
int d2 = 4;
SoftwareSerial mySerial(d1, d2);
SoftEasyTransfer ET;

typedef struct {
  int power7, power6, Vrms;
  unsigned long pulseCount;
} PayloadTX;     // create structure - a neat way of packaging data for RF comms
PayloadTX emontx;

ESP8266WebServer server(80);
int blue = 2;
int sda = 0;//GPIO0 is D3
int scl = 4;//GPIO4 is D2


WiFiClient client;                      // Create an ESP8266 WiFiClient class to connect to the MQTT server.
char domain[] = "emoncms.org";
//float power6;
//float power7;
String webResponse;
unsigned long counter = 0;
bool calibrate = true;

void setup()
{
  Serial.begin(9600);
  Serial.print(F("Connecting to "));
  Serial.println(WLAN_SSID);
  pinMode(blue, OUTPUT);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(blue, LOW);
    delay(250);
    digitalWrite(blue, HIGH);
    delay(250);
    Serial.print(F("."));
    counter++;
    if (counter > 50)
    {
      Serial.println("REBOOT");
      ESP.restart();
    }
  }
  Serial.println();

  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());

  setupOTA();

  mySerial.begin(9600);
  //start the library, pass in the data details and the name of the serial port.
  ET.begin(details(emontx), &mySerial);

  server.on("/", handle_root);
  server.on("/data", handle_data);
  server.begin();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("REBOOT");
    ESP.restart();
  }

  ArduinoOTA.handle();
  server.handleClient();

  if (ET.receiveData()) {
    counter++;
    Serial.print(counter);
    Serial.print(": emontx.power7 = '");
    Serial.print(emontx.power7);
    Serial.print("' ");
    Serial.print("emontx.power6 = '");
    Serial.print(emontx.power6);
    Serial.print("' ");
    Serial.print("emontx.Vrms = '");
    Serial.print(emontx.Vrms);
    Serial.print("' ");
    Serial.print("emontx.pulseCount = '");
    Serial.print(emontx.pulseCount);
    Serial.print("'");
    Serial.println();
    digitalWrite(blue, LOW);
    delay(50);
    digitalWrite(blue, HIGH);

    if (calibrate)
    {
      delay(1);
      return;
    }

    Serial.println("connect to Server...");
    if (client.connect(domain, 80))
    {
      digitalWrite(blue, LOW);
      Serial.println("connected");
      //      client.print("GET / emoncms / input / post.json ? apikey = a24ab9dffa26a22e1f6d50c289b11afa&node = 20 & csv = ");
      //      client.print(u);
      //      client.print(", ");
      //      client.print(i);
      client.print("GET /emoncms/input/post.json?apikey=a24ab9dffa26a22e1f6d50c289b11afa&json={new_power6:");
      client.print(emontx.power6);
      client.print(",new_power7:");
      client.print(emontx.power7);
      client.print(",new_power6_7:");
      client.print(emontx.power6 + emontx.power7);
      client.print("}");
      client.println();
      char c;
      long old = millis();
      while ( (millis() - old) < 500 ) // 500ms timeout for 'ok' answer from server
      {
        while (client.available())
        {
          c = client.read();
          Serial.write(c);
        }
      }
      client.stop();
      Serial.println("\nclosed");
      digitalWrite(blue, HIGH);
    }
    else
    {
      Serial.println("Failed to connect");
      digitalWrite(blue, HIGH);
      delay(500);
      digitalWrite(blue, LOW);
      delay(500);
      digitalWrite(blue, HIGH);
      delay(500);
      digitalWrite(blue, LOW);
    }
  }
  delay(100);
}

void setupOTA()
{
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("WeMos");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress : % u % % \r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[ % u] : ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void handle_root()
{
  static String responseHTML = F(""
                                 "<html>                                      "
                                 "  <head>                                    "
                                 "    <script type = \"text/javascript\" src=\"https://ajax.googleapis.com/ajax/libs/jquery/2.2.2/jquery.min.js\"></script>"
                                 "    <title>Current Monitoring</title>               "
                                 "  </head>                                   "
                                 "  <body>                                    "
                                 "<script type=\"text/javascript\">"
                                 "$(document).ready(function(){"
                                 " setInterval(function() {"
                                 "  $.ajax(\"/data\", {"
                                 "    cache: false,"
                                 "    success: function(data) {"
                                 "      $(\"#msgid\").html(data);"
                                 "    },"
                                 "    error: function() {"
                                 "      $(\"#msgid\").html(\"Error\");"
                                 "    }"
                                 "  });"
                                 "}, 1000);"
                                 "});"
                                 "</script>"
                                 "<div id=\"msgid\"/>"
                                 "</body>"
                                 "</html>");
  server.send (200, "text/html", responseHTML.c_str() );
}

void handle_data()
{
  String p7_msg = ": power7: ";
  String p6_msg = " power6: ";
  String Vrms_msg = " Vrms: ";
  String pulse_msg = " pulseCount: ";
  server.send(200, "text/plain", counter + p7_msg + emontx.power7 + p6_msg + emontx.power6 + Vrms_msg + emontx.Vrms + pulse_msg + emontx.pulseCount);
}

