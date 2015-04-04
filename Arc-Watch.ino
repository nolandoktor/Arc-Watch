#include <Time.h>
#include <avr/sleep.h>


const int TOTAL_LED = 16;
const int TOTAL_COLS = 4;
const int TOTAL_ROWS = 4;

const int SECONDS_IN_MINUTE = 60;
const float LED_FLICKER_DELAY = 0.5;
const int CIRCLE_BOUNDARY = 13;

const int BUTTON1_PIN = 8;
const int BUTTON2_PIN = 9;

const int BUTTON1_PRESSED = 0;
const int BUTTON2_PRESSED = 1;
const int BUTTON1_HOLD = 2;
const int BUTTON2_HOLD = 3;

const int BUTTON_SENSITIVITY_MILLIS = 5;
const int BUTTON_HOLD_DELAY = 3000;

volatile int b1IntLastTime = -BUTTON_SENSITIVITY_MILLIS;
volatile int b2IntLastTime = -BUTTON_SENSITIVITY_MILLIS;

volatile boolean preemptAnimation = false;
volatile unsigned int currentState = 0;

volatile unsigned int buttonOnePressedTime = 0;
volatile unsigned int buttonTwoPressedTime = 0;

/*
            P0         P1        P2        P3
            C0         C1        C2        C3
P4 R0  4L0H(01)  4L1H(02)  4L2H(03)  4L3H(04)
P5 R1  5L0H(05)  5L1H(06)  5L2H(07)  5L3H(08)
P6 R2  6L0H(09)  6L1H(10)  6L2H(11)  6L3H(12)
P7 R3  7L0H(13)  7L1H(14)  7L2H(15)  7L3H(16)

*/

void setup()                   
{
//  setTime(23, 20, 30, 1, 1, 2014); // hour, minutes, secs
  setSyncProvider ( getCurrentTime );

  for ( int i = 0; i < 8; i ++ ) {
    pinMode ( i, OUTPUT ); 
  }
  
  pinMode ( BUTTON1_PIN, INPUT );
  pinMode ( BUTTON2_PIN, INPUT );

  attachInterrupt ( BUTTON1_PIN, buttonOnePinChange, CHANGE );
  attachInterrupt ( BUTTON2_PIN, buttonTwoPinChange, CHANGE );
  
  setSyncInterval ( 1 );
}

int getRowPin ( int led ) {
  return ( ( led - 1 ) / TOTAL_COLS ) + TOTAL_COLS;
}

int getColPin ( int led ) {
  return ( ( led - 1 ) % TOTAL_COLS );
}

void setState ( int rowPin, int colPin, boolean state ) {

  if ( state == HIGH ) {
   digitalWrite ( rowPin, LOW );
   digitalWrite ( colPin, HIGH );
  }
  else {
   digitalWrite ( rowPin, HIGH );
   digitalWrite ( colPin, LOW );
  }
} 

void setLedState ( int led, boolean state ) {
  
  if ( led > 0 ) {
    int row = getRowPin ( led );
    int col = getColPin ( led );
    setState ( row, col, state );
  }
}

void resetAll () {
  
  for ( int i = 1; i <= TOTAL_LED; i ++ ) {
    int row = getRowPin ( i );
    int col = getColPin ( i );
    setState ( row, col, LOW );
  }  
}

void animateLeds ( int outerLedNumber, int innerLedNumber, int forMillis ) {

  preemptAnimation = false;

  int ledsToAnimate [ TOTAL_LED ] = {0};
  int ledCounter = 0;
  
  for ( int led = 1; led <= TOTAL_LED; led ++ ) {
    if ( led < CIRCLE_BOUNDARY ) {
      if ( led <= outerLedNumber ) {
        ledsToAnimate [ ledCounter ++ ] = led;
      }
    }
    else {
      if ( led < ( CIRCLE_BOUNDARY + innerLedNumber ) ) {
        ledsToAnimate [ ledCounter ++ ] = led;
      }
    }
  }
  
//  int factor = ( TOTAL_LED - ledCounter ) * 2;
//  float animatorDelay = LED_FLICKER_DELAY / ( factor + 1 );
  float animatorDelay = LED_FLICKER_DELAY;

  int animationLength = 0;
  int startMillis = millis ();
  
  while ( !preemptAnimation && ( animationLength < forMillis ) ) {
  
    int i;
    for ( i = 0; i <= ledCounter; i ++ ) {
      setLedState ( ledsToAnimate [ i ], HIGH );
      delay ( animatorDelay );
      setLedState ( ledsToAnimate [ i ], LOW );
    }
    for ( ; i <= 16; i ++ ) {
      setLedState ( ledsToAnimate [ 0 ], LOW );
      delay ( animatorDelay );
      setLedState ( ledsToAnimate [ i ], LOW );
    }

    
    int currMillis = millis ();
    if ( currMillis < startMillis ) {
      currMillis += 1000;
    }
    
    animationLength += currMillis - startMillis;
  }
}

