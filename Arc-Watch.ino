#include <Time.h>
#include <Snooze.h>

// comment the following define statement to shut the built-in LED off
#define USE_DEBUG_LED 1

// Comment the following to skip Serial Port Setup
//#define SOFTWARE_DEBUG 1

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

#define TOTAL_LED_PINS_IN_USE  10 // It is assumed that 0-7 numbered pins are used
const int LED_PINS [ TOTAL_LED_PINS_IN_USE ] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

const int BUTTON_PINS [ TOTAL_BUTTONS ] = { 12, 11, 10 };

volatile int lastButtonInterruptTime [ TOTAL_BUTTONS ] = { -BUTTON_SENSITIVITY_MILLIS };
volatile unsigned int buttonPressedTime [ TOTAL_BUTTONS ] = { 0 };

volatile boolean preemptAnimation = false;
volatile bool isSleeping = false;

// According to library, this has to be global variable, odd though!
SnoozeBlock snoozeConfig;

volatile unsigned int currentState = STATE_SHOW_SECONDS;

// LED related constants
#define TOTAL_LED            16
#define CIRCLE_BOUNDARY       13
#define MAX_LEDS_FOR_ONE_LED  3  // Number of LEDs in a row
#define ROWED_LEDS            3
#define ACTUAL_LED_COUNT      TOTAL_LED + ( ROWED_LEDS * (MAX_LEDS_FOR_ONE_LED - 1) )

const byte ledPins [] = {
  0x00, // LED 'zero' means nothing, it does not exist, but arrays are zero-based indexed
  0x50, 0x51, 0x52, 0x53, 0x54, //  1,  2,  3,  4,  5  |    1,   2,   3,   4,  10a
  0x60, 0x61, 0x62, 0x63, 0x64, //  6,  7,  8,  9, 10  |    5,   6,   7,   8,  10c
  0x70, 0x71, 0x72, 0x73, 0x74, // 11, 12, 13, 14, 15  |    9, 10b, 11b, 12b,  11a
  0x80, 0x81, 0x82, 0x83, 0x84, // 16, 17, 18, 19, 20  |   13,  14,  15,  16,  11c
  0x90, 0x91, 0x92, 0x93, 0x94  // 21, 22, 23, 24, 25  |  12a, 12c
};

typedef struct {
  byte numberOfLeds;
  byte ledIds [ MAX_LEDS_FOR_ONE_LED ];
}  rowedLedConfig;

rowedLedConfig ledConfig [ TOTAL_LED + 1 ];

void initLedConfig () {
  ledConfig [  0 ] = { 0, {  0,  0,  0} };
  ledConfig [  1 ] = { 1, {  1,  1,  1} };
  ledConfig [  2 ] = { 1, {  2,  2,  2} };
  ledConfig [  3 ] = { 1, {  3,  3,  3} };
  ledConfig [  4 ] = { 1, {  4,  4,  4} };
  ledConfig [  5 ] = { 1, {  6,  6,  6} };
  ledConfig [  6 ] = { 1, {  7,  7,  7} };
  ledConfig [  7 ] = { 1, {  8,  8,  8} };
  ledConfig [  8 ] = { 1, {  9,  9,  9} };
  ledConfig [  9 ] = { 1, { 11, 11, 11} };
  ledConfig [ 10 ] = { 3, {  5, 12, 10} };
  ledConfig [ 11 ] = { 3, { 15, 13, 20} };
  ledConfig [ 12 ] = { 3, { 21, 14, 22} };
  ledConfig [ 13 ] = { 1, { 16, 16, 16} };
  ledConfig [ 14 ] = { 1, { 17, 17, 17} };
  ledConfig [ 15 ] = { 1, { 18, 18, 18} };
  ledConfig [ 16 ] = { 1, { 19, 19, 19} };
}

