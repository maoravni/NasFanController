#include <EEPROM.h>

//#include <TimerOne.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include "PID.h"
#include "PID_Autotune_V0.h"

/*TODO:
 * Setup the RFM69 radio
 * Keepalive - when no RF comms are available, stop calculating the PID.
 * Serial Commands:
 *     * KEEPALIVE
 *     * STARTHEATING
 *     * STOPHEATING
 *     * AUTOTUNE
 *     * SETP/SETI/SETD
 * Store PID values in EEPROM
 * PWM for the SSR
 * Setup of the PID Control.
 */

PIDDynamicSampleTime PID;
PID_ATune PIDAutoTune(E_PID_ControlType_PID, 20, 40, 0.1, 120, -120);
bool isInAutotune = false;

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 15
#define FAN_PWM 5
#define TACHO_PIN 6
#define MEASURE_LED 7

#define PID_EEPROM_ADDRESS 0

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

DeviceAddress tempDeviceAddress;

void _analogWrite(int pin, int value)
{
  Serial.println(value);
  analogWrite(pin, value);
}

//#define analogWrite _analogWrite
//int  resolution = 12;
#define resolution 12
unsigned long lastTempRequest = 0;
unsigned long actualTempDelay = 0;
#define delayInMillis (750 / (1 << (12 - resolution)))
float temperature = 0.0;
unsigned char output = 255;
char randomResult = 0;
bool newTempReading = false;

char outgoingMessage[50];
char incomingMessage[50];
String incomingString = "";
char incomingMessageSize = 0;
char bytesRead = 0;
char tachoPin = 0;

void setup(void)
{
  pinMode(MEASURE_LED, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(FAN_PWM, OUTPUT);
  pinMode(TACHO_PIN, OUTPUT);
  delay(2000);
  Serial.begin(115200);
  Serial.setTimeout(1);

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, resolution);

  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  lastTempRequest = millis();

//  Timer1.initialize(10000);
//  Timer1.attachInterrupt(PwmOutputInterrupt); // blinkLED to run every 0.15 seconds
    
  float p, i, d;
  p = -1;
  i = -1;
  d = 0;
  EEPROM.get(PID_EEPROM_ADDRESS, p);
  EEPROM.get(PID_EEPROM_ADDRESS+4, i);
  EEPROM.get(PID_EEPROM_ADDRESS+8, d);
  Serial.print("p: ");
  Serial.println(p);
  Serial.print("i: ");
  Serial.println(i);
  Serial.print("d: ");
  Serial.println(d);
  PID.SetTunings(p, i, d, 0);
  PID.SetOutputLimits(40, 255);
  PID.setSetPoint(40, 0);
  PID.setAutoMode(true);
  PID.setEnabled(true);
  analogWrite(FAN_PWM, 255);
  delay(3000);
}

void PwmOutputInterrupt(void)
{
//  randomResult = random(100);
//  digitalWrite(LED_BUILTIN, (output > randomResult));
//  digitalWrite(FAN_PWM, (output > randomResult));
//  analogWrite(FAN_PWM, output<<2);
}

unsigned long t1, t2;

bool readTemperature()
{
  if ((actualTempDelay = (millis() - lastTempRequest)) >= delayInMillis) // waited long enough??
  {
    t1 = millis();
    digitalWrite(MEASURE_LED, HIGH);
    temperature = sensors.getTempCByIndex(0);

    sensors.requestTemperatures();
    lastTempRequest = millis();

    digitalWrite(MEASURE_LED, LOW);

    t2 = millis();
    return true;
  }
  return false;
}

void handleCommands()
{
  float parameter;
  bytesRead = Serial.readBytesUntil('\n', incomingMessage, 50);
  if (bytesRead > 0)
  {
    //Serial.println("handleCommands()");
    //incomingMessageSize += bytesRead;
    incomingString += incomingMessage;
    Serial.print("incoming: ");
    Serial.print(bytesRead, DEC);
    Serial.println(incomingMessage);
    if (incomingMessage[bytesRead-1] == '\n' || incomingMessage[bytesRead-1] == '\r')
    {
      // start parsing commands:

      //keep alive
      if (incomingString.startsWith("KEEPALIVE"))
      {
        Serial.println("Alive");
      }
      else if (incomingString.startsWith("SETP"))
      {
        Serial.println("SETP");
        parameter = incomingString.substring(5).toFloat();
        EEPROM.put(PID_EEPROM_ADDRESS, parameter);
        Serial.println(parameter);
      }
      else if (incomingString.startsWith("SETI"))
      {
        Serial.println("SETI");
        parameter = incomingString.substring(5).toFloat();
        EEPROM.put(PID_EEPROM_ADDRESS+4, parameter);
        Serial.println(parameter);
      }
      else if (incomingString.startsWith("SETD"))
      {
        Serial.println("SETD");
        parameter = incomingString.substring(5).toFloat();
        EEPROM.put(PID_EEPROM_ADDRESS+8, parameter);
        Serial.println(parameter);
      }
      else if (incomingString.startsWith("STARTHEATING"))
      {
        Serial.println("STARTHEATING");
        parameter = incomingString.substring(12).toFloat();
        Serial.println(parameter);
        isInAutotune = false;
        PID.setSetPoint(parameter, 0);
        PID.setAutoMode(true);
        PID.setEnabled(true);
      }
      else if (incomingString.startsWith("STOP"))
      {
        Serial.println("STOP");
        PID.setEnabled(false);
        PID.setAutoMode(false);
        isInAutotune = false;
      }
      else if (incomingString.startsWith("AUTOTUNE"))
      {
        Serial.println("AUTOTUNE");
        parameter = incomingString.substring(12).toFloat();
//        EEPROM.put(PID_EEPROM_ADDRESS+8, parameter);
        Serial.println(parameter);
        isInAutotune = true;
        PID.setSetPoint(parameter, 0);
        PID.setAutoMode(true);
        PID.setEnabled(true);
      }
      incomingString = "";
    }
    if (incomingMessageSize >= 50)
      incomingMessageSize = 0;
    incomingMessage[0] = 0;
  }
}

void loop() {
  newTempReading = readTemperature();
  if (newTempReading)
  {
    if (isInAutotune)
    {
      output = (unsigned char)PIDAutoTune.Compute(temperature);
      if (!PIDAutoTune.isRunning())
      {
        isInAutotune = false;
        float p, i, d;
        p = PIDAutoTune.GetKp();
        i = PIDAutoTune.GetKi();
        d = PIDAutoTune.GetKd();
        Serial.println(p);
        Serial.println(i);
        Serial.println(d);
        EEPROM.put(PID_EEPROM_ADDRESS, p);
        EEPROM.put(PID_EEPROM_ADDRESS+4, i);
        EEPROM.put(PID_EEPROM_ADDRESS+8, d);
        PID.SetTunings(p, i, d, 0);
      }
    }
    else
    {
      output = (unsigned char)PID.Compute(temperature);
    }
    analogWrite(FAN_PWM, output);
    sprintf(outgoingMessage, "T%0d.%02d O%0d\n", (int)temperature, (int)(((int)(temperature*100))%100), output);
    Serial.print(outgoingMessage);
  }
  digitalWrite(TACHO_PIN, tachoPin);
  tachoPin = !tachoPin;
  
  handleCommands();
}
