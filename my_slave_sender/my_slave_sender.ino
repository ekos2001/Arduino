#include "EmonLib.h"                   // Include Emon Library
#include "I2C_Anything.h"
#include <Wire.h>

#define I2CAddressESPWifi 6
const int NUM_MONITORS = 8;

double irms = 0;
EnergyMonitor monitors[NUM_MONITORS];

void setup()
{
  Serial.begin(9600);

  for (int i = 0; i < NUM_MONITORS; i++)
  {
    monitors[i].current(i, 59.3);
  }

  for (int i = 0; i < NUM_MONITORS; i++)
  {
    for (int k = 0; k < 5; k++)
    {
      monitors[i].calcIrms(1480);
    }
  }

  Wire.begin(I2CAddressESPWifi);
  Wire.onReceive(espWifiReceiveEvent);
  Wire.onRequest(espWifiRequestEvent);
}

void loop()
{
  delay(1);
	/*
	Serial.print(monitors[6].calcIrms(1480));
	Serial.print(" ");
	Serial.println(monitors[7].calcIrms(1480));
	*/
}

void espWifiReceiveEvent(int count)
{
  long c;
  Serial.print("Received[");
  I2C_readAnything(c);
  Serial.print(c);
  Serial.println("]");
  //calc response.
  if (c < 0)
    irms  = monitors[0].readVcc();
  else
    irms = monitors[c].calcIrms(1480);
  Serial.print("IRMS='");
  Serial.print(irms);
  Serial.println("'");
}

void espWifiRequestEvent()
{
  I2C_singleWriteAnything(irms);
}

