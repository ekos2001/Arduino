#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Ticker.h>
#include "I2C_Anything.h"

#define WLAN_SSID       "linksys2"
#define WLAN_PASS       "s0ftc0mputer"
#define I2CAddressESPWifi 6
const byte TIME_BETWEEN_READINGS = 10;            //Time between readings  
const float Vcal_USA = 130.0;                             //Calibration for US AC-AC adapter 77DA-10-09
ESP8266WebServer server(80);
int blue = 2;
WiFiClient client;                      // Create an ESP8266 WiFiClient class to connect to the MQTT server.
char domain[] = "emoncms.org";
float power6;
float power7;
String webResponse;
Ticker ticker;


void setup()
{
  Serial.begin(9600);
  Wire.begin(0,4);//Change to Wire.begin() for non ESP.
  Serial.print(F("Connecting to "));
  Serial.println(WLAN_SSID);
  pinMode(blue, OUTPUT);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
	  digitalWrite(blue, HIGH);
	  delay(250);
	  digitalWrite(blue, LOW);
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

  server.on("/", handle_root);
  server.begin();
  ticker.attach(1.0, handleClient);
}

void loop()
{
	//unsigned long start = millis();
	if (WiFi.status() != WL_CONNECTED)
	{
		Serial.println("REBOOT");
		ESP.restart();
	}

	ArduinoOTA.handle();
	//server.handleClient();

	power6 = getPower(6);
	power7 = getPower(7);



	Serial.println("connect to Server...");
	if (client.connect(domain, 80))
	{
		digitalWrite(blue, LOW);
		Serial.println("connected");
		/*
		client.print("GET /emoncms/input/post.json?apikey=a24ab9dffa26a22e1f6d50c289b11afa&node=20&csv=");
		client.print(u);
		client.print(",");
		client.print(i);
		*/
		client.print("GET /emoncms/input/post.json?apikey=a24ab9dffa26a22e1f6d50c289b11afa&json={new_power6:");
		client.print(power6);
		client.print(",new_power7:");
		client.print(power7);
		client.print(",new_power6_7:");
		client.print(power6 + power7);
		client.print("}");
		client.println();
		char c;
		long old = millis();
		while( (millis()-old)<500 )    // 500ms timeout for 'ok' answer from server
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
	//unsigned long runtime = millis() - start;
	//unsigned long sleeptime = (TIME_BETWEEN_READINGS * 1000) - runtime - 100;
	//delay(sleeptime);
	delay(8000);
}

float getPower(int pin)
{
	Wire.beginTransmission(I2CAddressESPWifi);
	I2C_writeAnything(pin);
	Wire.endTransmission();
	delay(500);//Wait for Slave to calculate response.

	Wire.requestFrom(I2CAddressESPWifi, 4);
	Serial.print("Requested ");
	Serial.print(pin);
	Serial.print(". Return:[");

	float irms;
	I2C_readAnything(irms);

	float power = irms*130.0;
	if (power < 30) {
		power = 0;
	}

	Serial.print(irms);
	Serial.print("A ");
	Serial.print(power);
	Serial.print("W ");
	Serial.println("]");

	return power;
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
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
		else if (error == OTA_END_ERROR) Serial.println("End Failed");
	});
	ArduinoOTA.begin();
	Serial.println("OTA Ready");
}

void handleClient()
{
  server.handleClient();
}

void handle_root()
{
  webResponse = String(power6) + " " + String(power7);
	server.send(200, "text/plain", webResponse);
	delay(100);
}
