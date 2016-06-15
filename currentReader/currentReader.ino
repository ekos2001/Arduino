#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//#include "EmonLib.h"                   // Include Emon Library

/************************* WiFi Access Point *********************************/

#define WLAN_SSID       "linksys2"
#define WLAN_PASS       "s0ftc0mputer"
#define ADC_BITS    10
#define ADC_COUNTS  (1<<ADC_BITS)
//ADC_COUNTS=1024

//EnergyMonitor emon1;                   // Create an instance
WiFiClient client;                      // Create an ESP8266 WiFiClient class to connect to the MQTT server.
char domain[] = "emoncms.org";
char path[]   = "emoncms";
char apikey[] = "b....................e";
unsigned int inPinI;
double ICAL;
double offsetI;                          //Low-pass filter output
int sampleI;
double filteredI; 
double sqI, sumI, Irms;
int blue = 15;

void setupOTA()
{
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

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

void setup()
{  
  Serial.begin(115200);

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

//  emon1.current(A0, 45);             // Current: input pin, calibration.
  current(A0, 30);
  calcIrms(1480);

/*
  while(1)
  {
    ArduinoOTA.handle();
    delay(10);
  }
  */
}

void loop()
{
  //return;
    ArduinoOTA.handle();
    //delay(10);
    
  if (WiFi.status() != WL_CONNECTED)
  {
      Serial.println("REBOOT");
      ESP.restart();
  }

  char c;
  double Irms = calcIrms(1480);// = emon1.calcIrms(1480);  // Calculate Irms only
  double power = Irms*120.0;
  if (power < 30) {
    power = 0;
  }
  

  Serial.print("ADC:");
  Serial.print(analogRead(A0));
  Serial.print(" W:");
  Serial.print(power);         // Apparent power
  Serial.print(" A:");
  Serial.println(Irms, 3);          // Irms
  //delay(200);
  //return;

      Serial.println("connect to Server...");
      if( client.connect(domain, 80) )
      {
        digitalWrite(blue, HIGH);
        Serial.println("connected");
        /*
        client.print("GET /emoncms/input/post.json?apikey=a24ab9dffa26a22e1f6d50c289b11afa&node=20&csv=");
        client.print(u);
        client.print(",");
        client.print(i);
        */
        client.print("GET /emoncms/input/post.json?apikey=a24ab9dffa26a22e1f6d50c289b11afa&json={power1:");
        client.print(power);
        client.print("}");
        client.println();
        //old = millis();
        //while( (millis()-old)<500 )    // 500ms timeout for 'ok' answer from server
        {
          while( client.available() )
          {  
            c = client.read();
            Serial.write(c);
          }
        }
        client.stop();
        Serial.println("\nclosed");
        digitalWrite(blue, LOW);
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
  delay(10000);
}

void current(unsigned int _inPinI, double _ICAL)
{
   inPinI = _inPinI;
   ICAL = _ICAL;
   offsetI = ADC_COUNTS>>1;
   //Serial.print("setup: offsetI ");
   //Serial.println(offsetI);
}

double calcIrms(unsigned int Number_of_Samples)
{
  int SupplyVoltage = 1000;
  
  for (unsigned int n = 0; n < Number_of_Samples; n++)
  {
    sampleI = analogRead(inPinI);

    // Digital low pass filter extracts the 2.5 V or 1.65 V dc offset, 
  //  then subtract this - signal is now centered on 0 counts.
    offsetI = (offsetI + (sampleI-offsetI)/1024);
   //Serial.print("calc: offsetI ");
   //Serial.println(offsetI);
  filteredI = sampleI - offsetI;

    // Root-mean-square method current
    // 1) square current values
    sqI = filteredI * filteredI;
    // 2) sum 
    sumI += sqI;
    //Serial.printf("%d\n", n);
    //Serial.printf("%d: sample=%d, offset=%d", n, sampleI, offsetI);
  }

  double I_RATIO = ICAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  Irms = I_RATIO * sqrt(sumI / Number_of_Samples); 

  //Reset accumulators
  sumI = 0;
//--------------------------------------------------------------------------------------       
 
  return Irms;
}


