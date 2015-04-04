#include <Time.h>
#include <Snooze.h>

// comment the following define statement to shut the built-in LED off
#define USE_DEBUG_LED 1

// Number of milli seconds after which watch will go to sleep automatically
#define AUTO_SLEEP_AFTER_MILLI_SECONDS    20000

// Using compiler directives to save runtime memory, could've very well been 'const int' declarations too
#define STATE_DEFAULT           0
#define STATE_SHOW_SECONDS      0
#define STATE_SHOW_MINUTES      1
#define STATE_SHOW_HOURS        2
#define STATE_SHOW_ALL          3
#define STATE_SHUTDOWN          4
#define STATE_SET_TIME_HOURS    5
#define STATE_SET_TIME_MINS     6
#define STATE_SET_TIME_INC      7
#define STATE_SET_TIME_DEC      8
// Make sure that whenever a new state is added, or exisitng one is removed, the count is reflected in TOTAL_STATES
#define TOTAL_STATES            9

#define BUTTON_SENSITIVITY_MILLIS  3
#define BUTTON_HOLD_DELAY          3000

#define BUTTON1        0
#define BUTTON2        1
#define BUTTON3        2
// Alwyas make sure that TOTAL_BUTTONS is defined as number of Buttons
#define TOTAL_BUTTONS  3
// 2 Events per Button - Pressed and Hold
#define EVENTS_PER_BUTTON  2
#define TOTAL_BUTTON_EVENTS  (TOTAL_BUTTONS * EVENTS_PER_BUTTON)

const int BUTTON_PINS [ TOTAL_BUTTONS ] = { 8, 9, 10 };

volatile int lastButtonInterruptTime [ TOTAL_BUTTONS ] = { -BUTTON_SENSITIVITY_MILLIS };
volatile unsigned int buttonPressedTime [ TOTAL_BUTTONS ] = { 0 };

volatile boolean preemptAnimation = false;
volatile bool isSleeping = false;

// According to library, this has to be global variable, odd though!
SnoozeBlock snoozeConfig;

volatile unsigned int currentState = STATE_SHOW_SECONDS;

void setup()
{

  setSyncProvider ( getCurrentTime );

  int i;

  // LED Pins
  for ( i = 0; i < 8; i ++ ) {
    pinMode ( i, OUTPUT );
  }

  // Button Pins
  for ( i = 0; i < TOTAL_BUTTONS; i ++ ) {
    pinMode ( BUTTON_PINS [ i ], INPUT );
  }

  // Debug LED setup
#if defined( USE_DEBUG_LED )
  pinMode ( LED_BUILTIN, OUTPUT );
#endif

  attachInterrupt ( BUTTON_PINS [ BUTTON1 ], pinChanged1, CHANGE );
  attachInterrupt ( BUTTON_PINS [ BUTTON2 ], pinChanged2, CHANGE );
//  attachInterrupt ( BUTTON_PINS [ BUTTON3 ], pinChanged3, CHANGE );

  snoozeConfig.pinMode ( BUTTON_PINS [ BUTTON3 ], INPUT, CHANGE );

  setSyncInterval ( 1 );
}

time_t getCurrentTime()
{
  return Teensy3Clock.get();
}

