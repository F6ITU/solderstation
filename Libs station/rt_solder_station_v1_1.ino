/*
Arduino Project for an SMD solder station for Weller RT solder tips based on Arduino One.

Requires the Timer1 library (available under http://playground.arduino.cc/code/timer1, simply unzip and copy to Arduino/hardware/libraries/Timer1/)
*/
#include <EEPROM.h>
#include "TimerOne.h"  

//#define PROTOTYPE

#define CNTRL_GAIN 10

//Times:
#define TIME_DISP_MUX_IN_MS 2
#define TIME_DISP_BLINK_IN_MS 500
#define TIME_DISP_REFRESH_IN_MS 1000
#define TIME_DISP_RESET_IN_MS 3000
#define TIME_SW_POLL_IN_MS 10
#define DELAY_MAIN_LOOP_IN_MS 10
#define PWM_DIV 1024 //default: 64
//#define PWM_DIV 64 //default: 64

#define MAX_TARGET_TEMP_IN_DEGREES 400
//#define MAX_TARGET_TEMP_IN_DEGREES 600

#define STDBY_TEMP_IN_DEGREES 150

//Pins:
#define PIN_LED 13

#ifdef PROTOTYPE

//R2=65K:
#define ADC_TO_TEMP_GAIN 0.4079
#define ADC_TO_TEMP_OFFSET 25.0

#define DELAY_BEFORE_MEASURE_IN_MS 50

#define PIN_STDBY_SW A2
#define PIN_HEAT_LED A3
#define PIN_PWM_OUT 5

#define PIN_7SEG_A 8
#define PIN_7SEG_B 7
#define PIN_7SEG_C 6
#define PIN_7SEG_D A5
#define PIN_7SEG_E A4
#define PIN_7SEG_F 3
#define PIN_7SEG_G 2
#define PIN_7SEG_DP 4

#define PIN_7SEG_ACT_LEFT 9
#define PIN_7SEG_ACT_MIDDLE 10
#define PIN_7SEG_ACT_RIGHT 11

#define PIN_SW_DOWN 12
#define PIN_SW_UP 13

#else

//R2=68K:
#define ADC_TO_TEMP_GAIN 0.39
#define ADC_TO_TEMP_OFFSET 23.9

#define DELAY_BEFORE_MEASURE_IN_MS 10

#define PIN_STDBY_SW A2
#define PIN_HEAT_LED A3
#define PIN_PWM_OUT 3

#define PIN_7SEG_A 6
#define PIN_7SEG_B 7
#define PIN_7SEG_C 8
#define PIN_7SEG_D 5
#define PIN_7SEG_E A4
#define PIN_7SEG_F 2
#define PIN_7SEG_G 4
//#define PIN_7SEG_G A1 //Konrad
#define PIN_7SEG_DP A5

#define PIN_7SEG_ACT_LEFT 11
#define PIN_7SEG_ACT_MIDDLE 10
#define PIN_7SEG_ACT_RIGHT 9

#define PIN_SW_DOWN 12
#define PIN_SW_UP 13

#endif //PROTOTYPE

#define DELAY_BEFORE_MEASURE DELAY_BEFORE_MEASURE_IN_MS
#define DELAY_MAIN_LOOP DELAY_MAIN_LOOP_IN_MS


int pwm;

int target_temperature;
//int value_last1,value_last2,value_last3;
int actual_temperature;
bool heater_active=false;
bool disp_on=true;
bool disp_blink=false;

char segment=0;
char disp_digit=0;
int disp=0;
int disp_act=0;

int cnt_disp_blink=0;
int cnt_disp_mux=0;
int cnt_disp_refresh=0;
int cnt_sw_poll=0;
int cnt_disp_reset=0;
int cnt_pwm=0;

