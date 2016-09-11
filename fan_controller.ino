
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
//#include <TimerOne.h>

// for the fans
#define FANPIN A0

// for accessing DS1337 RTC
#include <Wire.h>
#define DS1337ADDRESS 0b1101000

#define INTPIN 2

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


void setTime(byte second, byte minute, byte hour, byte day, byte date, byte month, byte year){
  byte errorcode;
  Wire.beginTransmission(DS1337ADDRESS);
  Wire.write( 0 ); // start write at 0 (register for seconds)
  Wire.write(charToBCD(second));
  Wire.write(charToBCD(minute));
  Wire.write(charToBCD(hour));
  Wire.write(charToBCD(day));
  Wire.write(charToBCD(date));
  Wire.write(charToBCD(month));
  Wire.write(charToBCD(year));

  errorcode = Wire.endTransmission();
  if (errorcode != 0){
    lcd.setCursor(0,1);
    lcd.print("ERROR DS1337b:");
    lcd.print(String(errorcode));    
    return;
  }
}

/*
  convert a character of a number [0-9] to the corresponding byte-value
  - does not check the input
 */
byte char2byte(char value){
  return (value - '0');
}

/* 
  read settime.txt from SD and set RTC - then delete settime.txt

  Format in settime.txt
  20161231-0-14:35:55
  YYYYMMDD W HH mm SS
  Y Year
  M Month
  D Day
  W Weekday
  H Hour
  m Minute
  S Second
 */
void setTimeFromSD(){
  byte second, minute, hour, day, date, month, year;
  char buffer[20];
  File timefile;
  timefile = SD.open("settime.txt", FILE_READ);
  if (timefile){
    timefile.read(buffer, 19);
    year = char2byte(buffer[2]) * 10 + char2byte(buffer[3]);
    month = char2byte(buffer[4]) * 10 + char2byte(buffer[5]);
    date = char2byte(buffer[6]) * 10 + char2byte(buffer[7]);
    day = char2byte(buffer[9]);
    hour = char2byte(buffer[11]) * 10 + char2byte(buffer[12]);
    minute = char2byte(buffer[14]) * 10 + char2byte(buffer[15]);
    second = char2byte(buffer[17]) * 10 + char2byte(buffer[18]);
    setTime(second, minute, hour, day, date, month, year);
    timefile.close();
    SD.remove("settime.txt");
  }
}


/*
  convert a value to BCD-encoding 
  leads to wrong results if value > 99
 */
byte charToBCD(byte value){
  byte firstDigit;
  firstDigit = value/10;
  value = value - (firstDigit * 10);
  firstDigit <<= 4;
  firstDigit += value;
  return firstDigit;
}



/*
  set Alarm 2 on DS1337 to ring once per minute
 */
void setAlarmOncePerMinute(){
  byte errorcode;
  // set Alarm registers (first bit of 0xBH to 0xDH) to 111 (alarm every minute)
  Wire.beginTransmission(DS1337ADDRESS);
  Wire.write( 0x0B ); // first register of Alarm2
  Wire.write(0b10000000);
  Wire.write(0b10000000);
  Wire.write(0b10000000);
  Wire.write(0b00000110); // enable alarm interrupt, set interrupt control
  Wire.write(0);
  errorcode = Wire.endTransmission();
  if (errorcode != 0){
    lcd.setCursor(0,1);
    lcd.print("ERROR DS1337c:");
    lcd.print(String(errorcode));    
    return;
  }
}


/*
   clears the A2F Flag to aknowledge the alarm
 */
void resetAlarm(){
  byte errorcode;
  Wire.beginTransmission(DS1337ADDRESS);
  Wire.write( 0x0F); // status register
  Wire.write( 0 ); // reset alarm
  errorcode = Wire.endTransmission();
  if (errorcode != 0){
    lcd.setCursor(0,1);
    lcd.print("ERROR DS1337d:");
    lcd.print(String(errorcode));    
    return;
  }
}

void writeTimestampToSD(File datafile){
  char datestring[23];
  byte second, minute, hour, temp, date, month, year;
  byte errorcode;


  Wire.beginTransmission( DS1337ADDRESS );
  Wire.write( 0 );
  errorcode = Wire.endTransmission();
  if (errorcode != 0){
    //lcd.setCursor(0,1);
    //lcd.print("ERROR DS1337:");
    //lcd.print(String(errorcode));
    //return;
  }

  

  Wire.requestFrom(DS1337ADDRESS, 7);
  second = Wire.read();
  minute = Wire.read();
  hour = Wire.read();
  temp = Wire.read();  // ignore the day of week
  date = Wire.read();
  month = Wire.read();
  year = Wire.read();
  errorcode = Wire.endTransmission();

  datestring[0]='2';
  datestring[1]='0';
  temp = year & 0b11110000;
  temp >> 4;
  datestring[2]= '0' + temp;
  datestring[3]= '0' + ( year & 0b00001111);
  datestring[4]= '-';
  temp = month & 0b00010000;
  temp >> 4;
  datestring[5]= '0' + temp;
  datestring[6]= '0' + ( month & 0b00001111);
  datestring[7]= '-';
  temp = date & 0b00110000;
  temp >> 4;
  datestring[8]= '0' + temp;
  datestring[9]= '0' + ( date & 0b00001111);
  datestring[10]= ' ';
  temp = hour & 0b00110000;
  temp >> 4;
  datestring[11]= '0' + temp;
  datestring[12]= '0' + ( hour & 0b00001111);
  datestring[13]= ':';
  temp = minute & 0b01110000;
  temp >> 4;
  datestring[14]= '0' + temp;
  datestring[15]= '0' + ( minute & 0b00001111);
  datestring[16]= ':';
  temp = second & 0b01110000;
  temp >> 4;
  datestring[17]= '0' + temp;
  datestring[18]= '0' + ( second & 0b00001111);
  datestring[19]= ' ';
  datestring[20]= ';';
  datestring[21]= ' ';
  datestring[22]= 0;
  datafile.write(datestring);
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
    //lcd.print("I dht11 error     ");  
    lcd.print(dataString);
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
    //lcd.print("dht11 error     ");
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

  if (SD_present && !(errorDHT1 || errorDHT2)){
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open("temphum.txt", FILE_WRITE);

    // if the file is available, write to it:
    if (dataFile) {
      writeTimestampToSD(dataFile);
      dataFile.println(logString);
      dataFile.close();
    }
    // if the file isn't open, pop up an error:
    else {
      //lcd.print("SD error");
    }
  }

  resetAlarm();
}

void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    lcd.print("SD Card failed");
    SD_present = false;
  }
  SD_present = true;
  lcd.print("card initialized.");

  // enable DHT11 sensors
  dht.begin();
  dht2.begin();

  // enable FANPIN as output
  pinMode(FANPIN, OUTPUT);
  digitalWrite(FANPIN, LOW);

  Wire.begin();
  setTimeFromSD();
  setAlarmOncePerMinute();



  lcd.setCursor(0,1);
  lcd.write("Init complete");
  delay(100);

  // enable Interrupt on Pin 2
  pinMode(INTPIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INTPIN), measureAndProcess, FALLING);

  lcd.write("!");
}



void loop() {
  // nothing to do here - everything is handled by timer-interrupt
}
