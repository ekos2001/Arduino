#include "EmonLib.h"                   // Include Emon Library
#include <SoftEasyTransfer.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(4, 5);
SoftEasyTransfer ET;

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
const byte TIME_BETWEEN_READINGS = 10;            //Time between readings
volatile byte pulseCount = 0;
unsigned long pulsetime = 0;                                  // Record time of interrupt pulse

//
bool calibrate = true;
//
void setup()
{
  Serial.begin(9600);
  mySerial.begin(9600);
  //start the library, pass in the data details and the name of the serial port.
  ET.begin(details(emontx), &mySerial);
  pinMode(pulse_count_pin, INPUT_PULLUP);                     // Set emonTx V3.4 interrupt pulse counting pin as input (Dig 3 / INT1)
  emontx.pulseCount = 0;                                      // Make sure pulse count starts at zero

  pinMode(13, OUTPUT);

  ct7.current(7, Ical7);             // CT ADC channel 7, calibration.
  ct6.current(6, Ical6);             // CT ADC channel 6, calibration.

  ct7.voltage(0, Vcal, phase_shift);          // ADC pin, Calibration, phase_shift
  ct6.voltage(0, Vcal, phase_shift);          // ADC pin, Calibration, phase_shift

  attachInterrupt(digitalPinToInterrupt(pulse_count_pin), onPulse, FALLING);     // Attach pulse counting interrupt pulse counting
}

void loop()
{
  unsigned long start = millis();
  if (READVCC_CALIBRATION_CONST != 1092980L)
  {
    Serial.println("Wrong READVCC_CALIBRATION_CONST, should be 1092980L");
    delay(100);
    return;
  }
  if (calibrate)
  {
    //1. Calibrate voltage
    //New calibration = existing calibration × (correct reading ÷ emonTx reading)
    ct7.calcVI(no_of_half_wavelengths, timeout); emontx.power7 = ct7.Vrms * 100;
    ct6.calcVI(no_of_half_wavelengths, timeout); emontx.power6 = ct6.Vrms * 100;

    //2. Calibrate current
    //ct7.calcVI(no_of_half_wavelengths, timeout); emontx.power7 = ct7.Irms * 100;
    //ct6.calcVI(no_of_half_wavelengths, timeout); emontx.power6 = ct6.Irms * 100;

    //3. Calibrate phase
    //ct7.calcVI(no_of_half_wavelengths, timeout); emontx.power7 = ct7.powerFactor * 100;
    //ct6.calcVI(no_of_half_wavelengths, timeout); emontx.power6 = ct6.powerFactor * 100;

    //ct7.serialprint();
  }
  else
  {
    ct7.calcVI(no_of_half_wavelengths, timeout); emontx.power7 = ct7.realPower;
    ct6.calcVI(no_of_half_wavelengths, timeout); emontx.power6 = ct6.realPower;
    emontx.Vrms = ct6.Vrms * 100;
  }
  
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
  Serial.print("emontx.pulseCount='");
  Serial.print(emontx.pulseCount);
  Serial.print("'");
  Serial.println();

  ET.sendData();
  digitalWrite(13, HIGH);
  delay(100);
  digitalWrite(13, LOW);

  if (!calibrate)
  {
      unsigned long runtime = millis() - start;
      unsigned long sleeptime = (TIME_BETWEEN_READINGS * 1000) - runtime - 100;
      delay(sleeptime);
      //delay(8000);
  }
}

// The interrupt routine - runs each time a falling edge of a pulse is detected
void onPulse()
{
  if ( (millis() - pulsetime) > min_pulsewidth) {
    pulseCount++;          //calculate wh elapsed from time between pulses
  }
  pulsetime = millis();
}

void CalcVref()
{
  float Vs = 5.18;
  long Vref;
  long RefConstant;

  //Vref = readVcc();
  //RefConstant = (long)(Vs * Vref * 1000);
  //Serial.println(RefConstant);
  //emontx.Vrms = RefConstant;
  emontx.Vrms = ct7.readVcc();
  Serial.println(emontx.Vrms);
  ET.sendData();
  digitalWrite(13, HIGH);
  delay(100);
  digitalWrite(13, LOW);
}

long readVcc(void) {
  // Not quite the same as the emonLib version
  long result;

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328__) || defined (__AVR_ATmega328P__)
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  ADCSRB &= ~_BV(MUX5);   // Without this the function always returns -1 on the ATmega2560 http://openenergymonitor.org/emon/node/2253#comment-11432
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#endif

  delay(2);          // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);        // Convert
  while (bit_is_set(ADCSRA, ADSC));
  result = ADCL;
  result |= ADCH << 8;
  // return the raw count  - that's more useful here
  return result;
}

