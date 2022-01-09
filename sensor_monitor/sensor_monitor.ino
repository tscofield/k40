//*This code watches the flow rate and temperature of your K40 Laser cooling liquid. 
//*If the flow stops, the relay will automaticly cut the output of the laser tube.
//*It displays the temperature in deg C or deg. F and the flow rate in L/min.
//*The display updates the values every second.
//*Changing deg. C to deg. F is as easy as swapping "lcd.print(Tc);" to "lcd.print(Tf);" in the code below.


#include<math.h>                                      //math for converting the thermistor data
#include "Wire.h"
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

// define device I2C address: 0x76 or 0x77 (0x77 is library default address)
#define BMP280_I2C_ADDRESS1  0x76
#define BMP280_I2C_ADDRESS2  0x77

// initialize the LCD library with the numbers of the interface pins
#define LCD_2004 True

#if defined(LCD_1602)
 LiquidCrystal_I2C lcd(0x27,16,2); // set the LCD address to 0x27 for a 16 chars and 2 line display
 #define lcd_rows 2
 #define lcd_cols 16
#elif defined(LCD_2004)
 LiquidCrystal_I2C lcd(0x27,20,4); // set the LCD address to 0x27 for a 16 chars and 2 line display
 #define lcd_rows 4
 #define lcd_cols 20
#else
 #define lcd_rows 0
 #define lcd_cols 0
#endif

// Setup arrays to manage acceptable temprature ranges for the temp probes
const int ThermisterCount = 3;

// Define the pins each probe is connected to
int ThermistorPin[ThermisterCount] = {A0,A1,A2};

// Define the critical temprature above which the laser will be shutoff
float TCrit[ThermisterCount] = {20,20,20};

// Define the warning temprature above which a warning will be sent
float TWarn[ThermisterCount] = {18,18,18};

// Base temprature, below this we assume the probe has failed
// Set to -274 or lower to disable, since it can never be that cold
// Something slightly below freezing should be sufficient
float TBase[ThermisterCount] = {-5,-5,-275};

// set initial error state for devices
// 0 = good
// 1 = warning
// 2 = critical
int TState[ThermisterCount] = {0,0,0};

// Thermister info
// https://create.arduino.cc/projecthub/iasonas-christoulakis/make-an-arduino-temperature-sensor-thermistor-tutorial-b26ed3

int Relay = 4;
//int flow1_Pin = 2;
int flow1_Pin = 15;   //D8 on D1 mini

double flow1_Rate;                                      // This is the value after the math that can be displayed or worked with 
volatile int flow1_count;
// Flow Meter Constant
float fmc =7.5;

Adafruit_BMP280 bmp2801;
Adafruit_BMP280 bmp2802;

// Set the laser pause to 1, paused
int laser_pause = 1;


void setup()
{
  Serial.begin(9600);
  Serial.println("Initializing LCD");
  lcd.init();
  lcd.backlight();

  lcd.setCursor(((lcd_cols/2)-4), (lcd_rows/2)-1);
  Serial.println("Display LCD banner");
  lcd.print("K40 Laser");                         // You can change this start up text to whatever you want, it appears on the display when you turn on the system
  lcd.setCursor(((lcd_cols/2)-3), ((lcd_rows/2)));
  lcd.print("Monitor");                         // You can change this start up text to whatever you want, it appears on the display when you turn on the system
  Serial.println("Waiting 3 seconds");
  delay(3000);
  for (int scrollCounter = 0; scrollCounter < lcd_cols; scrollCounter++) {
    lcd.scrollDisplayLeft();
    delay(50);
  }
  Serial.println("Display Cleared");

  Serial.println("Setup pins");
  pinMode(flow1_Pin, INPUT);
  //attachInterrupt(0, Flow1, RISING);
  attachInterrupt(flow1_Pin, Flow1, RISING);

  // Relay pin to low 
  // Relay should use the NO connection for saftey
  // Fails to an open state
  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, LOW);

  Serial.println("Initializing Pressure Sensor");
  pressureInit();

  Serial.println("Initialization Complete");
}

