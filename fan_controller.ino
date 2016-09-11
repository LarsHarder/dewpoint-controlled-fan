
/*

  fan controller based on dewpoint of outside and inside air


  To reduce the humidity in a cellar two fans are installed to replace the damp air inside
  with air from the outside. The fans only run when the outside air has a lower water-content
  than the air inside.

  Hardware consists of an Arduino One clone, a LCD, a SD-Card Reader and two DHT11 Sensors.
  
  Project Page: tbd
  Author:   Lars Harder

  used libraries
    LCD       https://www.arduino.cc/en/Reference/LiquidCrystal
    DHT11     https://github.com/adafruit/DHT-sensor-library
    TimerOne  http://playground.arduino.cc/Code/Timer1
    SD        https://www.arduino.cc/en/Reference/SD

  copied functions
    Dewpoint  http://playground.arduino.cc/Main/DHT11Lib

*/

//for LCD
#include <stdlib.h>
#include <LiquidCrystal.h>

// for SD-Card
#include <SPI.h>
#include <SD.h>
const int chipSelect = 10;

// for DHT11
#include <DHT.h>
#define DHTTYPE DHT11
#define DHTPIN 9
#define DHTPIN2 8
DHT dht(DHTPIN, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

// for Timer
#include <TimerOne.h>

// for the fans
#define FANPIN A0


// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 1, 0);


bool SD_present=false;

/*
 * copied from http://playground.arduino.cc/Main/DHT11Lib
 */
double dewPoint(double celsius, double humidity)
{
  // (1) Saturation Vapor Pressure = ESGG(T)
  double RATIO = 373.15 / (273.15 + celsius);
  double RHS = -7.90298 * (RATIO - 1);
  RHS += 5.02808 * log10(RATIO);
  RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
  RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
  RHS += log10(1013.246);

        // factor -3 is to adjust units - Vapor Pressure SVP * humidity
  double VP = pow(10, RHS - 3) * humidity;

        // (2) DEWPOINT = F(Vapor Pressure)
  double T = log(VP/0.61078);   // temp var
  return (241.88 * T) / (17.558 - T);
}

/*
    state machine to control the fans
    If dewpoint of exterior is at least 5°C lower then the dewpoint of the inside fans are enabled (state = 1).
    If dewpoint of exterior raises above 2°C lower the the dewpoint of the inside fans are disabled (state = 0).
 */
int FSM_fan_control(float dewpointInterior, float dewpointExterior){
  static int state=0;

  switch (state){
    case 0:
      if ((dewpointInterior - dewpointExterior) >= 5){  
        // switch fans on
        state = 1;
        digitalWrite(FANPIN, HIGH);
      }
      break;
    case 1:
      if ((dewpointInterior - dewpointExterior) < 2){
        // switch fans off
        state = 0;
        digitalWrite(FANPIN, LOW);
      }
      break;
  }
  return state;
}

/*
    monolythic function to be split up...
 */
void measureAndProcess() {
  String dataString = "";
  String logString = "";
  bool errorDHT1;
  bool errorDHT2;
  int retryCounter = 0;
  char charVal[12];
  float h;
  float t;
  int stateFSM;

  float dewpointInterior;
  float dewpointExterior;

  while (retryCounter <= 9){
    delay(2000);
    errorDHT1 = false;
    h = dht.readHumidity();
    t = dht.readTemperature();

    // check for error in reading DHT11
    if (isnan(h) ||isnan(t)){
      errorDHT1 = true;      
      lcd.setCursor(15,0);
      retryCounter ++;
     } else break;
  }

  
  dtostrf(t, 3, 0, charVal);
  dataString = "I ";
  dataString += charVal;
  dataString +="C ";

  dtostrf(h,3,0, charVal);
  dataString += charVal;
  dataString += "% ";

  dewpointInterior = dewPoint(t,h);
  dtostrf(dewpointInterior,3,0, charVal);
  dataString += charVal;
  dataString += " ";


  dtostrf(t, 6, 2, charVal);
  logString += charVal;
  logString += ", ";
  dtostrf(h, 6, 2, charVal);
  logString += charVal;
  logString += ", ";

  lcd.setCursor(0,0);
  if (errorDHT1){
    lcd.print("I dht11 error     ");  
  } else {
    lcd.print(dataString);
  }
  

  lcd.setCursor(0, 1);

  errorDHT2 = false;
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  h = dht2.readHumidity();
  // Read temperature as Celsius (the default)
  t = dht2.readTemperature();

  if (isnan(h) ||isnan(t)){
    errorDHT2 = true;
    lcd.print("dht11 error     ");
  }

  dtostrf(t, 3, 0, charVal);
  dataString = "A ";
  dataString += charVal;
  dataString +="C ";

  dtostrf(h,3,0, charVal);
  dataString += charVal;
  dataString += "% ";

  dewpointExterior =  dewPoint(t,h);
  dtostrf(dewpointExterior,3,0, charVal);
  dataString += charVal;
  dataString += " ";
  
  lcd.print(dataString);

  
  dtostrf(t, 6, 2, charVal);
  logString += charVal;
  logString += ", ";
  dtostrf(h, 6, 2, charVal);
  logString += charVal;
  logString += ", ";

  if (!(errorDHT1 || errorDHT2)){
    stateFSM = FSM_fan_control(dewpointInterior, dewpointExterior);
  }
  
  logString += String(stateFSM);

  if (SD_present){
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open("temphum.txt", FILE_WRITE);

    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(logString);
      dataFile.close();
    }
    // if the file isn't open, pop up an error:
    else {
      lcd.print("SD error");
    }
  }
  
}

void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    lcd.print("SD Card failed");
    SD_present = false;
    return;
  }
  SD_present = true;
  lcd.print("card initialized.");

  // enable DHT11 sensors
  dht.begin();
  dht2.begin();

  // enable FANPIN as output
  pinMode(FANPIN, OUTPUT);
  digitalWrite(FANPIN, LOW);

  // setup timer for interrupt 
  Timer1.initialize();
  Timer1.attachInterrupt(measureAndProcess, 30 * 1000000);
}



void loop() {
  // nothing to do here - everything is handled by timer-interrupt
}