time_t getCurrentTime()
{
  return Teensy3Clock.get();
}

const int OUTER_LED_GROUP = 5;
const int STATE_MACHINE [ 5 ] [ 4 ] = { 
    { 1, 2, 3, 4 }, 
    { 0, 2, 3, 4 }, 
    { 1, 0, 3, 4 },
    { 3, 3, 0, 3 },
    { 4, 4, 3, 0 }
};
  
/*
  S0: Second Display
  S1: Hour Display
  S2: Minute Display
  S3: Sleep
  S4: Settings/All On
  
  B1: Pin 8 - 0
  B2: Pin 9 - 1
  B1H:      - 2
  B2H:      - 3

        B1    B2   B1H   B2H
  S0:    1    2      3     4
  S1:    0    2      3     4
  S2:    1    0      3     4
  S3:    3    3      0     3
  S4:    4    4      3     0
*/

void changeState ( int event ) {
  cli ();
    currentState = STATE_MACHINE [ currentState ] [ event ];
    preemptAnimation = true;
  sei ();
}

void buttonOnePinChange () {
  
  int pinVal = digitalRead ( BUTTON1_PIN );
  
  if ( pinVal == LOW ) {

    int currMillis = millis ();
    int timeLapse = currMillis - buttonOnePressedTime;
  
    if ( timeLapse >= BUTTON_HOLD_DELAY ) {
      changeState ( BUTTON1_HOLD );
      resetAll ();
      buttonOnePressedTime = 0;
    }
    else {
      if ( timeLapse >= BUTTON_SENSITIVITY_MILLIS ) {
        changeState ( BUTTON1_PRESSED );
        resetAll ();
        buttonOnePressedTime = 0;
      }
    }
  }
  else {
    buttonOnePressedTime = millis ();
  }
}

void buttonTwoPinChange () {
  
  int pinVal = digitalRead ( BUTTON2_PIN );
  
  if ( pinVal == LOW ) {
    
    int currMillis = millis ();
    int timeLapse = currMillis - buttonTwoPressedTime;

    if ( timeLapse >= BUTTON_HOLD_DELAY ) {
      changeState ( BUTTON2_HOLD );
      resetAll ();
      buttonTwoPressedTime = 0;
    }
    else {
      if ( timeLapse >= BUTTON_SENSITIVITY_MILLIS ) {
        changeState ( BUTTON2_PRESSED );
        resetAll ();
        buttonTwoPressedTime = 0;
      }
    }
  }
  else {
    buttonTwoPressedTime = millis ();
  }
}


void loop()                    
{
  int loopDelay = 1000;
  int hours;
  int minutes;
  int seconds;
  int outerCircleLed;
  int innerCircleLed;
  int animationLength = 1000; // 3 seconds
  
  resetAll ();
  
  while ( 1 ) {
  
    switch ( currentState ) {

      case 0:
        seconds = ( second() + 1 );
        outerCircleLed = ( seconds / OUTER_LED_GROUP );
        innerCircleLed = ( seconds % OUTER_LED_GROUP );
        animateLeds ( outerCircleLed, innerCircleLed, animationLength );
        break;
        
      case 1:
        hours = hour () + 1; // because it is in 24Hr format (0-23)
        if ( hours > 12 ) {
          hours = hours - 12;
        }
        outerCircleLed = hours;
        innerCircleLed = 0;
        animateLeds ( outerCircleLed, innerCircleLed, animationLength );    
        break;
        
      case 2:
        minutes = ( minute () + 1 );
        outerCircleLed = minutes / OUTER_LED_GROUP;
        innerCircleLed = ( minutes % OUTER_LED_GROUP );
        animateLeds ( outerCircleLed, innerCircleLed, animationLength );
        break;
        
      case 3:
        resetAll ();
        break;
        
       case 4:
        animateLeds ( 12, 4, animationLength );
        break;
    }
  }
} 
 
