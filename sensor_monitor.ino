//*This code watches the flow rate and temperature of your K40 Laser cooling liquid. 
//*If the flow stops, the relay will automaticly cut the output of the laser tube.
//*It displays the temperature in deg C or deg. F and the flow rate in L/min.
//*The display updates the values every second.
//*Changeing deg. C to deg. F is as easy as swapping "lcd.print(Tc);" to "lcd.print(Tf);" in the code below.


#include<math.h>                                      //math for converting the thermistor data
#include "Wire.h"
#include <LiquidCrystal_I2C.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal_I2C lcd(0x27,16,2); // set the LCD address to 0x27 for a 16 chars and 2 line display

int Relay = 4;
int flowPin = 2;
int ThermistorPin = A0;
int Vo;

double flowRate;                                      // This is the value after the math that can be displayed or worked with 
volatile int count;
float R1 = 10000;
float logR2, R2, T, Tc, Tf;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;
// Flow Meter Constant
float fmc =7.5;


void setup()
{
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(4, 0);
  lcd.print("K40 Laser");                         // You can change this start up text to whatever you want, it appears on the display when you turn on the system
  lcd.setCursor(5, 1);
  lcd.print("Monitor");                         // You can change this start up text to whatever you want, it appears on the display when you turn on the system
  delay(3000);
  for (int scrollCounter = 0; scrollCounter < 16; scrollCounter++) {
    lcd.scrollDisplayLeft();
    delay(50);
  }
 
  pinMode(flowPin, INPUT);
  attachInterrupt(0, Flow, RISING);

  // Relay pin to low 
  // Relay should use the NO connection for saftey
  // Fails to an open state
  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, LOW);
  }

void loop()
{
  Serial.print("looping ... \n");

  // Caclulate Temps
  Vo = analogRead(ThermistorPin);
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  Tc = T - 273.15;
  Tf = (Tc * 9.0)/ 5.0 + 32.0;
  // End Temp Calculations

  // reset the count so we can get the current flow
  count = 0;
  delay(1000);
  flowRate = (count / fmc);                          // tweak the Flow Sensor depending on minimum Flow

  Serial.print("flow: ");
  Serial.print(flowRate);
  Serial.print("\n");


  // clear the lcd so we can write new info
  lcd.clear();

  if (flowRate >= 0.5){                               // turns off the relay when "flowRate" is under 0.5L/min and opens the laser switch
    digitalWrite(Relay, HIGH);  
  
    lcd.print("Temp:");
    lcd.setCursor(6, 0);
    lcd.print(Tc);                                      // Prints the temperature of the cooling liquid on the display. To change this to Fahrenheit swap "Tc" with "Tf"
    lcd.setCursor(11, 0);
    lcd.print((char)223);
    lcd.setCursor(12, 0);
    lcd.print("C");                                     // When changing to Fahrenheit swap the "C" to "F"

    lcd.setCursor(0, 1);
    lcd.print("Flow:");
    lcd.setCursor(6, 1);
    lcd.print(flowRate);
    lcd.setCursor(11, 1);
    lcd.print("L/Min");
  }
  else {
    digitalWrite(Relay, LOW);
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("PUMP ERROR");
    delay(1000);
  }
}

void Flow()
{
   count++; 
}
