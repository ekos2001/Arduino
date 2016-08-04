#include "EmonLib.h"                   // Include Emon Library
#include "I2C_Anything.h"
#include <Wire.h>

#define I2CAddressESPWifi 6

typedef struct {
  long power7, power6, Vrms;
  unsigned long pulseCount;
} PayloadTX;     // create structure - a neat way of packaging data for RF comms
PayloadTX emontx;

//const int NUM_MONITORS = 8;
double irms = 0;
//EnergyMonitor monitors[NUM_MONITORS];
EnergyMonitor ct7, ct6;
const float Ical7 =                59.3;              // (2000 turns / 22 Ohm burden) = 90.9; my is 32 Ohm, so 2000/32 = 62
const float Ical6 =                59.3;
const float Vcal =                 79.5;
const int no_of_half_wavelengths = 30;
const int timeout =                2000;                              //emonLib timeout
const float phase_shift =          1.7;
const byte pulse_count_pin =        3;
const byte min_pulsewidth = 110;   // minimum width of interrupt pulse (default pulse output meters = 100ms)
volatile byte pulseCount = 0;
unsigned long pulsetime = 0;                                  // Record time of interrupt pulse

void setup()
{
  Serial.begin(9600);
  pinMode(pulse_count_pin, INPUT_PULLUP);                     // Set emonTx V3.4 interrupt pulse counting pin as input (Dig 3 / INT1)
  emontx.pulseCount = 0;                                      // Make sure pulse count starts at zero

  /*
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
  */

  ct7.current(7, Ical7);             // CT ADC channel 7, calibration.
  ct6.current(6, Ical6);             // CT ADC channel 6, calibration.

  ct7.voltage(0, Vcal, phase_shift);          // ADC pin, Calibration, phase_shift
  ct6.voltage(0, Vcal, phase_shift);          // ADC pin, Calibration, phase_shift

  attachInterrupt(digitalPinToInterrupt(pulse_count_pin), onPulse, FALLING);     // Attach pulse counting interrupt pulse counting

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
  unsigned long startTime = millis();
  long c;
  Serial.print("Received[");
  I2C_readAnything(c);
  Serial.print(c);
  Serial.println("]");
  //calc response.
  /*
    if (c < 0)
    irms  = monitors[0].readVcc();
    else
    irms = monitors[c].calcIrms(1480);
  */

  ct7.calcVI(no_of_half_wavelengths, timeout); emontx.power7 = ct7.realPower;
  ct6.calcVI(no_of_half_wavelengths, timeout); emontx.power6 = ct6.realPower;
  emontx.Vrms = ct6.Vrms * 100;

  if (pulseCount)                 // if the ISR has counted some pulses, update the total count
  {
    cli();                        // Disable interrupt just in case pulse comes in while we are updating the count
    emontx.pulseCount += pulseCount;
    pulseCount = 0;
    sei();                        // Re-enable interrupts
  }

  Serial.print("emontx.power7='");
  Serial.print(emontx.power7);
  Serial.print("' ");
  Serial.print("emontx.power6='");
  Serial.print(emontx.power6);
  Serial.print("' ");
  Serial.print("emontx.Vrms='");
  Serial.print(emontx.Vrms);
  Serial.print("' ");
  Serial.print("Done in '");
  Serial.print(millis() - startTime);
  Serial.print("' msec");
  Serial.println();
}

void espWifiRequestEvent()
{
  I2C_singleWriteAnything(emontx);
}

// The interrupt routine - runs each time a falling edge of a pulse is detected
void onPulse()
{
  if ( (millis() - pulsetime) > min_pulsewidth) {
    pulseCount++;          //calculate wh elapsed from time between pulses
  }
  pulsetime = millis();
}

