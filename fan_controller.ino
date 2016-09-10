
/*
  LiquidCrystal Library - Hello World

 Demonstrates the use a 16x2 LCD display.  The LiquidCrystal
 library works with all LCD displays that are compatible with the
 Hitachi HD44780 driver. There are many of them out there, and you
 can usually tell them by the 16-pin interface.

 This sketch prints "Hello World!" to the LCD
 and shows the time.

  The circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * LCD VSS pin to ground
 * LCD VCC pin to 5V
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)

 Library originally added 18 Apr 2008
 by David A. Mellis
 library modified 5 Jul 2009
 by Limor Fried (http://www.ladyada.net)
 example added 9 Jul 2009
 by Tom Igoe
 modified 22 Nov 2010
 by Tom Igoe

 This example code is in the public domain.

 http://www.arduino.cc/en/Tutorial/LiquidCrystal
 */

// include the library code:
#include <stdlib.h>
#include <LiquidCrystal.h>

// for SD-Card
#include <SPI.h>
#include <SD.h>
const int chipSelect = 10;


// for DHT11
#include <DHT.h>


#include <TimerOne.h>


#define DHTTYPE DHT11
#define DHTPIN 9
#define DHTPIN2 8
DHT dht(DHTPIN, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);


#define FANPIN A0


// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);



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

void measureAndProcess() {
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
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
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    t = dht.readTemperature();

    
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

void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    lcd.print("SD Card failed");
    // don't do anything more:
    return;
  }
  lcd.print("card initialized.");
  dht.begin();
  dht2.begin();

  pinMode(FANPIN, OUTPUT);
  digitalWrite(FANPIN, LOW);

  Timer1.initialize();
  Timer1.attachInterrupt(measureAndProcess, 30 * 1000000);
}



void loop() {
  // put your main code here, to run repeatedly:
  //measureAndProcess();
  //delay(5000);
}
