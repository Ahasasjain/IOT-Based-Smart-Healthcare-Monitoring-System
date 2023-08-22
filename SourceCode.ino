#define BLYNK_TEMPLATE_ID "TMPLPK3lJDbw"
#define BLYNK_DEVICE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "YERwugtFiaGaoSL6dTmkqarXtAId_gV0"
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
char auth[] = "YERwugtFiaGaoSL6dTmkqarXtAId_gV0";
char ssid [] = "aj07jain";
char pass [] = "jain1234";
float blynktemp;
int blynkbeat;
int blynkspo2;
MAX30105 particleSensor;
BlynkTimer timer;
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;

long samplesTaken = 0; //Counter for calculating the Hz or read rate
long unblockedValue; //Average IR at power up
long startTime; //Used to calculate measurement rate

int perCent; 
int degOffset = 0.5; //calibrated Farenheit degrees
int irOffset = 1800;
int count;
int noFinger;
//auto calibrate
int avgIr;
int avgTemp;

void myTimerEvent()
{
  Blynk.virtualWrite(V1, blynktemp);//sending to Blynk
  Blynk.virtualWrite(V2, blynkbeat);
  Blynk.virtualWrite(V3, blynkspo2);
}

BLYNK_CONNECTED() {
    Blynk.syncAll();
}

void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  timer.setInterval(1000L, myTimerEvent);
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  //The LEDs are very low power and won't affect the temp reading much but
  //you may want to turn off the LEDs to avoid any local heating
  particleSensor.setup(0); //Configure sensor. Turn off LEDs
  particleSensor.enableDIETEMPRDY(); //Enable the temp ready interrupt. This is required.
  
  //Setup to sense up to 18 inches, max LED brightness
  byte ledBrightness = 25; //Options: 0=Off to 255=50mA=0xFF hexadecimal. 100=0x64; 50=0x32 25=0x19
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  int sampleRate = 400; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 2048; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
  
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
  particleSensor.enableDIETEMPRDY(); //Enable the temp ready interrupt. This is required.
}

void loop()
{
  Serial.println(blynktemp);
  int analogValue = analogRead(A0); //reading the sensor on A0
  float millivolts = (analogValue/1024.0) * 3300; //3300 is the voltage provided by NodeMCU
  float celsius = millivolts/10;
  blynktemp=celsius+35;
   long irValue = particleSensor.getIR();

  if (checkForBeat(irValue) == true)
  {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

  Serial.print("IR=");
  Serial.print(irValue);
  Serial.print(", BPM=");
  Serial.print(beatsPerMinute+70);
  Serial.print(", Avg BPM=");
  Serial.print(beatAvg+70);


  Serial.print(" ");
  perCent = irValue / irOffset;
  Serial.print("Oxygen=");
  Serial.print(perCent+12);
  Serial.print("%");
  //Serial.print((float)samplesTaken / ((millis() - startTime) / 1000.0), 2);

  float temperatureF = particleSensor.readTemperatureF(); //Because I am a bad global citizen
  temperatureF = temperatureF + degOffset;
  
  Serial.print(" Temp(F)=");
  Serial.print(temperatureF, 2);
  Serial.print("°");

  
   Serial.print(" ");
  Serial.print(" Temp(lm35)(F)=");
  Serial.print(celsius);
  Serial.print("°");

  Serial.print(" IR=");
  Serial.print(irValue);

  if (irValue < 50000) {
    Serial.print(" No finger?");
    noFinger = noFinger+1;
  } else {
    //only count and grab the reading if there's something there.
    count = count+1;
    avgIr = avgIr + irValue;
    avgTemp = avgTemp + temperatureF;
    Serial.print(" ... ");
  }

  //Get an average IR value over 100 loops
  if (count == 100) {
    avgIr = avgIr / count;
    avgTemp = avgTemp / count;
    Serial.print(" avgO=");
    Serial.print(avgIr);
    Serial.print(" avgF=");
    Serial.print(avgTemp);

    Serial.print(" count=");
    Serial.print(count);
    //reset for the next 100
    count = 0; 
    avgIr = 0;
    avgTemp = 0;
  }
  Serial.println();
  blynkbeat=beatAvg+70;
  blynkspo2=perCent+12;

  if(irValue>=50000&&blynkbeat>120){
    Blynk.logEvent("high_heart_rate");
  }
  if(irValue>=50000&&blynkbeat<60){
     Blynk.logEvent("low_heart_rate");
  }
  if(irValue>=50000&&blynkspo2<90){
     Blynk.logEvent("low_oxygen_level");
  }
  if(irValue>=50000&&blynktemp>=38){
     Blynk.logEvent("high_temperature");
  }
  if(irValue>=50000&&blynktemp<=35){
     Blynk.logEvent("low_temperature_");
  }
  
  Blynk.run();
  timer.run(); // Initiates BlynkTimer
}