/*
Set the frequency of the PWM output (code from http://playground.arduino.cc/Code/PwmFrequency):
*/
void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if(pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if(pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if(pin == 3 || pin == 11) {
    switch(divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x7; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}

/*
Select the Digit of the 7-segment display:
*/
void select7Seg(char num)
{
  switch(num)
  {
    case 0:
      digitalWrite(PIN_7SEG_ACT_RIGHT, HIGH);
      digitalWrite(PIN_7SEG_ACT_MIDDLE, LOW);
      digitalWrite(PIN_7SEG_ACT_LEFT, LOW);
      break;
    case 1:
      digitalWrite(PIN_7SEG_ACT_RIGHT, LOW);
      digitalWrite(PIN_7SEG_ACT_MIDDLE, HIGH);
      digitalWrite(PIN_7SEG_ACT_LEFT, LOW);
      break;
    case 2:
      digitalWrite(PIN_7SEG_ACT_RIGHT, LOW);
      digitalWrite(PIN_7SEG_ACT_MIDDLE, LOW);
      digitalWrite(PIN_7SEG_ACT_LEFT, HIGH);
      break;
    default:
      digitalWrite(PIN_7SEG_ACT_RIGHT, LOW);
      digitalWrite(PIN_7SEG_ACT_MIDDLE, LOW);
      digitalWrite(PIN_7SEG_ACT_LEFT, LOW);
  }
}

/*
Display a number on the 7-segment display:
*/
void set7Seg(char num)
{
  if(disp_on)
  {
    switch(num)
    {
      case 0:
        digitalWrite(PIN_7SEG_A, HIGH);
        digitalWrite(PIN_7SEG_B, HIGH);
        digitalWrite(PIN_7SEG_C, HIGH);
        digitalWrite(PIN_7SEG_D, HIGH);
        digitalWrite(PIN_7SEG_E, HIGH);
        digitalWrite(PIN_7SEG_F, HIGH);
        digitalWrite(PIN_7SEG_G, LOW);
        break;
      case 1:
        digitalWrite(PIN_7SEG_A, LOW);
        digitalWrite(PIN_7SEG_B, HIGH);
        digitalWrite(PIN_7SEG_C, HIGH);
        digitalWrite(PIN_7SEG_D, LOW);
        digitalWrite(PIN_7SEG_E, LOW);
        digitalWrite(PIN_7SEG_F, LOW);
        digitalWrite(PIN_7SEG_G, LOW);
        break;
      case 2:
        digitalWrite(PIN_7SEG_A, HIGH);
        digitalWrite(PIN_7SEG_B, HIGH);
        digitalWrite(PIN_7SEG_C, LOW);
        digitalWrite(PIN_7SEG_D, HIGH);
        digitalWrite(PIN_7SEG_E, HIGH);
        digitalWrite(PIN_7SEG_F, LOW);
        digitalWrite(PIN_7SEG_G, HIGH);
        break;
      case 3:
        digitalWrite(PIN_7SEG_A, HIGH);
        digitalWrite(PIN_7SEG_B, HIGH);
        digitalWrite(PIN_7SEG_C, HIGH);
        digitalWrite(PIN_7SEG_D, HIGH);
        digitalWrite(PIN_7SEG_E, LOW);
        digitalWrite(PIN_7SEG_F, LOW);
        digitalWrite(PIN_7SEG_G, HIGH);
        break;
      case 4:
        digitalWrite(PIN_7SEG_A, LOW);
        digitalWrite(PIN_7SEG_B, HIGH);
        digitalWrite(PIN_7SEG_C, HIGH);
        digitalWrite(PIN_7SEG_D, LOW);
        digitalWrite(PIN_7SEG_E, LOW);
        digitalWrite(PIN_7SEG_F, HIGH);
        digitalWrite(PIN_7SEG_G, HIGH);
        break;
      case 5:
        digitalWrite(PIN_7SEG_A, HIGH);
        digitalWrite(PIN_7SEG_B, LOW);
        digitalWrite(PIN_7SEG_C, HIGH);
        digitalWrite(PIN_7SEG_D, HIGH);
        digitalWrite(PIN_7SEG_E, LOW);
        digitalWrite(PIN_7SEG_F, HIGH);
        digitalWrite(PIN_7SEG_G, HIGH);
        break;
      case 6:
        digitalWrite(PIN_7SEG_A, HIGH);
        digitalWrite(PIN_7SEG_B, LOW);
        digitalWrite(PIN_7SEG_C, HIGH);
        digitalWrite(PIN_7SEG_D, HIGH);
        digitalWrite(PIN_7SEG_E, HIGH);
        digitalWrite(PIN_7SEG_F, HIGH);
        digitalWrite(PIN_7SEG_G, HIGH);
        break;
      case 7:
        digitalWrite(PIN_7SEG_A, HIGH);
        digitalWrite(PIN_7SEG_B, HIGH);
        digitalWrite(PIN_7SEG_C, HIGH);
        digitalWrite(PIN_7SEG_D, LOW);
        digitalWrite(PIN_7SEG_E, LOW);
        digitalWrite(PIN_7SEG_F, LOW);
        digitalWrite(PIN_7SEG_G, LOW);
        break;
      case 8:
        digitalWrite(PIN_7SEG_A, HIGH);
        digitalWrite(PIN_7SEG_B, HIGH);
        digitalWrite(PIN_7SEG_C, HIGH);
        digitalWrite(PIN_7SEG_D, HIGH);
        digitalWrite(PIN_7SEG_E, HIGH);
        digitalWrite(PIN_7SEG_F, HIGH);
        digitalWrite(PIN_7SEG_G, HIGH);
        break;
      case 9:
        digitalWrite(PIN_7SEG_A, HIGH);
        digitalWrite(PIN_7SEG_B, HIGH);
        digitalWrite(PIN_7SEG_C, HIGH);
        digitalWrite(PIN_7SEG_D, HIGH);
        digitalWrite(PIN_7SEG_E, LOW);
        digitalWrite(PIN_7SEG_F, HIGH);
        digitalWrite(PIN_7SEG_G, HIGH);
        break;
    }
  }
  else
  {
    digitalWrite(PIN_7SEG_A, LOW);
    digitalWrite(PIN_7SEG_B, LOW);
    digitalWrite(PIN_7SEG_C, LOW);
    digitalWrite(PIN_7SEG_D, LOW);
    digitalWrite(PIN_7SEG_E, LOW);
    digitalWrite(PIN_7SEG_F, LOW);
    digitalWrite(PIN_7SEG_G, LOW);
  }
}

/*
The setup routine, runs once when you press reset:
*/
void setup() {
  Serial.begin(115200); //set serial debug interface to 115200 Baud
  
  //set pin directions:
  pinMode(PIN_PWM_OUT, OUTPUT);     
  pinMode(PIN_HEAT_LED, OUTPUT);     
  pinMode(PIN_7SEG_A, OUTPUT);     
  pinMode(PIN_7SEG_B, OUTPUT);     
  pinMode(PIN_7SEG_C, OUTPUT);     
  pinMode(PIN_7SEG_D, OUTPUT);     
  pinMode(PIN_7SEG_E, OUTPUT);     
  pinMode(PIN_7SEG_F, OUTPUT);     
  pinMode(PIN_7SEG_G, OUTPUT);     
  pinMode(PIN_7SEG_DP, OUTPUT);     
  pinMode(PIN_7SEG_ACT_LEFT, OUTPUT);     
  pinMode(PIN_7SEG_ACT_MIDDLE, OUTPUT);     
  pinMode(PIN_7SEG_ACT_RIGHT, OUTPUT);     
  pinMode(PIN_SW_DOWN, INPUT);     
  pinMode(PIN_SW_UP, INPUT);
  pinMode(PIN_STDBY_SW, INPUT);
  
  digitalWrite(PIN_SW_DOWN, HIGH); //activate pull up
  digitalWrite(PIN_SW_UP, HIGH); //activate pull up
  digitalWrite(PIN_STDBY_SW, HIGH); //activate pull up


  //set pwm frequency:
  setPwmFrequency(PIN_PWM_OUT, PWM_DIV);
  Serial.println("starting");
  Serial.flush();
  pwm=(25*255)/100;
  digitalWrite(PIN_PWM_OUT, LOW);
    
  digitalWrite(PIN_7SEG_DP, LOW);
  select7Seg(-1);

  target_temperature = EEPROM.read(0) | (((int) EEPROM.read(1)) << 8);

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timer_isr);
}

/*
Reads the temperature value from the ADC:
*/
int getTemperature()
{
  analogWrite(PIN_PWM_OUT, 0); //switch off heater
  delay(DELAY_BEFORE_MEASURE); //wait for some time (to get low pass filter in steady state)
  int adcValue = analogRead(A0); // read the input on analog pin 0:
  analogWrite(PIN_PWM_OUT, pwm); //switch heater back to last value
  return round(((float) adcValue)*ADC_TO_TEMP_GAIN+ADC_TO_TEMP_OFFSET); //apply linear conversion to actual temperature
}

/*
This function is periodically called every TIME_DISP_BLINK_IN_MS (for blinking of display during setting the target temperature):
*/
void timer_disp_blink() 
{
  if(disp_blink)
  {
    disp_on = !disp_on;
  }
}

/*
This function is periodically called every TIME_DISP_MUX_IN_MS (for time multiplexing the 7-segment digits):
*/
void timer_disp_mux()
{
  segment = segment >=2 ? segment=0 : segment+1;
  switch(segment)
  {
    case 0:
      disp_act = disp;
      disp_digit = ((int) disp_act) % 10;
      break;
    case 1:
      disp_digit = (((int) disp_act)/10) % 10;
      break;
    case 2:
      disp_digit = ((int) disp_act) / 100;
      break;
  }
  select7Seg(-1); //switch off display
  set7Seg(disp_digit); //display digit
  select7Seg(segment); //select digit
}


/*
This function is periodically called every TIME_DISP_REFRESH_IN_MS (for selecting the display information (actual/target)):
*/
void timer_disp_refresh()
{
//  actual_temperature = 123.0;
  if(disp_blink)
    disp = target_temperature;
  else
    disp = actual_temperature;
}

bool sw_up_old = false;
bool sw_down_old = false;
int cnt_but_press = 0;

/*
This function is periodically called every TIME_SW_POLL_IN_MS (for checking the up/down buttons):
*/
void timer_sw_poll()
{
  bool sw_up = !digitalRead(PIN_SW_UP);
  bool sw_down = !digitalRead(PIN_SW_DOWN);
  bool sw_changed = (sw_up != sw_up_old) || (sw_down !=sw_down_old);
  
  if(sw_up || sw_down)
  {
    cnt_but_press++;

    disp_on = true;
    cnt_disp_blink=0;
    cnt_disp_reset=0;

    if((cnt_but_press >= 100) || sw_changed)
    {
      if(sw_up)
        target_temperature += 1;
      if(sw_down)
        target_temperature -= 1;

      if(target_temperature < 0)
        target_temperature = 0;
      else 
      if(target_temperature > MAX_TARGET_TEMP_IN_DEGREES)
        target_temperature = MAX_TARGET_TEMP_IN_DEGREES;
        
      disp_blink = true;
      timer_disp_refresh();
      if(!sw_changed)
        cnt_but_press=97;
    }
  }
  else
  {
    cnt_but_press=0;
  }
  sw_up_old=sw_up;
  sw_down_old=sw_down;
}

/*
This function is periodically called every TIME_DISP_RESET_IN_MS (for setting the display back to actual temperature and to store the result in the EEPROM):
*/
void timer_disp_reset()
{
  disp_blink=false;
  disp_on=true;
  
  EEPROM.write(0, target_temperature & 0xff);
  EEPROM.write(1, (target_temperature & 0xff00) >> 8);
}

/*
This function is the interrupt service routine which is called every ms. Several counters are used then for "virtual" timers.
*/
void timer_isr() 
{  
  if(cnt_disp_blink >= TIME_DISP_BLINK_IN_MS)
  {
    timer_disp_blink();
    cnt_disp_blink=0;
  }
  cnt_disp_blink++;

  if(cnt_disp_mux >= TIME_DISP_MUX_IN_MS)
  {
    timer_disp_mux();
    cnt_disp_mux=0;
  }
  cnt_disp_mux++;

  if(cnt_disp_refresh >= TIME_DISP_REFRESH_IN_MS)
  {
    timer_disp_refresh();
    cnt_disp_refresh=0;
  }
  cnt_disp_refresh++;
  
  if(cnt_sw_poll >= TIME_SW_POLL_IN_MS)
  {
    timer_sw_poll();
    cnt_sw_poll=0;
  }
  cnt_sw_poll++;

  if(cnt_disp_reset >= TIME_DISP_RESET_IN_MS)
  {
    timer_disp_reset();
    cnt_disp_reset=0;
  }
  cnt_disp_reset++;
}


// the loop routine runs over and over again forever:
void loop() {
  //get actual ADC temperature:
  actual_temperature = getTemperature();
  
  //get state of sandby switch:  
  bool sw_stdby = !digitalRead(PIN_STDBY_SW);
  int target_temperature_tmp;
  if(sw_stdby)
  {
    target_temperature_tmp = STDBY_TEMP_IN_DEGREES; //override target temperature
  }  
  else
  {
    target_temperature_tmp = target_temperature;
  }
  
  //compute difference between target and actual temperature:
  int diff = target_temperature_tmp-actual_temperature;
  
  //apply P controller:  
  pwm = diff*CNTRL_GAIN;
  
  //limit pwm value to 0...255:
  pwm = pwm > 255 ? pwm = 255 : pwm < 0 ? pwm = 0 : pwm;
  
  //set PWM:
  analogWrite(PIN_PWM_OUT, pwm);

  //set heat LED
  if(pwm > 0)
    digitalWrite(PIN_HEAT_LED, HIGH);
  else
    digitalWrite(PIN_HEAT_LED, LOW);

  //display debug information:  
  Serial.print(target_temperature);
  Serial.print(" & ");
  Serial.print(actual_temperature);
  Serial.print(" & ");
  Serial.print(diff);
  Serial.print(" & ");
  Serial.println(pwm);
  Serial.flush();
  delay(DELAY_MAIN_LOOP); //wait for some time
}