void setup()
{
  setSyncProvider ( getCurrentTime );

  int i;

  // LED Pins
  for ( i = 0; i < TOTAL_LED_PINS_IN_USE; i ++ ) {
    pinMode ( LED_PINS [ i ], OUTPUT );
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
  
  initLedConfig ();
  
#if defined( USE_DEBUG_LED )
  Serial.begin(9600);
  delay(2000);// Give reader a chance to see the output.
#endif

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


#define LED_ID_DIVIDER     5

// 1 second long animation
#define ANIMATION_LENGTH_IN_MILLIS  1000

int hours = 0;
int minutes = 0;
int seconds = 0;
unsigned int previousState = 0xFF;

#if defined( USE_DEBUG_LED )
bool builtInLedState = HIGH;
#endif

void mainRoutine()
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
      animateLed ( seconds / LED_ID_DIVIDER, seconds % LED_ID_DIVIDER );
      break;

    case STATE_SHOW_MINUTES:
      hours = hour () + 1; // because it is in 24Hr format (0-23)
      if ( hours > 12 ) {
        hours = hours - 12;
      }
      animateLed ( hours, 0 );
      break;

    case STATE_SHOW_HOURS:
      minutes = ( minute () + 1 );
      animateLed ( minutes / LED_ID_DIVIDER, minutes % LED_ID_DIVIDER );
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
      animateLed ( TOTAL_LED, 0 );
      break;
  }
  
  previousState = currentState;

}

void loop () {
  mainRoutine ();
}

// ---------------------------------------------------- //
//            LED Config and Methods Start
// ---------------------------------------------------- //

const unsigned int ROW_PIN_MASK = 0xF0; // First Byte of address is Row ID
const unsigned int COL_PIN_MASK = 0x0F; // Second Byte of address is Col ID

void setLedState ( int led, boolean state ) {

  if ( led > 0 && led <= ACTUAL_LED_COUNT ) {
    
    unsigned int address = ledPins [ led ];
    unsigned int rowPin = ( address & ROW_PIN_MASK ) >> 4;
    unsigned int colPin = address & COL_PIN_MASK;
    
    if ( state == HIGH ) {
      digitalWrite ( rowPin, LOW );
      digitalWrite ( colPin, HIGH );
    }
    else {
      digitalWrite ( rowPin, HIGH );
      digitalWrite ( colPin, LOW );
    }
  }
}

void resetAllLeds () {

  for ( int i = 0; i <= ACTUAL_LED_COUNT; i ++ ) {
    setLedState ( i, LOW );
  }
}

#define INNER_CIRCLE_OFFSET  13

void animateLed ( int outerLed, int innerLed ) {

  preemptAnimation = false;
  int ledsToAnimate [ ACTUAL_LED_COUNT + 1 ] = {0};
  int ledIndex = 0;
  int i = 0;
  
  for ( i = 0; i <= outerLed; i ++  ) {
    for ( int j = 0; j < ledConfig [ i ].numberOfLeds; j ++ ) {
      ledsToAnimate [ ledIndex ++ ] = ledConfig [ i ].ledIds [ j ];
    }
  }
  
  for ( i = 0; i < innerLed; i ++ ) {
    for ( int j = 0; j < ledConfig [ INNER_CIRCLE_OFFSET + i ].numberOfLeds; j ++ ) {
      ledsToAnimate [ ledIndex ++ ] = ledConfig [ INNER_CIRCLE_OFFSET + i ].ledIds [ j ];
    }
  }

  resetAllLeds ();
  
  elapsedMillis animationLength = 0;
  
  while ( !preemptAnimation && ( animationLength < ANIMATION_LENGTH_IN_MILLIS ) ) {

    for ( i = 0; i < ledIndex; i ++ ) {
      setLedState ( ledsToAnimate [ i ], HIGH );
      setLedState ( ledsToAnimate [ i ], LOW );
    }
  }
}

// ---------------------------------------------------- //
//            LED Config and Methods End
// ---------------------------------------------------- //