const int STATE_MACHINE [ TOTAL_STATES ] [ TOTAL_BUTTON_EVENTS ] = {
  //        0                  1                  2                3                4                5
  { STATE_SHOW_MINUTES, STATE_SHUTDOWN, STATE_SHOW_HOURS,   STATE_SHUTDOWN,   STATE_DEFAULT,   STATE_SHUTDOWN }, //  0 - STATE_SHOW_SECONDS
  { STATE_SHOW_SECONDS, STATE_SHUTDOWN, STATE_SHOW_HOURS,   STATE_SHUTDOWN,   STATE_DEFAULT,   STATE_SHUTDOWN }, //  1 - STATE_SHOW_MINUTES
  { STATE_SHOW_MINUTES, STATE_SHUTDOWN, STATE_SHOW_SECONDS, STATE_SHUTDOWN,   STATE_DEFAULT,   STATE_SHUTDOWN }, //  2 - STATE_SHOW_HOURS
  { STATE_SHOW_MINUTES, STATE_SHUTDOWN, STATE_SHOW_SECONDS, STATE_SHUTDOWN,   STATE_DEFAULT,   STATE_SHUTDOWN }, //  3 - STATE_SHOW_ALL
  { STATE_DEFAULT,      STATE_DEFAULT,  STATE_DEFAULT,      STATE_DEFAULT,    STATE_DEFAULT,   STATE_DEFAULT  }, //  4 - STATE_SHUTDOWN
  { STATE_DEFAULT,      STATE_DEFAULT,  STATE_DEFAULT,      STATE_DEFAULT,    STATE_DEFAULT,   STATE_DEFAULT  }, //  5 - STATE_SET_TIME_HOURS
  { STATE_DEFAULT,      STATE_DEFAULT,  STATE_DEFAULT,      STATE_DEFAULT,    STATE_DEFAULT,   STATE_DEFAULT  }, //  6 - STATE_SET_TIME_MINS
  { STATE_DEFAULT,      STATE_DEFAULT,  STATE_DEFAULT,      STATE_DEFAULT,    STATE_DEFAULT,   STATE_DEFAULT  }, //  7 - STATE_SET_TIME_INC
  { STATE_DEFAULT,      STATE_DEFAULT,  STATE_DEFAULT,      STATE_DEFAULT,    STATE_DEFAULT,   STATE_DEFAULT  }  //  8 - STATE_SET_TIME_DEC
};

elapsedMillis millisSinceInterrupt = 0;

void changeState ( int buttonId, int event ) {
//  millisSinceInterrupt = 0;
  currentState = STATE_MACHINE [ currentState ] [ event ];
  preemptAnimation = true;
  buttonPressedTime [ buttonId ] = millisSinceInterrupt;
}

elapsedMillis millisSinceLastActivity = 0;

void executeInterruptRoutine ( int buttonId ) {

  cli ();
  
  int pinVal = digitalRead ( BUTTON_PINS [ buttonId ] );

  if ( pinVal == LOW ) {

//    int currMillis = millis ();
    int timeLapse = millisSinceInterrupt - buttonPressedTime [ buttonId ];

    if ( timeLapse >= BUTTON_HOLD_DELAY ) {
      changeState ( buttonId, ( 2 * buttonId ) + 1 );
    }
    else {
      if ( timeLapse >= BUTTON_SENSITIVITY_MILLIS ) {
        changeState ( buttonId, ( 2 * buttonId ) );
      }
    }
  }
  else {
    buttonPressedTime [ buttonId ] = millis ();
  }

//  millisSinceInterrupt = 0;
  millisSinceLastActivity = 0;

  sei ();
}

void pinChanged1 () {
  executeInterruptRoutine ( BUTTON1 );
}

void pinChanged2 () {
    executeInterruptRoutine ( BUTTON2 );
}

void pinChanged3 () {
    executeInterruptRoutine ( BUTTON3 );
}


#define OUTER_LED_GROUP     5

// 1 second long animation
#define ANIMATION_LENGTH_IN_MILLIS  1000

int loopDelay = 1000;
int hours = 0;
int minutes = 0;
int seconds = 0;
int outerCircleLed = 0;
int innerCircleLed = 0;
int counter = 0;
int previousState = -1;

#if defined( USE_DEBUG_LED )
bool builtInLedState = HIGH;
#endif

