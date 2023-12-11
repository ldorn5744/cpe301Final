#include <RTClib.h>
#include <dht.h>
#include <Stepper.h>


#include <LiquidCrystal.h>
#define RDA 0x80
#define TBE 0x20    

// UART Pointers
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;
// GPIO Pointers

volatile unsigned char *ddrA = (unsigned char *) 0x21;
volatile unsigned char *portA =    (unsigned char *) 0x22;
volatile unsigned char *pinA = (unsigned char *) 0x20;

volatile unsigned char *ddrB = (unsigned char *) 0x24;
volatile unsigned char *portB =    (unsigned char *) 0x25;
volatile unsigned char *pinB = (unsigned char *) 0x23;


volatile unsigned char *ddrD = (unsigned char *) 0x2A;
volatile unsigned char *portD =    (unsigned char *) 0x2B;
volatile unsigned char *pinD = (unsigned char *) 0x29;

volatile unsigned char *portL = (unsigned char *) 0x10B;
volatile unsigned char *ddrL = (unsigned char *) 0x10A;
volatile unsigned char *pinL = (unsigned char *) 0x109;

// ADC STuff
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;



// Timer Pointers
volatile unsigned char *myTCCR1A = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B = (unsigned char *) 0x81;
volatile unsigned char *myTCCR1C = (unsigned char *) 0x82;
volatile unsigned char *myTIMSK1 = (unsigned char *) 0x6F;
volatile unsigned int  *myTCNT1  = (unsigned  int *) 0x84;
volatile unsigned char *myTIFR1 =  (unsigned char *) 0x36;


// LCD Stuff
const int RS = 11, EN = 12, D4 = 2, D5 = 3, D6 = 4, D7 = 5;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// DHT Stuff

#define DHT11_PIN 53
dht DHT;

// Water level
#define POWER_PIN 52
#define SIGNAL_PIN A15
int value = 0; 

// Stepper
const int stepsPerRevolution = 2038; 
Stepper myStepper = Stepper(stepsPerRevolution, 30, 32, 34, 36);

// Fan Motor
#define SPEED_PIN 44
#define DIR1 46
#define DIR2 48
int mSpeed = 90;

// RTC Stuff
#define SDA 20
#define SDL 21
RTC_DS1307 rtc;


//Project vars
#define START_PIN 19
bool enabled = 0;
unsigned long previousMillis = 0;
#define MAX_TEMP 22
#define WATER_THRESHOLD 100
bool error = 0;
bool flip = 0;
bool switched = 0;

void setup() {
  U0Init(9600);
  lcd.begin(16, 2);
  *ddrB |= 0x02;  
  *portB &= 0xFD;


  // Water reset Button
  *ddrB &= 0xFB; // Pin 51 B2 
  *portB |= 0x04; // internal pullup enable

  //Motor Reset Button
  *ddrB &= 0xF7; // Pin 50 B3 
  *portB |= 0x08; // internal pullup enable

  //LED Buttons
  *ddrA |= 0x01; //A0 red
  *ddrA |= 0x04; // A2 yellow
  *ddrA |= 0x10; // A4 blue
  *ddrA |= 0x40; // A6 green



  *ddrL |= 0x20;
  *ddrL |= 0x08;
  *ddrL |= 0x02;
  adc_init();
  *ddrD &= 0xFB;
  *portD |= 0x04;
  attachInterrupt(digitalPinToInterrupt(START_PIN), offOn, RISING);
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

}

