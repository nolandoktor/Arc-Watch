
#include <Time.h>

const int TOTAL_LED = 16;
const int TOTAL_COLS = 4;
const int TOTAL_ROWS = 4;

const int SECONDS_IN_MINUTE = 60;
const float LED_FLICKER_DELAY = 0.5;
const int CIRCLE_BOUNDARY = 13;
const int OUTER_LED_GROUP = 5;

const int BUTTON1_PIN = 8;
const int BUTTON2_PIN = 9;

const int PRESSED_B1 = 0;
const int PRESSED_B2 = 1;
const int HOLD_B1 = 2;
const int HOLD_B2 = 3;
const int PRESSED_2B = 4;
const int HOLD_2B = 5;

const int TOTAL_EVENTS = 6;

const int TOTAL_BUTTONS = 2;
const int BUTTON1_INDEX = 0;
const int BUTTON2_INDEX = 1;

const int BUTTON_SENSITIVITY_MILLIS = 5;
const int BUTTON_HOLD_DELAY = 3000;

volatile boolean preemptAnimation = false;
volatile unsigned int currentState = 0;
volatile unsigned int buttonPressedTime [ TOTAL_BUTTONS ] = {0};

void setup()                   
{
//  setTime(23, 20, 30, 1, 1, 2014); // hour, minutes, secs
  setSyncProvider ( getCurrentTime );

  for ( int i = 0; i < 8; i ++ ) {
    pinMode ( i, OUTPUT ); 
  }
  
  pinMode ( BUTTON1_PIN, INPUT );
  pinMode ( BUTTON2_PIN, INPUT );

  attachInterrupt ( BUTTON1_PIN, buttonStateChange, CHANGE );
  attachInterrupt ( BUTTON2_PIN, buttonStateChange, CHANGE );
  
  setSyncInterval ( 1 );
}


time_t getCurrentTime()
{
  return Teensy3Clock.get();
}

const int STATE_MACHINE [ 5 ] [ TOTAL_EVENTS ] = { 
    { 1, 2, 3, 4, 0, 0 }, 
    { 0, 2, 3, 4, 0, 0 }, 
    { 1, 0, 3, 4, 0, 0 },
    { 3, 3, 0, 3, 0, 0 },
    { 4, 4, 3, 0, 0, 0 }
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
  2B:       - 4
  2BH:      - 5

        B1    B2   B1H   B2H
  S0:    1    2      3     4
  S1:    0    2      3     4
  S2:    1    0      3     4
  S3:    3    3      0     3
  S4:    4    4      3     0
*/

void changeState ( int event ) {
  currentState = STATE_MACHINE [ currentState ] [ event ];
  preemptAnimation = true;
  resetAll ();
}

int detectEvent ( int buttonIndex, int pin ) {
  
  int event = -1;

  int currMillis = millis ();
  int pinVal = digitalRead ( pin );
  
  if ( pinVal == LOW ) {

    int timeLapse = currMillis - buttonPressedTime [ buttonIndex ];
    if ( timeLapse >= BUTTON_HOLD_DELAY ) {
      buttonPressedTime [ buttonIndex ] = 0;
      event = buttonIndex | 2; // 2 for b1h, 3 for b2h
    }
    else {
      if ( timeLapse >= BUTTON_SENSITIVITY_MILLIS ) {
        buttonPressedTime [ buttonIndex ] = 0;
        event = buttonIndex;
      }
    }
  }
  else {
    buttonPressedTime [ buttonIndex ] = currMillis;
  }  

  return event;
}

void buttonStateChange () {

  int b1Event = detectEvent ( BUTTON1_INDEX, BUTTON1_PIN );
  int b2Event = detectEvent ( BUTTON2_INDEX, BUTTON2_PIN );
  
  if ( ( b1Event >= 0 ) && ( b2Event >= 0 ) ) {
    // This is two button event
    if ( b1Event == HOLD_B1 && b2Event == HOLD_B2 ) {
      changeState ( HOLD_2B );
    }
    else {
      changeState ( PRESSED_2B );
    }
  }
  else {
    if ( b1Event >= 0 ) {
      // This is Button 1 event
      changeState ( b1Event );
    }
    else {
      if ( b2Event >= 0 ) {
        // This is Button 2 event
        changeState ( b2Event );
      }
    }
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

/* ------------------------------------------------------------------------------*/
//                 LED functions Start
/* ------------------------------------------------------------------------------*/

/*
            P0         P1        P2        P3
            C0         C1        C2        C3
P4 R0  4L0H(01)  4L1H(02)  4L2H(03)  4L3H(04)
P5 R1  5L0H(05)  5L1H(06)  5L2H(07)  5L3H(08)
P6 R2  6L0H(09)  6L1H(10)  6L2H(11)  6L3H(12)
P7 R3  7L0H(13)  7L1H(14)  7L2H(15)  7L3H(16)

*/

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
  
  float animatorDelay = LED_FLICKER_DELAY;

  int animationLength = 0;
  int startMillis = millis ();
  
  while ( !preemptAnimation && ( animationLength < forMillis ) ) {
    
    for ( int i = 0; i < TOTAL_LED; i ++ ) {
      setLedState ( ledsToAnimate [ i ], HIGH );
      delay ( animatorDelay );
      setLedState ( ledsToAnimate [ i ], LOW );
    }
    
    int currMillis = millis ();
    if ( currMillis < startMillis ) {
      currMillis += 1000;
    }
    
    animationLength = currMillis - startMillis;
  }
}

/* ------------------------------------------------------------------------------*/
//                 LED functions End
/* ------------------------------------------------------------------------------*/