void loop()
{

  if ( previousState != currentState ) {
    resetAllLeds ();
  }
  
#if defined( USE_DEBUG_LED )
  builtInLedState = !builtInLedState;
  digitalWrite ( LED_BUILTIN, builtInLedState );
#endif

  if ( millisSinceLastActivity > AUTO_SLEEP_AFTER_MILLI_SECONDS ) {
    currentState = STATE_SHUTDOWN;
  }
  
  switch ( currentState ) {

    case STATE_SHOW_SECONDS:
      seconds = ( second() + 1 );
      outerCircleLed = ( seconds / OUTER_LED_GROUP );
      innerCircleLed = ( seconds % OUTER_LED_GROUP );
      animateLeds ( outerCircleLed, innerCircleLed, ANIMATION_LENGTH_IN_MILLIS );
      break;

    case STATE_SHOW_MINUTES:
      hours = hour () + 1; // because it is in 24Hr format (0-23)
      if ( hours > 12 ) {
        hours = hours - 12;
      }
      outerCircleLed = hours;
      innerCircleLed = 0;
      animateLeds ( outerCircleLed, innerCircleLed, ANIMATION_LENGTH_IN_MILLIS );
      break;

    case STATE_SHOW_HOURS:
      minutes = ( minute () + 1 );
      outerCircleLed = minutes / OUTER_LED_GROUP;
      innerCircleLed = ( minutes % OUTER_LED_GROUP );
      animateLeds ( outerCircleLed, innerCircleLed, ANIMATION_LENGTH_IN_MILLIS );
      break;

    case STATE_SHUTDOWN:
      isSleeping  = true;
      resetAllLeds ();

#if defined( USE_DEBUG_LED )
      digitalWrite ( LED_BUILTIN, LOW );
#endif

      Snooze.deepSleep ( snoozeConfig );

#if defined( USE_DEBUG_LED )
      digitalWrite ( LED_BUILTIN, builtInLedState );
#endif

      isSleeping = false;
      currentState = 0;
      millisSinceLastActivity = 0;
      break;

    case STATE_SHOW_ALL:
      animateLeds ( 12, 4, ANIMATION_LENGTH_IN_MILLIS );
      break;
  }
  
  previousState = currentState;

}

// ---------------------------------------------------- //
//            LED Config and Methods Start
// ---------------------------------------------------- //

// LED related constants
#define TOTAL_LED          16
#define TOTAL_COLS          4
#define TOTAL_ROWS          4
#define LED_FLICKER_DELAY 0.5
#define CIRCLE_BOUNDARY    13

/*
            P0         P1        P2        P3
            C0         C1        C2        C3
P4 R0  4L0H(01)  4L1H(02)  4L2H(03)  4L3H(04)
P5 R1  5L0H(05)  5L1H(06)  5L2H(07)  5L3H(08)
P6 R2  6L0H(09)  6L1H(10)  6L2H(11)  6L3H(12)
P7 R3  7L0H(13)  7L1H(14)  7L2H(15)  7L3H(16)

*/

const int LED_ID_MAPPING [ TOTAL_LED + 1 ] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 13, 14, 15
};

int getRowPin ( int led ) {
  int ledId = LED_ID_MAPPING [ led ];
  return ( ( ledId - 1 ) / TOTAL_COLS ) + TOTAL_COLS;
}

int getColPin ( int led ) {
  int ledId = LED_ID_MAPPING [ led ];
  return ( ( ledId - 1 ) % TOTAL_COLS );
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

  int row = getRowPin ( led );
  int col = getColPin ( led );

  if ( led > 0 ) {
    setState ( row, col, state );
  }
}

void resetAllLeds () {

  for ( int i = 1; i <= TOTAL_LED; i ++ ) {
    int row = getRowPin ( i );
    int col = getColPin ( i );
    setState ( row, col, LOW );
  }
}

void animateLeds ( int outerLedNumber, int innerLedNumber, int forMillis ) {

  preemptAnimation = false;

  int ledsToAnimate [ TOTAL_LED + 1 ] = {0};

  for ( int led = 1; led <= TOTAL_LED; led ++ ) {
    if ( led < CIRCLE_BOUNDARY ) {
      if ( led <= outerLedNumber ) {
        ledsToAnimate [ led ] = led;
      }
    }
    else {
      if ( led < ( CIRCLE_BOUNDARY + innerLedNumber ) ) {
        ledsToAnimate [ led ] = led;
      }
    }
  }

  elapsedMillis animationLength = 0;
  
  while ( !preemptAnimation && ( animationLength < forMillis ) ) {

    for ( int i = 1; i <= TOTAL_LED; i ++ ) {
      setLedState ( ledsToAnimate [ i ], HIGH );
//      delay ( LED_FLICKER_DELAY );
      setLedState ( ledsToAnimate [ i ], LOW );
//      delay ( LED_FLICKER_DELAY );
    }
  }
}

// ---------------------------------------------------- //
//            LED Config and Methods End
// ---------------------------------------------------- //