void loop() {
  // put your main code here, to run repeatedly:
  if(!error & enabled){
    if(!switched){
      printTime();
      switched = 1;
    }
    int chk = DHT.read11(DHT11_PIN);

    unsigned long currentMillis = millis();
    if(currentMillis - previousMillis > 60000){
      previousMillis = currentMillis;
      lcd.clear();
      lcd.setCursor(0,0); 
      lcd.print("Temp: ");
      lcd.print(DHT.temperature);
      lcd.print((char)223);
      lcd.print("C");
      lcd.setCursor(0,1);
      lcd.print("Humidity: ");
      lcd.print(DHT.humidity);
      lcd.print("%");
    }

    if(*pinB & 0x08){ // while the button is pressed control the step motor
      printTime();
      myStepper.setSpeed(5);
      myStepper.step(50);
    }

    
    *portB |= 0x02;
    my_delay(5);
    value = adc_read(15);
    *portB &= 0xFD;
    
    if(value < WATER_THRESHOLD){
      printTime();
      error = 1;
    }

    if(DHT.temperature > MAX_TEMP){ // turn on if greater than maxTemp
      if(!flip){
        printTime();
      }
      flip = 1;
      //blue LED ON
      *portA &= 0xBA; //turns off all 3 lights (not blue)
      *portA |= 0x10; // turn on blue

      *portL &= 0xF7; 
      *portL |= 0x02; 
      *portL |= 0x20; 
    }
    else{
      if(flip){
        printTime();
      }
      flip = 0;
      //green led on
      *portA &= 0xEA; //turns off all 3 lights (not green)
      *portA |= 0x40; // turn on green
      *portL &= 0xDF; 
    }
  }
  else if (error && enabled){
    *portA &= 0xAB; //turns off all 3 lights (not red)
    *portA |= 0x01; // turn on red

    *portL &= 0xDF; // turn off motor
    lcd.setCursor(0,0); 
    lcd.print("Water Level Low!");
    lcd.setCursor(0,1);
    lcd.print("Please add more water.");
    //add reset button
    if(*pinB & 0x04){
      printTime();
      error = 0;
      lcd.clear();
    }
  }
  else{
    //yellow LED On
    if(!switched){
      printTime();
      switched = 1;
    }
    *portA &= 0xAE; //turns off all 3 lights (not yellow)
    *portA |= 0x04; // turn on yellow
    *portL &= 0xDF; // turn off motor
    if(*pinB & 0x08){ // while the button is pressed control the step motor
      printTime();
      myStepper.setSpeed(5);
      myStepper.step(50);
    }
    lcd.clear();
  }
}

void offOn(){ 
  enabled = !enabled;
  switched = !switched;
  //printTime();
}

void printTime(){
  DateTime now = rtc.now();
  unsigned int digit;
  unsigned int year = now.year();
  for (int i = 3; i >= 0; i--){ 
    digit = ((int) floor((year / pow(10, i)))) % 10;
    putChar('0' + digit);
  }
  putChar('/');

  unsigned int month = now.month();
  for (int i = 1; i >= 0; i--){ 
    digit = ((int) floor((month / pow(10, i)))) % 10;
    putChar('0' + digit);
  }
  putChar('/');

  unsigned int day = now.day();
  for (int i = 1; i >= 0; i--){ 
    digit = ((int) floor((day / pow(10, i)))) % 10;
    putChar('0' + digit);
  }

  putChar(' ');
  putChar('-');
  putChar('-');
  putChar(' ');

  unsigned int hour = now.hour();
  for (int i = 1; i >= 0; i--){ 
    digit = ((int) floor((hour / pow(10, i)))) % 10;
    putChar('0' + digit);
  }
  putChar(':');

  unsigned int minute = now.minute();
  for (int i = 1; i >= 0; i--){ 
    digit = ((int) floor((minute / pow(10, i)))) % 10;
    putChar('0' + digit);
  }
  putChar(':');

  unsigned int second = now.second();
  for (int i = 1; i >= 0; i--){ 
    digit = ((int) floor((second / pow(10, i)))) % 10;
    putChar('0' + digit);
  }
  putChar('\n');
}

void U0Init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
unsigned char kbhit()
{
  return *myUCSR0A & RDA;
}
unsigned char getChar()
{
  return *myUDR0;
}
void putChar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}

void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}

unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

void my_delay(unsigned int millis)
{
  // stop the timer
  unsigned int ticks = millis*10000;
  *myTCCR1B &= 0xF8;
  // set the counts
  *myTCNT1 = (unsigned int) (65536 - ticks);
  // start the timer
  * myTCCR1A = 0x0;
  * myTCCR1B |= 0b00000001;
  // wait for overflow
  while((*myTIFR1 & 0x01)==0); // 0b 0000 0000
  // stop the timer
  *myTCCR1B &= 0xF8;   // 0b 0000 0000
  // reset TOV           
  *myTIFR1 |= 0x01;
}