float tempcalc(int ThermistorPin) {
  // Caclulate Temps
  int Vo;
  float R1 = 10000;
  float logR2, R2, T, Tc, Tf;
  float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;
  // analogRead for default arduino is 0-1023
  // esp32 is between 0-4095
  #if defined(ESP32)
  float Vin=4095;
  #else
  float Vin=1023;
  #endif
  Vo = analogRead(ThermistorPin);
  R2 = R1 * (Vin / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  Tc = T - 273.15;
  // End Temp Calculations
  Serial.print("temp ");
  Serial.print(ThermistorPin);
  Serial.print(" : ");
  Serial.print(Tc);
  Serial.print("\n");
  //Serial.print("temp Vo: ");
  //Serial.print(Vo);
  //Serial.print("\n");
  //Serial.print("temp R2: ");
  //Serial.print(R2);
  //Serial.print("\n");
  //Serial.print("temp logR2: ");
  //Serial.print(logR2);
  //Serial.print("\n");
  //Serial.print("temp T: ");
  //Serial.print(T);
  //Serial.print("\n");
  return Tc;
}

float tempconvertCtoF(float Tc) {
  float Tf;
  Tf = (Tc * 9.0)/ 5.0 + 32.0;
  return Tf;
}

void pressureInit() {
  Serial.println("Init Pressuse sensor1");
  unsigned status1 = bmp2801.begin(BMP280_I2C_ADDRESS1);
  Serial.println("Init Pressuse sensor2");
  unsigned status2 = bmp2802.begin(BMP280_I2C_ADDRESS2);

  if (!status1) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    Serial.print("SensorID was: 0x"); Serial.println(bmp2801.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("        ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
  }
  if (!status2) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    Serial.print("SensorID was: 0x"); Serial.println(bmp2802.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("        ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
  }
  
}
float pressureGet1() {
  float pressure = bmp2801.readPressure();
  return pressure;
}
float pressureGet2() {
  float pressure = bmp2802.readPressure();
  return pressure;
}

void loop()
{
  // so we know if we paused this loop
  int local_pause = 0;
  
  Serial.print("looping ... \n");
  float TempC[3];
  float TempF[3];
  int int_TempC[3];
  int int_TempF[3];
  for (int i = 0; i < ThermisterCount; ++i) {
    TempC[i] = tempcalc(ThermistorPin[i]);
    int_TempC[i] = round(TempC[i]);
    if (TempC[i] < TBase[i]) {
      TState[i] = 3;
      Serial.print("Pausing laser, below base temp - laser #: ");
      Serial.println(i);
      local_pause = 1;
      bitSet(laser_pause, 1);
    } else if (TempC[i] < TWarn[i]) {
      TState[i] = 0;
    } else if (TempC[i] < TCrit[i]) {
      TState[i] = 1;
    } else if ( TempC[i] >= TCrit[i]) {
      TState[i] = 2;
      Serial.print("Pausing laser, above crit temp - laser #: ");
      Serial.println(i);
      local_pause = 1;
      bitSet(laser_pause, 1);
    } else {
      // something went horribly wrong
      Serial.print("Something went wrong, we should never end up in this section of code");
    }
  }
  
  Serial.print("Temps ");
  for (int i = 0; i < ThermisterCount; ++i)
    Serial.println(int_TempC[i]);
  Serial.print("\n");
  
  float pressure1, pressure2;
  pressure1 = pressureGet1();
  pressure2 = pressureGet2();
  Serial.print("Pressure 1: ");
  Serial.print(pressure1);
  Serial.print("\n");
  Serial.print("Pressure 2: ");
  Serial.print(pressure2);
  Serial.print("\n");

  // reset the count so we can get the current flow
  flow1_count = 0;
  delay(1000);
  flow1_Rate = (flow1_count / fmc);                          // tweak the Flow Sensor depending on minimum Flow

  Serial.print("\n");
  Serial.print("flow1: ");
  Serial.print(flow1_Rate);
  Serial.print("\n");

  if (flow1_Rate < -1) {
    local_pause = 1;
    bitSet(laser_pause, 1);
  }

  // if nothing paused the laser this loop, unset the global pause and let the job resume
  if (local_pause == 0) {
    laser_pause = 0;
  }

  // clear the lcd so we can write new info
  lcd.clear();
  lcd.setCursor(0,lcd_rows-2);
  lcd.print("T0 T1 T2 F1 P1 P2");
  lcd.setCursor(0,lcd_rows-1);
  lcd.print("## ## ## ## ## ##");
  for (int i = 0; i < ThermisterCount; ++i) {
    lcd.setCursor((i*3),lcd_rows-1);
    if (TempC[i] < TBase[1]) {
      // It too cold, can't be a real measurement
      Serial.print("Probe temp (");
      Serial.print(TempC[i]);
      Serial.print(") below base (");
      Serial.print(TBase[i]);
      Serial.print(")\n");
      lcd.print("**");
    } else {
      lcd.print(int_TempC[i]); 
    }
  }

  if (laser_pause > 0){
    Serial.print("ERROR - Laser currently paused\n");
//    Serial.print(
  }
}

void verbose_display(float Tc) {
 int pause = 0; // set something for now so the code runs
  if (pause == 0){                               // turns off the relay when "flowRate" is under 0.5L/min and opens the laser switch
    digitalWrite(Relay, HIGH);  
  
    lcd.print("Temp:");
    lcd.setCursor(6, 0);
    lcd.print(Tc);                                      // Prints the temperature of the cooling liquid on the display. To change this to Fahrenheit swap "Tc" with "Tf"
    lcd.setCursor(11, 0);
    lcd.print((char)223);
    lcd.setCursor(12, 0);
    lcd.print("C");                                     // When changing to Fahrenheit swap the "C" to "F"

    lcd.setCursor(0, 1);
    lcd.print("Flow1:");
    lcd.setCursor(6, 1);
    lcd.print(flow1_Rate);
    lcd.setCursor(11, 1);
    lcd.print("L/Min");
  }
  else {
    digitalWrite(Relay, LOW);
    lcd.clear();
    lcd.print(pause);
    int row = 0;
    if (bitRead(pause, 0)) {
      lcd.setCursor(3, row);
      lcd.print("PUMP ERROR");
      row++;
    }
    if (bitRead(pause, 1)) {
      lcd.setCursor(3, row);
      lcd.print("TEMP ERROR ");
      lcd.print(Tc);
      row++;
    }
    delay(1000);
  }
}

// ESp chips require the ICACHE_RAM_ATTR to be set before the interupt function definition
#if defined(ESP32)
ICACHE_RAM_ATTR void Flow1()
#elif defined(ESP8266)
ICACHE_RAM_ATTR void Flow1()
#else
void Flow1()
#endif
{
   flow1_count++; 
}
