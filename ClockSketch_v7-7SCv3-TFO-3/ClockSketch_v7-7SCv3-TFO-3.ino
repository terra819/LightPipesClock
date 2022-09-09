/* -[ClockSketch v7.2]----------------------------------------------------------------------------------------
   https://www.instructables.com/ClockSketch-V7-Part-I/

   pre-configured for:
   Retro 7 Segment Clock v3 - The Final One(s) (3 LEDs/Segment)
   https://www.instructables.com/Retro-7-Segment-Clock-the-Final-Ones/
   https://www.thingiverse.com/thing:5001559

   Arduino UNO/Nano/Pro Mini (AtMega328, 5V, 16 MHz), DS3231 RTC

   May 2022 - Daniel Cikic

   Serial Baud Rates:
   Arduino: 57600
   nodeMCU: 74880
  -------------------------------------------------------------------------------------------------------------- */

// comment below to disable serial in-/output and free some RAM
#define DEBUG

// nodeMCU - uncomment to compile this sketch for nodeMCU 1.0 / ESP8266, make sure to select the proper board
// type inside the IDE! This mode is NOT supported and only experimental!
#define NODEMCU

// useWiFi - enable WiFi support, WPS setup only! If no WPS support is available on a router check settings
// further down, set useWPS to false and enter ssid/password there
#define USEWIFI

// useNTP - enable NTPClient, requires NODEMCU and USEWIFI. This will also enforce AUTODST.
// Configure a ntp server further down below!
#define USENTP

// RTC selection - uncomment the one you're using, comment all others and make sure pin assignemts for
// DS1302 are correct in the parameters section further down!
// #define RTC_DS1302
// #define RTC_DS1307
//#define RTC_DS3231

// autoDST - uncomment to enable automatic DST switching, check Time Change Rules below!
#define AUTODST

// FADING - uncomment to enable fading effects for dots/digits, other parameters further down below
// #define FADING

// autoBrightness - uncomment to enable automatic brightness adjustments by using a photoresistor/LDR
// #define AUTOBRIGHTNESS

// customDisplay - uncomment this to enable displayMyStuff(). It's an example of how to display values
// at specified times, like temperature readouts
// #define CUSTOMDISPLAY

// FastForward will speed up things and advance time, this is only for testing purposes!
// Disables AUTODST, USENTP and USERTC.
// #define FASTFORWARD

// customHelper will start some kind of assistant when adapting this sketch to other led layouts, this
// tests all the steps neccessary to run it on almost any led strip configuration.
// #define CUSTOMHELPER

/* ----------------------------------------------------------------------------------------------------- */

// Use an accelerometer instead of button functions
#define USEGYRO

#include <TimeLib.h> // "Time" by Michael Margolis, used in all configs
#include <EEPROM.h>  // required for reading/saving settings to eeprom

/* Start RTC config/parameters--------------------------------------------------------------------------
   Check pin assignments for DS1302 (SPI), others are I2C (A4/A5 on Arduino by default)
   Currently all types are using the "Rtc by Makuna" library                                             */
#ifdef RTC_DS1302
#include <ThreeWire.h>
#include <RtcDS1302.h>
ThreeWire myWire(7, 6, 8); // IO/DAT, SCLK, CE/RST
RtcDS1302<ThreeWire> Rtc(myWire);
#define RTCTYPE "DS1302"
#define USERTC
#endif

#ifdef RTC_DS1307
#include <Wire.h>
#include <RtcDS1307.h>
RtcDS1307<TwoWire> Rtc(Wire);
#define RTCTYPE "DS1307"
#define USERTC
#endif

#ifdef RTC_DS3231
#include <Wire.h>
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);
#define RTCTYPE "DS3231"
#define USERTC
#endif

#if !defined(USERTC)
#pragma message "No RTC selected, check definitions on top of the sketch!"
#endif
/* End RTC config/parameters---------------------------------------------------------------------------- */

/* Start WiFi config/parameters------------------------------------------------------------------------- */
#ifdef USEWIFI
const bool useWPS = false; // set to false to disable WPS and use credentials below
const char *wifiSSID = "maWhyFhy";
const char *wifiPWD = "5up3r1337r0xX0r!";
#endif
/* End WiFi config/parameters--------------------------------------------------------------------------- */

/* Start NTP config/parameters--------------------------------------------------------------------------
   Using NTP will enforce autoDST, so check autoDST/time zone settings below!                            */
#ifdef USENTP
/* I recommend using a local ntp service (many routers offer them), don't spam public ones with dozens
   of requests a day, get a rtc! ^^                                                                    */
#define NTPHOST "pool.ntp.org"
#ifndef AUTODST
#define AUTODST
#endif
#endif
/* End NTP config/parameters---------------------------------------------------------------------------- */

/* Start autoDST config/parameters ----------------------------------------------------------------------
   Comment/uncomment/add TimeChangeRules as needed, only use 2 (tcr1, tcr2), comment out unused ones!
   Enabling/disabling autoDST will require to set time again, clock will be running in UTC time if autoDST
   is enabled, only display times are adjusted (check serial monitor with DEBUG defined!)
   This will also add options for setting the date (Year/Month/Day) when setting time on the clock!      */
#ifdef AUTODST
#include <Timezone.h> // "Timezone" by Jack Christensen
TimeChangeRule *tcr;
//-----------------------------------------------
/* US */
TimeChangeRule tcr1 = {"tcr1", First, Sun, Nov, 2, -300};  // utc -5h, valid from first sunday of november at 2am
TimeChangeRule tcr2 = {"tcr2", Second, Sun, Mar, 2, -240}; // utc -4h, valid from second sunday of march at 2am
//-----------------------------------------------
/* Europe */
//  TimeChangeRule tcr1 = {"tcr1", Last, Sun, Oct, 3, 60};         // standard/winter time, valid from last sunday of october at 3am, UTC + 1 hour (+60 minutes) (negative value like -300 for utc -5h)
//  TimeChangeRule tcr2 = {"tcr2", Last, Sun, Mar, 2, 120};        // daylight/summer time, valid from last sunday of march at 2am, UTC + 2 hours (+120 minutes)
//-----------------------------------------------
Timezone myTimeZone(tcr1, tcr2);
#endif
/* End autoDST config/parameters ----------------------------------------------------------------------- */

/* Start autoBrightness config/parameters -------------------------------------------------------------- */
uint8_t upperLimitLDR = 180;           // everything above this value will cause max brightness (according to current level) to be used (if it's higher than this)
uint8_t lowerLimitLDR = 50;            // everything below this value will cause minBrightness to be used
uint8_t minBrightness = 30;            // anything below this avgLDR value will be ignored
const bool nightMode = false;          // nightmode true -> if minBrightness is used, colorizeOutput() will use a single color for everything, using HSV
const uint8_t nightColor[2] = {0, 70}; // hue 0 = red, fixed brightness of 70, https://github.com/FastLED/FastLED/wiki/FastLED-HSV-Colors
float factorLDR = 1.0;                 // try 0.5 - 2.0, compensation value for avgLDR. Set dbgLDR true & define DEBUG and watch the serial monitor. Looking...
const bool dbgLDR = false;             // ...for values roughly in the range of 120-160 (medium room light), 40-80 (low light) and 0 - 20 in the dark
#ifdef NODEMCU
uint8_t pinLDR = 0; // LDR connected to A0 (nodeMCU only offers this one)
#else
uint8_t pinLDR = 1;        // LDR connected to A1 (in case somebody flashes this sketch on arduino and already has an ldr connected to A1)
#endif
uint8_t intervalLDR = 75; // read value from LDR every 75ms (most LDRs have a minimum of about 30ms - 50ms)
uint16_t avgLDR = 0;      // we will average this value somehow somewhere in readLDR();
uint16_t lastAvgLDR = 0;  // last average LDR value we got
/* End autoBrightness config/parameters ---------------------------------------------------------------- */

#define SKETCHNAME "ClockSketch v7.2"
#define CLOCKNAME "Retro 7 Segment Clock v3 - The Final One(s), 3 LEDs/segment"

/* Start button config/pins----------------------------------------------------------------------------- */
#ifdef NODEMCU
const uint8_t buttonA = 13; // momentary push button, 1 pin to gnd, 1 pin to d7 / GPIO_13
const uint8_t buttonB = 14; // momentary push button, 1 pin to gnd, 1 pin to d5 / GPIO_14
#else
const uint8_t buttonA = 3; // momentary push button, 1 pin to gnd, 1 pin to d3
const uint8_t buttonB = 4; // momentary push button, 1 pin to gnd, 1 pin to d4
#endif
/* End button config/pins------------------------------------------------------------------------------- */

/* Start Gyro config/pins - ---------------------------------------------------------------------------- */
#ifdef USEGYRO
#include <MPU6050_tockn.h>
#include <Wire.h>
MPU6050 mpu6050(Wire);
int ORIENTATION = 0;
#endif
/* End button config/pins------------------------------------------------------------------------------- */

/* Start basic appearance config------------------------------------------------------------------------ */
const bool dotsBlinking = false;                 // true = only light up dots on even seconds, false = always on
const bool leadingZero = false;                  // true = enable a leading zero, 9:00 -> 09:00, 1:30 -> 01:30...
uint8_t displayMode = 1;                         // 0 = 24h mode, 1 = 12h mode ("1" will also override setting that might be written to EEPROM!)
uint8_t colorMode = 0;                           // different color modes, setting this to anything else than zero will overwrite values written to eeprom, as above
uint16_t colorSpeed = 750;                       // controls how fast colors change, smaller = faster (interval in ms at which color moves inside colorizeOutput();)
const bool colorPreview = true;                  // true = preview selected palette/colorMode using "8" on all positions for 3 seconds
const uint8_t colorPreviewDuration = 3;          // duration in seconds for previewing palettes/colorModes if colorPreview is enabled/true
const bool reverseColorCycling = false;          // true = reverse color movements
const uint8_t brightnessLevels[3]{80, 130, 220}; // 0 - 255, brightness Levels (min, med, max) - index (0-2) will be saved to eeprom
uint8_t brightness = brightnessLevels[0];        // default brightness if none saved to eeprom yet / first run
#ifdef FADING
uint8_t fadeDigits = 2; // fade digit segments, 0 = disabled, 1 = only fade out segments turned off, 2 = fade old out and fade new in
uint8_t fadeDots = 2;   // fade dots, 0 = disabled, 1 = turn dots off without fading in/out after specidfied time, 2 = fade in and out
uint8_t fadeDelay = 15; // milliseconds between each fading step, 5-25 should work okay-ish
#endif
/* End basic appearance config-------------------------------------------------------------------------- */

/* End of basic config/parameters section */

/* End of feature/parameter section, unless changing advanced things/modifying the sketch there's absolutely nothing to do further down! */

/* library, wifi and ntp stuff depending on above config/parameters */
#ifdef NODEMCU
#if defined(USENTP) && !defined(USEWIFI) // enforce USEWIFI when USENTP is defined
#define USEWIFI
#pragma warning "USENTP without USEWIFI, enabling WiFi"
#endif
#ifdef USEWIFI
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif
#endif

#ifdef USENTP
#include <NTPClient.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTPHOST, 0, 60000);
#endif
/* end library stuff */

/* setting feature combinations/options */
#if defined(FASTFORWARD) || defined(CUSTOMHELPER)
bool firstLoop = true;
#ifdef USERTC
#undef USERTC
#endif
#ifdef USEWIFI
#undef USEWIFI
#endif
#ifdef USENTP
#undef USENTP
#endif
#ifdef AUTODST
#undef AUTODST
#endif
#endif
/* setting feature combinations/options */

/* Start of FastLED/clock stuff */
#define LEDSTUFF
#ifdef LEDSTUFF
#ifdef NODEMCU
#define FASTLED_ESP8266_RAW_PIN_ORDER // this means we'll be using the raw esp8266 pin order -> GPIO_12, which is d6 on nodeMCU
#define LED_PIN 12                    // led data in connected to GPIO_12 (d6/nodeMCU)
#else
#define FASTLED_ALLOW_INTERRUPTS 0 // AVR + WS2812 + IRQ = https://github.com/FastLED/FastLED/wiki/Interrupt-problems
#define LED_PIN 6                  // led data in connected to d6 (arduino)
#endif

#define LED_PWR_LIMIT 500 // 500mA - Power limit in mA (voltage is set in setup() to 5v)
#define LED_DIGITS 4      // 4 or 6 digits, HH:MM or HH:MM:SS
#define LED_COUNT 64      // Total number of leds, 103 on Retro 7 Segment Clock v3 - The Final One(s) - 3 LEDs/segment
#if (LED_DIGITS == 6)
#define LED_COUNT 157 // leds on the 6 digit version
#endif

#include <FastLED.h>

uint8_t markerHSV[3] = {0, 127, 20}; // this color will be used to "flag" leds for coloring later on while updating the leds
CRGB leds[LED_COUNT];
CRGBPalette16 currentPalette;
#endif

// start clock specific config/parameters
/* Segment order, seen from the front:

   <  A  >
  /\       /\
  F        B
  \/       \/
   <  G  >
  /\       /\
  E        C
  \/       \/
   <  D  >

  digit positions, seen from the front:
  _   _   _   _   _   _
  |_| |_| |_| |_| |_| |_|
  |_| |_| |_| |_| |_| |_|

  0   1   2   3   4   5

  Note: Digit positions for showSegments() depends on the order in which the segments
  are defined in segGroups[] below. Most of my things/clocks published so far start
  from the right side when seen from the front, TFO from the left. Others may have different
  orders, like Lazy 7 - QBE, which is using a single strip and has an order of
  3, 0, 2, 1 for top left, top right, bottom left, bottom right.

  "The Final One(s)" is starting from the left when looking at the front, so it's
  exactly the reverse order of the old/other ones. This doesn't really matter, that's
  what "digitPositions" a few lines below is for...

*/

/* Below is the configuration for led <> segment assignments.
   LED_ACCESS_MODE 0 will use the two values inside each segment (led a, led b)
   as they are - 2 leds per segment.
   LED_ACCESS_MODE 1 will use the two values inside each segment (led a, led b)
   as start and end value to get 2+ leds/segment.

   Example:
   leds 0, 3 -> MODE 0 -> led 0 and 3 inside the segment -> 2 leds
   leds 0, 3 -> MODE 1 -> led 0 - 3 inside the segment -> 4 leds

   Simply add all the leds into their corresponding segments inside the array.
   The order of digits/strip routing doesn't really matter there, positions of
   HH:MM:SS are assigned using digitPositions.

  digitsLAM -> LED_ACCESS_MODE per digit

*/

// defining access modes for each digit individually
uint8_t digitsLAM[6] = {0, 0, 0, 0, 0, 0};

#if (LED_DIGITS == 4)
const uint8_t digitPositions[4] = {0, 1, 2, 3}; // positions of HH:MM (3, 0, 2, 1 on L7-QBE)
const uint16_t segGroups[28][2] PROGMEM = {
#endif

#if (LED_DIGITS == 6)
    const uint8_t digitPositions[6] = {0, 1, 2, 3, 4, 5}; // positions of HH:MM:SS
const uint16_t segGroups[42][2] PROGMEM = {
#endif

    /* segments 0-27, 4 digits x 7 segments */
    /* digit position 0 */
    {48, 40}, // top, a
    {32, 33}, // top right, b
    {34, 35}, // bottom right, c
    {43, 51}, // bottom, d
    {59, 58}, // bottom left, e
    {57, 56}, // top left, f
    {49, 41}, // center, g
    /* digit position 1 */
    {16, 8},  // top, a
    {0, 1},   // top right, b
    {2, 3},   // bottom right, c
    {11, 19}, // bottom, d
    {27, 26}, // bottom left, e
    {25, 24}, // top left, f
    {17, 9},  // center, g
    /* digit position 2 */
    {52, 44}, // top, a
    {36, 37}, // top right, b
    {38, 39}, // bottom right, c
    {47, 55}, // bottom, d
    {63, 62}, // bottom left, e
    {61, 60}, // top left, f
    {53, 45}, // center, g
    /* digit position 3 */
    {20, 12},         // top, a
    {4, 5},           // top right, b
    {6, 7},           // bottom right, c
    {23, 15},         // bottom, d
    {31, 30},         // bottom left, e
    {28, 29},         // top left, f
    {21, 13},         // center, g
#if (LED_DIGITS == 6) // add two digits, 14 segments, only used if LED_DIGITS is 6...
    /* segments 28-41, 6 digits x 7 segments */
    /* (bogus on some models which don't support 6 digits) */
    /* digit position 4 */
    ,
    {114, 116}, // top, a      !! do not remove the "," at the start of this line !!
    {111, 113}, // top right, b
    {128, 130}, // bottom right, c
    {125, 127}, // bottom, d
    {122, 124}, // bottom left, e
    {117, 119}, // top left, f
    {108, 110}, // center, g
    /* digit position 5 */
    {148, 150}, // top, a
    {145, 147}, // top right, b
    {140, 142}, // bottom right, c
    {137, 139}, // bottom, d
    {134, 136}, // bottom left, e
    {151, 153}, // top left, f
    {154, 156}  // center, g
#endif          // ...end of digits 5+6
};

#if (LED_DIGITS == 4)
const uint16_t upperDots[2] PROGMEM = {50}; // leds inside the upper dots (right on L7-QBE)
const uint16_t lowerDots[2] PROGMEM = {42}; // leds inside the lower dots (left on L7-QBE)
#endif

#if (LED_DIGITS == 6)
const uint16_t upperDots[4] PROGMEM = {49, 50, 103, 104}; // all the leds inside the upper dots (bogus values on some models which don't support 6 digits)
const uint16_t lowerDots[4] PROGMEM = {52, 53, 106, 107}; // all the leds inside the lower dots (bogus values on some models which don't support 6 digits)
#endif

const uint16_t amLight[1] PROGMEM = {54};
const uint16_t pmLight[1] PROGMEM = {14};

// Using above arrays it's very easy to "talk" to the segments. Simply use 0-6 for the first 7 segments, add 7 (7-13) for the second one, 14-20 for third....
const uint8_t digits[21][7] PROGMEM = {
    /* Lets define 10 numbers (0-9) with 7 segments each, also adding some letters
      1 = segment is on, 0 = segment is off   */
    {1, 1, 1, 1, 1, 1, 0}, // 0 -> Show segments a - f, don't show g (center one)
    {0, 1, 1, 0, 0, 0, 0}, // 1 -> Show segments b + c (top right and bottom right), nothing else
    {1, 1, 0, 1, 1, 0, 1}, // 2 -> and so on...
    {1, 1, 1, 1, 0, 0, 1}, // 3
    {0, 1, 1, 0, 0, 1, 1}, // 4
    {1, 0, 1, 1, 0, 1, 1}, // 5
    {1, 0, 1, 1, 1, 1, 1}, // 6
    {1, 1, 1, 0, 0, 0, 0}, // 7
    {1, 1, 1, 1, 1, 1, 1}, // 8
    {1, 1, 1, 1, 0, 1, 1}, // 9
    {0, 0, 0, 1, 1, 1, 1}, // t -> some letters/symbols from here on (index 10-20, so this won't...
    {0, 0, 0, 0, 1, 0, 1}, // r -> ...interfere with using digits 0-9 by using index 0-9
    {0, 1, 1, 1, 0, 1, 1}, // y
    {0, 1, 1, 1, 1, 0, 1}, // d
    {1, 0, 0, 1, 1, 1, 0}, // C
    {1, 0, 0, 0, 1, 1, 1}, // F
    {1, 1, 0, 0, 1, 1, 0}, // some kind of "half letter M" (left half), displayed using two digits
    {1, 1, 1, 0, 0, 1, 0}, // some kind of "half letter M" (right half), displayed using two digits
    {1, 1, 0, 0, 0, 1, 1}, // °
    {0, 1, 1, 0, 1, 1, 1}, // H
    {0, 0, 0, 0, 0, 0, 0}  // "blank"
};

uint8_t clockStatus = 1; // Used for various things, don't mess around with it! 1 = startup
// 0 = regular mode, 1 = startup, 9x = setup modes (90, 91, 92, 93...)

/* these values will be saved to EEPROM:
  0 = index for selected palette
  1 = index for selected brightness level
  2 = displayMode, 12h/24h mode
  3 = colorMode */

/* End of FastLED/clock stuff */
// End clock specific configs/parameters

/* other variables */
uint8_t btnRepeatCounter = 0; // keeps track of how often a button press has been repeated
/* */

// for USEGYRO
int getOrientation()
{
  int y = mpu6050.getAngleY();
  // should be abt 0 if setting upright
  // + or - more than 130 when turned over (abt 170)
  // so if flipped, y should be less than -130, or greater than 130
  return (y < -130 || y > 130) ? 0 : 1; // 1 indicates right side up, 0 upside down
}
void checkFlip()
{
  int currentOrientation = getOrientation();
  // if flipped over, toggle schema
  if (ORIENTATION == 1 && currentOrientation == 0)
  {
    Serial.println("flipped!");
    // toggle pallet
    paletteSwitcher();
  }
  // if flipped back upright, do nothing

  // set new orientation to prevent multiple pallet switches per flip
  ORIENTATION = currentOrientation;
}

/* -- this is where the fun parts start -------------------------------------------------------------------------------------------------------- */

void setup()
{
#ifdef DEBUG
  while (millis() < 300)
  { // safety delay for serial output
#ifdef NODEMCU
    yield();
#endif
  }
#ifdef NODEMCU
  Serial.begin(74880);
  Serial.println(F("  "));
#else
  Serial.begin(57600);
  Serial.println(F("  "));
#endif
#ifdef SKETCHNAME
  Serial.print(SKETCHNAME);
  Serial.println(F(" starting up..."));
#endif
#ifdef CLOCKNAME
  Serial.print("Clock Type: ");
  Serial.println(CLOCKNAME);
#endif
#ifdef RTCTYPE
  Serial.print(F("Configured RTC: "));
  Serial.println(RTCTYPE);
#endif
#ifdef LEDSTUFF
  Serial.print(F("LED power limit: "));
  Serial.print(LED_PWR_LIMIT);
  Serial.println(F(" mA"));
  Serial.print(F("Total LED count: "));
  Serial.println(LED_COUNT);
  Serial.print(F("LED digits: "));
  Serial.println(LED_DIGITS);
#endif
#ifdef AUTODST
  Serial.println(F("autoDST enabled"));
#endif
#ifdef NODEMCU
  Serial.println(F("Configured for nodeMCU"));
#ifdef USEWIFI
  Serial.println(F("WiFi enabled"));
#endif
#ifdef USENTP
  Serial.print(F("NTP enabled, NTPHOST: "));
  Serial.println(NTPHOST);
#endif
#else
  Serial.println(F("Configured for Arduino"));
#endif
#ifdef FASTFORWARD
  Serial.println(F("!! FASTFORWARD defined !!"));
#endif
  while (millis() < 600)
  { // safety delay for serial output
#ifdef NODEMCU
    yield();
#endif
  }
#endif

#ifdef AUTOBRIGHTNESS
#ifdef DEBUG
  Serial.print(F("autoBrightness enabled, LDR using pin: "));
  Serial.println(pinLDR);
#endif
  pinMode(pinLDR, INPUT);
#endif

  pinMode(buttonA, INPUT_PULLUP);
  pinMode(buttonB, INPUT_PULLUP);

#ifdef DEBUG
  if (digitalRead(buttonA) == LOW || digitalRead(buttonB) == LOW)
  {
    if (digitalRead(buttonA) == LOW)
    {
      Serial.println(F("buttonA is LOW / pressed - check wiring!"));
    }
    if (digitalRead(buttonB) == LOW)
    {
      Serial.println(F("buttonB is LOW / pressed - check wiring!"));
    }
  }
#endif

#ifdef LEDSTUFF
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT).setCorrection(TypicalSMD5050).setTemperature(DirectSunlight).setDither(1);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_PWR_LIMIT);
  FastLED.clear();
  FastLED.show();
#ifdef CUSTOMHELPER // customHelper() will run in a loop if defined!
  while (1 > 0)
  {
    customHelper();
  }
#endif
#ifdef DEBUG
  Serial.println(F("setup(): Lighting up some leds..."));
#endif
  for (uint8_t i = 0; i < LED_DIGITS; i++)
  {
    showSegment(6, i);
  }
  FastLED.show();
#endif

#ifdef USEGYRO
  Serial.println(F("Setting up mpu6050..."));
  Wire.begin();
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);
#endif

#ifdef NODEMCU // if building for nodeMCU...
#ifdef USEWIFI // ...and if using WiFi.....
#ifdef DEBUG
  Serial.println(F("Starting up WiFi..."));
#endif
  WiFi.mode(WIFI_STA); // set WiFi mode to STA...
  if (useWPS)
  {
    WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str()); // ...and start connecting using saved credentials...
#ifdef DEBUG
    Serial.println(F("Using WPS setup / saved credentials"));
#endif
  }
  else
  {
    WiFi.begin(wifiSSID, wifiPWD); // ...or credentials defined in the USEWIFI config section
#ifdef DEBUG
    Serial.println(F("Using credentials from sketch"));
#endif
  }
  unsigned long startTimer = millis();
  uint8_t wlStatus = 0;
  uint8_t counter = 6;
#ifdef DEBUG
  Serial.print(F("Waiting for WiFi connection... "));
#endif
  while (wlStatus == 0)
  {
    if (WiFi.status() != WL_CONNECTED)
      wlStatus = 0;
    else
      wlStatus = 1;
#ifdef LEDSTUFF
    if (millis() - startTimer >= 1000)
    {
      FastLED.clear();
      showDigit(counter, digitPositions[3]);
      FastLED.show();
      if (counter > 0)
        counter--;
      else
        wlStatus = 2;
      startTimer = millis();
#ifdef DEBUG
      Serial.print(F("."));
#endif
    }
#endif
#ifdef NODEMCU
    yield();
#endif
  }
  if (WiFi.status() == WL_CONNECTED)
  {                     // if status is connected...
#ifdef USENTP           // ...and USENTP defined...
    timeClient.begin(); // ...start timeClient
#endif
  }
#ifdef DEBUG
  Serial.println();
  if (WiFi.status() != 0)
  {
    Serial.print(F("setup(): Connected to SSID: "));
    Serial.println(WiFi.SSID());
  }
  else
    Serial.println(F("setup(): WiFi connection failed."));
#endif
#endif
  EEPROM.begin(512);
#endif

#ifdef USERTC
  Rtc.Begin();
#ifdef DEBUG
  Serial.println(F("setup(): RTC.begin(), 2 second safety delay before"));
  Serial.println(F("         doing any read/write actions!"));
#endif
  unsigned long tmp_time = millis();
  while (millis() - tmp_time < 2000)
  {
#ifdef NODEMCU
    yield();
#endif
  }
#ifdef DEBUG
  Serial.println(F("setup(): RTC initialized"));
#endif
#else
#ifdef DEBUG
  Serial.println(F("setup(): No RTC defined!"));
#endif
#endif

#ifdef LEDSTUFF
  FastLED.clear();
  FastLED.show();
  /* eeprom settings */
#ifdef nodeMCU
  EEPROM.begin(512);
#endif
  paletteSwitcher();
  brightnessSwitcher();
  colorModeSwitcher();
  displayModeSwitcher();
#endif

#ifdef FASTFORWARD
  setTime(21, 59, 50, 30, 6, 2021); // h, m, s, d, m, y to set the clock to when using FASTFORWARD
#endif

#ifdef USENTP
  syncHelper();
#endif

  clockStatus = 0; // change from 1 (startup) to 0 (running mode)

#ifdef DEBUG
  printTime();
  Serial.println(F("setup() done"));
  Serial.println(F("------------------------------------------------------"));
#endif
}

/* MAIN LOOP */

void loop()
{
  static uint8_t lastInput = 0;                 // != 0 if any button press has been detected
  static uint8_t lastSecondDisplayed = 0;       // This keeps track of the last second when the display was updated (HH:MM and HH:MM:SS)
  static unsigned long lastCheckRTC = millis(); // This will be used to read system time in case no RTC is defined (not supported!)
  static bool doUpdate = false;                 // Update led content whenever something sets this to true. Coloring will always happen at fixed intervals!
#ifdef USEGYRO
  mpu6050.update();
  checkFlip();
#endif
#ifdef USERTC
  static RtcDateTime rtcTime = Rtc.GetDateTime().Epoch32Time(); // Get time from rtc (epoch)
#else
  static time_t sysTime = now(); // if no rtc is defined, get local system time
#endif
#ifdef LEDSTUFF
  static uint8_t refreshDelay = 5;    // refresh leds every 5ms
  static long lastRefresh = millis(); // Keeps track of the last led update/FastLED.show() inside the loop
#ifdef AUTOBRIGHTNESS
  static long lastReadLDR = millis();
#endif
#endif
#ifdef FASTFORWARD
  static unsigned long lastFFStep = millis(); // Keeps track of last time increment if FASTFORWARD is defined
#endif

  if (lastInput != 0)
  { // If any button press is detected...
    if (btnRepeatCounter < 1)
    { // execute short/single press function(s)
#ifdef DEBUG
      Serial.print(F("loop(): "));
      Serial.print(lastInput);
      Serial.println(F(" (short press)"));
#endif
      if (lastInput == 1)
      { // short press button A
#ifdef LEDSTUFF
        brightnessSwitcher();
#endif
      }
      if (lastInput == 2)
      { // short press button B
#ifdef LEDSTUFF
        paletteSwitcher();
#endif
      }
      if (lastInput == 3)
      { // short press button A + button B
      }
    }
    else if (btnRepeatCounter > 8)
    {                       // execute long press function(s)...
      btnRepeatCounter = 1; // ..reset btnRepeatCounter to stop this from repeating
#ifdef DEBUG
      Serial.print(F("loop(): "));
      Serial.print(lastInput);
      Serial.println(F(" (long press)"));
#endif
      if (lastInput == 1)
      { // long press button A
#ifdef LEDSTUFF
        colorModeSwitcher();
#endif
      }
      if (lastInput == 2)
      { // long press button B
#ifdef LEDSTUFF
        displayModeSwitcher();
#endif
      }
      if (lastInput == 3)
      {        // long press button A + button B
#ifdef USEWIFI // if USEWIFI is defined and...
        if (useWPS)
        {               // ...if useWPS is true...
          connectWPS(); // connect WiFi using WPS
        }
#else // if USEWIFI is not defined...
#ifdef LEDSTUFF
        FastLED.clear();
        FastLED.show();
        setupClock(); // start date/time setup
#endif
#endif
      }
      while (digitalRead(buttonA) == LOW || digitalRead(buttonB) == LOW)
      { // wait until buttons are released again
#ifdef LEDSTUFF
        if (millis() % 50 == 0)
        { // Refresh leds every 50ms to give optical feedback
          colorizeOutput(colorMode);
          FastLED.show();
        }
#endif
#ifdef NODEMCU
        yield();
#endif
      }
    }
  }

#ifdef FASTFORWARD // if FASTFORWARD is defined...
  if (millis() - lastFFStep >= 250)
  {                // ...and 250ms have passed...
    adjustTime(5); // ...add 5 seconds to current time
    lastFFStep = millis();
  }
#endif

  if (millis() - lastCheckRTC >= 50)
  { // check rtc/system time every 50ms
#ifdef USERTC
    rtcTime = Rtc.GetDateTime().Epoch32Time();
    if (lastSecondDisplayed != second(rtcTime))
      doUpdate = true;
#else
    sysTime = now();
    if (lastSecondDisplayed != second(sysTime))
      doUpdate = true;
#endif
    lastCheckRTC = millis();
  }

  if (doUpdate)
  { // this will update the led array if doUpdate is true because of a new second from the rtc
#ifdef USERTC
    setTime(rtcTime); // sync system time to rtc every second
#ifdef LEDSTUFF
    FastLED.clear();      // 1A - clear all leds...
    displayTime(rtcTime); // 2A - output rtcTime to the led array..
#endif
    lastSecondDisplayed = second(rtcTime);
#else
#ifdef LEDSTUFF
    FastLED.clear();      // 1B - clear all leds...
    displayTime(sysTime); // 2B - output sysTime to the led array...
#endif
    lastSecondDisplayed = second(sysTime);
#endif
#ifdef CUSTOMDISPLAY
    displayMyStuff(); // 3AB - if customDisplay is defined this will clear the led array again to display custom values...
#endif
    doUpdate = false;
#ifdef DEBUG
    if (second() % 20 == 0)
    {
      printTime();
    }
#endif
#ifdef USENTP // if NTP is enabled, resync to ntp server at 0:00:00 utc
    if (hour() == 0 && minute() == 0 and second() == 0)
    {
      syncHelper();
    }
#endif
  }

#ifdef LEDSTUFF
  colorizeOutput(colorMode); // 1C, 2C, 3C...colorize the data inside the led array right now...
#ifdef AUTOBRIGHTNESS
  if (millis() - lastReadLDR >= intervalLDR)
  {            // if LDR is enabled and sample interval has been reached...
    readLDR(); // ...call readLDR();
    if (abs(avgLDR - lastAvgLDR) >= 5)
    { // if avgLDR has changed for more than +/- 5 update lastAvgLDR
      lastAvgLDR = avgLDR;
      FastLED.setBrightness(avgLDR);
    }
    lastReadLDR = millis();
  }
#endif
#ifdef FADING
  digitsFader();
  dotsFader();
#endif
  if (millis() - lastRefresh >= refreshDelay)
  {
    FastLED.show();
    lastRefresh = millis();
  }
#endif

  lastInput = inputButtons();
}

/* */

#ifdef LEDSTUFF

#ifdef CUSTOMDISPLAY
void displayMyStuff()
{
  /* One way to display custom sensor data/other things. displayMyStuff() is then called inside the doUpdate if statement inside
     void loop() - after updating the leds but before calling colorizeOutput() and FastLED.show()                              */
  if (second() >= 30 && second() < 40)
  {                                                     // only do something if current second is 30-39
#ifdef RTC_DS3231                                       // if DS3231 is used we can read the temperature from that for demo purposes here
    float rtcTemp = Rtc.GetTemperature().AsFloatDegC(); // get temperature in °C as float (25.75°C)....
    uint8_t tmp = round(rtcTemp);                       // ...and round (26°C)
#else
    uint8_t tmp = 99; // get whatever value from whatever sensor into tmp
#endif
    FastLED.clear();
    if (LED_DIGITS == 4)
    {                                         // if 4 digits, display following content:
      showDigit(tmp / 10, digitPositions[0]); // tmp (26°C) / 10 = 2 on position 1 of HH
      showDigit(tmp % 10, digitPositions[1]); // tmp (26°C) % 10 = 6 on position 2 of HH
      showDigit(18, digitPositions[2]);       // ° symbol from array digits[][] on position 1 of MM
      showDigit(14, digitPositions[3]);       // C from array digits[][] on position 2 of MM
    }
    if (LED_DIGITS == 6)
    {                                         // if 6 digits....
      showDigit(tmp / 10, digitPositions[2]); // ...do the above using MM:SS positions instead of HH:MM
      showDigit(tmp % 10, digitPositions[3]);
      showDigit(18, digitPositions[4]);
      showDigit(14, digitPositions[5]);
    }
  }
}
#endif

#ifdef FADING
void fadeSegment(uint8_t pos, uint8_t segment, uint8_t amount, uint8_t fadeType)
{
  /* this will check if the first led of a given segment is lit and if it is, will fade by
     amount using fadeType. fadeType is important because when fading things in that where
     off previously we must avoid setting them black at first - hence fadeLightBy instead
     of fadeToBlack.  */
  uint8_t ledAM = digitsLAM[pos]; // led access mode according to the position
  if (leds[pgm_read_word_near(&segGroups[segment + pos * 7][0])])
  {
    if (ledAM == 0)
    {
      for (uint8_t i = 0; i < 2; i++)
      {
        if (fadeType == 0)
        {
          leds[pgm_read_word_near(&segGroups[segment + pos * 7][i])].fadeToBlackBy(amount);
        }
        else
        {
          leds[pgm_read_word_near(&segGroups[segment + pos * 7][i])].fadeLightBy(amount);
        }
      }
    }
    if (ledAM == 1)
    {
      uint16_t startLed = pgm_read_word_near(&segGroups[segment + pos * 7][0]);
      uint16_t endLed = pgm_read_word_near(&segGroups[segment + pos * 7][1]);
      for (uint16_t i = startLed; i <= endLed; i++)
      {
        if (fadeType == 0)
        {
          leds[i].fadeToBlackBy(amount);
        }
        else
        {
          leds[i].fadeLightBy(amount);
        }
      }
    }
  }
}

void digitsFader()
{
  if (fadeDigits == 0)
    return;
  static unsigned long firstRun = 0;                    // time when a change has been detected and fading starts
  static unsigned long lastRun = 0;                     // used to store time when this function was executed the last time
  static boolean active = false;                        // will be used as a flag when to do something / fade segments
  static uint8_t previousSegments[LED_DIGITS][7] = {0}; // all the segments lit after the last run
  static uint8_t currentSegments[LED_DIGITS][7] = {0};  // all the segments lit right now
  static uint8_t changedSegments[LED_DIGITS][7] = {0};  // used to store the differences -> 1 = led has been turned off, fade out, 2 = was off, fade in
  static uint8_t fadeSteps = 15;                        // steps used to fade dots in or out
  lastRun = millis();
  if (!active)
  { // this will check if....
    firstRun = millis();
    for (uint8_t digitPos = 0; digitPos < LED_DIGITS; digitPos++)
    { // ...any of the segments are on....
      for (uint8_t segmentPos = 0; segmentPos < 7; segmentPos++)
      {
        if (leds[pgm_read_word_near(&segGroups[segmentPos + digitPos * 7][0])])
        {
          currentSegments[digitPos][segmentPos] = 1;
        }
        else
        {
          currentSegments[digitPos][segmentPos] = 0;
        }
        if (currentSegments[digitPos][segmentPos] != previousSegments[digitPos][segmentPos])
        {                // ...and compare them to the previous displayed segments.
          active = true; // if a change has been detected, set active = true so fading gets executed
#ifdef DEBUG
          Serial.print(F("digitPos: "));
          Serial.print(digitPos);
          Serial.print(F(" - segmentPos: "));
          Serial.print(segmentPos);
          Serial.print(F(" was "));
#endif
          if (currentSegments[digitPos][segmentPos] == 0)
          {
            changedSegments[digitPos][segmentPos] = 1;
#ifdef DEBUG
            Serial.println(F("ON, is now OFF"));
#endif
          }
          else
          {
            changedSegments[digitPos][segmentPos] = 2;
#ifdef DEBUG
            Serial.println(F("OFF, is now ON"));
#endif
          }
        }
      }
    }
  }
  if (active)
  { // this part is executed once a change has been detected....
    static uint8_t counter = 1;
    static unsigned long lastFadeStep = millis();
    for (uint8_t digitPos = 0; digitPos < LED_DIGITS; digitPos++)
    { // redraw segments that have turned off, so we can fade them out...
      for (uint8_t segmentPos = 0; segmentPos < 7; segmentPos++)
      {
        if (changedSegments[digitPos][segmentPos] == 1)
        {
          showSegment(segmentPos, digitPos);
        }
      }
    }
    colorizeOutput(colorMode); // colorize again after redraw, so colors keep consistent
    for (uint8_t digitPos = 0; digitPos < LED_DIGITS; digitPos++)
    {
      for (uint8_t segmentPos = 0; segmentPos < 7; segmentPos++)
      {
        if (changedSegments[digitPos][segmentPos] == 1)
        {                                                                      // 1 - segment has turned on, this one has to be faded in
          fadeSegment(digitPos, segmentPos, counter * (255.0 / fadeSteps), 0); // fadeToBlackBy, segments supposed to be off/fading out
        }
        if (changedSegments[digitPos][segmentPos] == 2)
        { // 2 - segment has turned off, this one has to be faded out
          if (fadeDigits == 2)
          {
            fadeSegment(digitPos, segmentPos, 255 - counter * (255.0 / fadeSteps), 1); // fadeLightBy, segments supposed to be on/fading in
          }
        }
      }
    }
    if (millis() - lastFadeStep >= fadeDelay)
    {
      counter++;
      lastFadeStep = millis();
    }
    if (counter > fadeSteps)
    { // done with fading, reset variables...
      counter = 1;
      active = false;
      for (uint8_t digitPos = 0; digitPos < LED_DIGITS; digitPos++)
      { // and save current segments to previousSegments
        for (uint8_t segmentPos = 0; segmentPos < 7; segmentPos++)
        {
          if (leds[pgm_read_word_near(&segGroups[segmentPos + digitPos * 7][0])])
          {
            previousSegments[digitPos][segmentPos] = 1;
          }
          else
          {
            previousSegments[digitPos][segmentPos] = 0;
          }
          changedSegments[digitPos][segmentPos] = 0;
        }
      }
#ifdef DEBUG
      Serial.print(F("digit fading sequence took ")); // for debugging/checking duration - fading should never take longer than 1000ms!
      Serial.print(millis() - firstRun);
      Serial.println(F(" ms"));
#endif
    }
  }
}

void dotsFader()
{
  if (fadeDots == 0)
    return;
  static unsigned long firstRun = 0;
  static unsigned long lastRun = 0;
  static boolean active = false;
  static uint8_t fadeSteps = 15;
  lastRun = millis();
  if (!active)
  {
    if (leds[pgm_read_word_near(&upperDots[0])])
    {
      active = true;
      firstRun = millis();
    }
  }
  if (fadeDots == 1 && active)
  { // action = 1, simply turn off specidifc leds after 500ms
    if (lastRun - firstRun >= 500)
    {
      for (uint8_t i = 0; i < (sizeof(upperDots) / sizeof(upperDots[0])); i++)
      {
        leds[pgm_read_word_near(&upperDots[i])].setHSV(0, 0, 0);
      }
      for (uint8_t i = 0; i < (sizeof(lowerDots) / sizeof(lowerDots[0])); i++)
      {
        leds[pgm_read_word_near(&lowerDots[i])].setHSV(0, 0, 0);
      }
      active = false;
    }
  }
  if (fadeDots == 2 && active)
  { // fade in/out dots
    static uint8_t counter = 1;
    static unsigned long lastFadeStep = millis();
    static boolean fadeInDone = true;
    if (!fadeInDone)
    {
      for (uint8_t i = 0; i < (sizeof(upperDots) / sizeof(upperDots[0])); i++)
      {
        leds[pgm_read_word_near(&upperDots[i])].fadeToBlackBy(255 - counter * (255.0 / fadeSteps));
      }
      for (uint8_t i = 0; i < (sizeof(lowerDots) / sizeof(lowerDots[0])); i++)
      {
        leds[pgm_read_word_near(&lowerDots[i])].fadeToBlackBy(255 - counter * (255.0 / fadeSteps));
      }
      if (millis() - lastFadeStep >= fadeDelay)
      {
        counter++;
        lastFadeStep = millis();
      }
      if (counter > fadeSteps)
      {
        counter = 1;
        fadeInDone = true;
#ifdef DEBUG
        Serial.print(F("dot fade-in sequence took ")); // for debugging/checking
        Serial.print(millis() - firstRun);
        Serial.println(F(" ms"));
#endif
      }
    }
    if (lastRun - firstRun >= 950 - fadeDelay * fadeSteps)
    {
      for (uint8_t i = 0; i < (sizeof(upperDots) / sizeof(upperDots[0])); i++)
      {
        leds[pgm_read_word_near(&upperDots[i])].fadeToBlackBy(counter * (255.0 / fadeSteps));
      }
      for (uint8_t i = 0; i < (sizeof(lowerDots) / sizeof(lowerDots[0])); i++)
      {
        leds[pgm_read_word_near(&lowerDots[i])].fadeToBlackBy(counter * (255.0 / fadeSteps));
      }
      if (millis() - lastFadeStep >= fadeDelay)
      {
        counter++;
        lastFadeStep = millis();
      }
      if (counter > fadeSteps)
      {
        counter = 1;
        active = false;
        fadeInDone = false;
#ifdef DEBUG
        Serial.print(F("dot fading sequence took ")); // for debugging/checking
        Serial.print(millis() - firstRun);
        Serial.println(F(" ms"));
#endif
      }
    }
  }
}
#endif

#ifdef AUTOBRIGHTNESS
void readLDR()
{ // read LDR value 5 times and write average to avgLDR
  static uint8_t runCounter = 1;
  static uint16_t tmp = 0;
  uint8_t readOut = map(analogRead(pinLDR), 0, 1023, 0, 250);
  tmp += readOut;
  if (runCounter == 5)
  {
    avgLDR = (tmp / 5) * factorLDR;
    tmp = 0;
    runCounter = 0;
#ifdef DEBUG
    if (dbgLDR)
    {
      Serial.print(F("readLDR(): avgLDR value: "));
      Serial.print(avgLDR);
    }
#endif
    if (avgLDR < minBrightness)
      avgLDR = minBrightness;
    if (avgLDR > brightness)
      avgLDR = brightness;
    if (avgLDR >= upperLimitLDR && avgLDR < brightness)
      avgLDR = brightness; // if avgLDR is above upperLimitLDR switch to max current brightness
    if (avgLDR <= lowerLimitLDR)
      avgLDR = minBrightness; // if avgLDR is below lowerLimitLDR switch to minBrightness
#ifdef DEBUG
    if (dbgLDR)
    {
      Serial.print(F(" - adjusted to: "));
      Serial.println(avgLDR);
    }
#endif
  }
  runCounter++;
}
#endif

void setupClock()
{
  /* This sets time and date (if AUTODST is defined) on the clock/rtc */
  clockStatus = 90; // clockStatus 9x = setup, relevant for other functions/coloring
  while (digitalRead(buttonA) == LOW || digitalRead(buttonB) == LOW)
  { // do nothing until both buttons are released to avoid accidental inputs right away
#ifdef NODEMCU
    yield();
#endif
  }
  tmElements_t setupTime; // Create a time element which will be used. Using the current time would
  setupTime.Hour = 12;    // give some problems (like time still running while setting hours/minutes)
  setupTime.Minute = 0;   // Setup starts at 12 (12 pm) (utc 12 if AUTODST is defined)
  setupTime.Second = 0;   //
  setupTime.Day = 8;      // date settings only used when AUTODST is defined, but will set them anyways
  setupTime.Month = 7;    // see above
  setupTime.Year = 21;    // current year - 2000 (2021 - 2000 = 21)
#ifdef USERTC
  RtcDateTime writeTime;
#endif
#ifdef AUTODST
  clockStatus = 91; // 91 = y/m/d setup
  uint8_t y, m, d;
  y = getUserInput(12, 20, 21, 99); // show Y + blank, get value from 21 - 99 into y
  setupTime.Year = y + 30;          // 2 digit year + 30 (epoch), so we get offset from 1970
  m = getUserInput(16, 17, 1, 12);  // show M, get value from 1 - 12 into m
  setupTime.Month = m;
  if (m == 2)
  {
    if (leapYear(y + 2000))
    {        // check for leap year...
#ifdef DEBUG // ...and get according day input ranges for each month
      Serial.println(F("setupClock(): Leap year detected"));
#endif
      d = getUserInput(13, 20, 1, 29);
    }
    else
    {
      d = getUserInput(13, 20, 1, 28);
    }
  }
  if (m == 1 || m == 3 || m == 5 || m == 7 || m == 8 || m == 10 || m == 12)
  {
    d = getUserInput(13, 20, 1, 31);
  }
  if (m == 4 || m == 6 || m == 9 || m == 11)
  {
    d = getUserInput(13, 20, 1, 30);
  }
  setupTime.Day = d;
#ifdef USERTC
  writeTime = {2000 + y, setupTime.Month, setupTime.Day,
               setupTime.Hour, setupTime.Minute, setupTime.Second};
  Rtc.SetDateTime(writeTime);
  setTime(makeTime(setupTime));
#ifdef DEBUG
  Serial.println(now());
  Serial.print(F("setupClock(): RTC time/date set to: "));
  Serial.println(writeTime);
#endif
#else
  setTime(makeTime(setupTime));
#endif
#else
  setupTime.Year = 51;
#endif
  uint8_t lastInput = 0;
  // hours
  while (lastInput != 2)
  {
    clockStatus = 92; // 92 = HH setup
    if (lastInput == 1)
    {
      if (setupTime.Hour < 23)
      {
        setupTime.Hour++;
      }
      else
      {
        setupTime.Hour = 0;
      }
    }
    displayTime(makeTime(setupTime));
    lastInput = inputButtons();
  }
  lastInput = 0;
  // minutes
  while (lastInput != 2)
  {
    clockStatus = 93; // 93 = MM setup
    if (lastInput == 1)
    {
      if (setupTime.Minute < 59)
      {
        setupTime.Minute++;
      }
      else
      {
        setupTime.Minute = 0;
      }
    }
    displayTime(makeTime(setupTime));
    lastInput = inputButtons();
  }
  lastInput = 0;
  // seconds
  if (LED_DIGITS == 6)
  {
    while (lastInput != 2)
    {
      clockStatus = 94; // 94 = SS setup
      if (lastInput == 1)
      {
        if (setupTime.Second < 59)
        {
          setupTime.Second++;
        }
        else
        {
          setupTime.Second = 0;
        }
      }
      displayTime(makeTime(setupTime));
      lastInput = inputButtons();
    }
    lastInput = 0;
  }
#ifdef DEBUG
#ifdef AUTODST
  Serial.print(F("setupClock(): "));
  Serial.print(F("Y/M/D -> "));
  Serial.print(1970 + setupTime.Year);
  Serial.print(F("/"));
  Serial.print(setupTime.Month);
  Serial.print(F("/"));
  Serial.println(setupTime.Day);
#endif
  Serial.print(F("setupClock(): "));
  Serial.print(F("HH:MM:SS -> "));
#ifdef AUTODST
  Serial.print(F("AUTODST enabled, setting LOCAL time -> "));
#endif
  if (setupTime.Hour < 10)
    Serial.print(F("0"));
  Serial.print(setupTime.Hour);
  Serial.print(F(":"));
  if (setupTime.Minute < 10)
    Serial.print(F("0"));
  Serial.print(setupTime.Minute);
  Serial.print(F(":"));
  if (setupTime.Second < 10)
    Serial.print(F("0"));
  Serial.println(setupTime.Second);
#endif
#ifdef USERTC
  writeTime = {1970 + setupTime.Year, setupTime.Month, setupTime.Day,
               setupTime.Hour, setupTime.Minute, setupTime.Second};
#ifdef AUTODST
  time_t t = myTimeZone.toUTC(makeTime(setupTime)); // get UTC time from entered time
  writeTime = {1970 + setupTime.Year, month(t), day(t),
               hour(t), minute(t), second(t)};
#endif
  Rtc.SetDateTime(writeTime);
  setTime(makeTime(setupTime));
#ifdef DEBUG
  Serial.println(F("setupClock(): RTC time set"));
  Serial.println(makeTime(setupTime));
  printTime();
#endif
#else
#ifdef AUTODST
  time_t t = myTimeZone.toUTC(makeTime(setupTime)); // get UTC time from entered time
  setTime(t);
#else
  setTime(makeTime(setupTime));
#endif
#endif
  clockStatus = 0;
#ifdef DEBUG
  Serial.println(F("setupClock() done"));
#endif
}

uint16_t getUserInput(uint8_t sym1, uint8_t sym2, uint8_t startVal, uint8_t endVal)
{
  /* This will show two symbols on HH and allow to enter a 2 digit value using the buttons
     and display the value on MM.                                                                                        */
  static uint8_t lastInput = 0;
  static uint8_t currentVal = startVal;
  static bool newInput = true;
  if (newInput)
  {
    currentVal = startVal;
    newInput = false;
  }
  while (lastInput != 2)
  {
    if (lastInput == 1)
    {
      if (currentVal < endVal)
      {
        currentVal++;
      }
      else
      {
        currentVal = startVal;
      }
    }
    FastLED.clear();
    showDigit(sym1, digitPositions[0]);
    showDigit(sym2, digitPositions[1]);
    showDigit(currentVal / 10, digitPositions[2]);
    showDigit(currentVal % 10, digitPositions[3]);
    if (millis() % 30 == 0)
    {
      colorizeOutput(colorMode);
      FastLED.show();
    }
    lastInput = inputButtons();
  }
#ifdef DEBUG
  Serial.print(F("getUserInput(): returned "));
  Serial.println(currentVal);
#endif
  lastInput = 0;
  newInput = true;
  return currentVal;
#ifdef DEBUG
  Serial.print(F("getUserInput(): returned "));
  Serial.println(currentVal);
#endif
}

void colorizeOutput(uint8_t mode)
{
  /* So far showDigit()/showSegment() only set some leds inside the array to values from "markerHSV" but we haven't updated
     the leds yet using FastLED.show(). This function does the coloring of the right now single colored but "insivible"
     output. This way color updates/cycles aren't tied to updating display contents                                      */
  static unsigned long lastRun = 0;
  static unsigned long lastColorChange = 0;
  static uint8_t startColor = 0;
  static uint8_t colorOffset = 0; // different offsets result in quite different results, depending on the amount of leds inside each segment...
  // ...so it's set inside each color mode if required
  /* mode 0 = check every segment if it's lit and assign a color based on position -> different color per digit
     Checking the leds like this will not include the dots - they'll be colored later on                               */
  if (mode == 0)
  {
    colorOffset = 256 / LED_DIGITS;
    for (uint8_t pos = 0; pos < LED_DIGITS; pos++)
    {
      for (uint8_t segment = 0; segment < 7; segment++)
      {
        colorizeSegment(segment, pos, startColor + colorOffset * pos);
      }
    }
  }
  /* mode 1 = simply assign different colors with an offset of "colorOffset" to each led that's not black/off -
     This will include the dots if they're supposed to be on - but they will be overwritten later for all modes        */
  if (mode == 1)
  {
    colorOffset = 32;
    for (uint16_t i = 0; i < LED_COUNT; i++)
    {
      if (leds[i])
      {
        leds[i] = ColorFromPalette(currentPalette, startColor + i * colorOffset, brightness, LINEARBLEND);
      }
    }
  }
  /* mode 2 = check every segment if it's lit and assign a color based on segment -> different color per segment,
     same across digits. Checking the leds like this will not include the dots - they'll be colored later on           */
  if (mode == 2)
  {
    colorOffset = 24;
    for (uint8_t pos = 0; pos < LED_DIGITS; pos++)
    {
      for (uint8_t segment = 0; segment < 7; segment++)
      {
        colorizeSegment(segment, digitPositions[pos], startColor + 1 * colorOffset * segment);
      }
    }
  }
  /* mode 3 = same as above - but will assign colorOffsets depending on segment in a specific order (top/down effect)  */
  if (mode == 3)
  {
    uint8_t colorOffsets[7] = {0, 24, 72, 96, 72, 24, 48}; // colorOffsets for segments a-g
    for (uint8_t pos = 0; pos < LED_DIGITS; pos++)
    {
      for (uint8_t segment = 0; segment < 7; segment++)
      {
        colorizeSegment(segment, digitPositions[pos], startColor + 1 * colorOffsets[segment]);
      }
    }
  }
  /* clockStatus >= 90 is used for coloring output while in setup mode */
  if (clockStatus >= 90)
  {
    static boolean blinkFlag = true;
    static unsigned long lastBlink = millis();
    static uint8_t b = brightnessLevels[0];
    if (millis() - lastBlink > 333)
    { // blink switch frequency, 3 times a second
      if (blinkFlag)
      {
        blinkFlag = false;
        b = brightnessLevels[1];
      }
      else
      {
        blinkFlag = true;
        b = brightnessLevels[0];
      }
      lastBlink = millis();
    } // unset values = red, set value = green, current value = yellow and blinkinkg
    for (uint8_t pos = 0; pos < LED_DIGITS; pos++)
    {
      if (clockStatus == 91)
      { // Y/M/D setup
        colorHelper(digitPositions[0], 0, 255, brightness);
        colorHelper(digitPositions[1], 0, 255, brightness);
        colorHelper(digitPositions[2], 64, 255, b);
        colorHelper(digitPositions[3], 64, 255, b);
      }
      if (clockStatus == 92)
      { // hours
        colorHelper(digitPositions[0], 64, 255, b);
        colorHelper(digitPositions[1], 64, 255, b);
        colorHelper(digitPositions[2], 0, 255, brightness);
        colorHelper(digitPositions[3], 0, 255, brightness);
        if (LED_DIGITS == 6)
        {
          colorHelper(digitPositions[4], 0, 255, brightness);
          colorHelper(digitPositions[5], 0, 255, brightness);
        }
      }
      if (clockStatus == 93)
      { // minutes
        colorHelper(digitPositions[0], 96, 255, brightness);
        colorHelper(digitPositions[1], 96, 255, brightness);
        colorHelper(digitPositions[2], 64, 255, b);
        colorHelper(digitPositions[3], 64, 255, b);
        if (LED_DIGITS == 6)
        {
          colorHelper(digitPositions[4], 0, 255, brightness);
          colorHelper(digitPositions[5], 0, 255, brightness);
        }
      }
      if (clockStatus == 94)
      { // seconds
        colorHelper(digitPositions[0], 96, 255, brightness);
        colorHelper(digitPositions[1], 96, 255, brightness);
        colorHelper(digitPositions[2], 96, 255, brightness);
        colorHelper(digitPositions[3], 96, 255, brightness);
        if (LED_DIGITS == 6)
        {
          colorHelper(digitPositions[4], 64, 255, b);
          colorHelper(digitPositions[5], 64, 255, b);
        }
      }
    }
  }
  /* The dots will always be colored in the same way, just using colors from the current palette. Depending on setup/parameters
     this can otherwise lead to the dots looking quite different from the digits, so as before they're cycling through the
     color palette once per minute */
  if (leds[pgm_read_word_near(&upperDots[0])])
  { // if the first led inside the array upperDot is lit...
    for (uint8_t i = 0; i < (sizeof(upperDots) / sizeof(upperDots[0])); i++)
    { // ...start applying colors to all leds inside the array
      if (clockStatus == 0)
      {
        leds[pgm_read_word_near(&upperDots[i])] = ColorFromPalette(currentPalette, second() * 4.25, brightness, LINEARBLEND);
      }
      else
      {
        leds[pgm_read_word_near(&upperDots[i])].setHSV(64, 255, brightness);
      }
    }
  }
  if (leds[pgm_read_word_near(&lowerDots[0])])
  { // same as before for the lower dots...
    for (uint8_t i = (sizeof(lowerDots) / sizeof(lowerDots[0])); i > 0; i--)
    {
      if (clockStatus == 0)
      {
        leds[pgm_read_word_near(&lowerDots[i - 1])] = ColorFromPalette(currentPalette, second() * 4.25, brightness, LINEARBLEND);
      }
      else
      {
        leds[pgm_read_word_near(&lowerDots[i - 1])].setHSV(64, 255, brightness);
      }
    }
  }
#ifdef FASTFORWARD
  if (millis() - lastColorChange > 15)
  {
#else
  if (millis() - lastColorChange > colorSpeed)
  {
#endif
    if (reverseColorCycling)
    {
      startColor--;
    }
    else
    {
      startColor++;
    }
    lastColorChange = millis();
  }
#ifdef AUTOBRIGHTNESS
  if (nightMode && clockStatus == 0)
  { // nightmode will overwrite everything that has happened so far...
    for (uint16_t i = 0; i < LED_COUNT; i++)
    {
      if (leds[i])
      {
        if (avgLDR == minBrightness)
        {
          leds[i].setHSV(nightColor[0], 255, nightColor[1]); // and assign nightColor to all lit leds. Default is a very dark red.
          FastLED.setDither(0);
        }
        else
        {
          FastLED.setDither(1);
        }
      }
    }
  }
#endif

  /*  // example for time based coloring
    // for coloring based on current times the following will get local display time into
    // checkTime if autoDST is defined as the clock is running in utc time then
    #ifdef AUTODST
      time_t checkTime = myTimeZone.toLocal(now());
    #else
      time_t checkTime = now();
    #endif

    // below if-loop simply checks for a given time and colors everything in green/blue accordingly
    if ( hour(checkTime) > 6 && hour(checkTime) <= 22 ) {           // if hour > 6 AND hour <= 22 ---> 07:00 - 22:59
      for ( uint16_t i = 0; i < LED_COUNT; i++ ) {                  // for each position...
        if ( leds[i] ) {                                            // ...check led and if it's lit...
          leds[i].setHSV(96, 255, brightness);                      // ...redraw with HSV color 96 -> green
        }
      }
    } else {                                                        // ---> 23:00 - 06:59
      for ( uint16_t i = 0; i < LED_COUNT; i++ ) {                  // for each position...
        if ( leds[i] ) {                                            // ...check led and if it's lit...
          leds[i].setHSV(160, 255, brightness);                     // ...redraw with HSV color 160 -> blue
        }
      }
    }
  */

  lastRun = millis();
}

void colorizeSegment(uint8_t segment, uint8_t pos, uint8_t color)
{
  /* Checks if segment at position is on - and if it is, assigns color from current palette  */
  uint8_t ledAM = digitsLAM[pos]; // led access mode according to the position
  if (leds[pgm_read_word_near(&segGroups[segment + digitPositions[pos] * 7][0])])
  {
    if (ledAM == 0)
    {
      for (uint8_t i = 0; i < 2; i++)
      {
        leds[pgm_read_word_near(&segGroups[segment + digitPositions[pos] * 7][i])] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
      }
    }
    if (ledAM == 1)
    {
      uint16_t startLed = pgm_read_word_near(&segGroups[segment + digitPositions[pos] * 7][0]);
      uint16_t endLed = pgm_read_word_near(&segGroups[segment + digitPositions[pos] * 7][1]);
      for (uint16_t i = startLed; i <= endLed; i++)
      {
        leds[i] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
      }
    }
  }
}

void colorHelper(uint8_t pos, uint8_t hue, uint8_t sat, uint8_t bri)
{
  /* Used for coloring digits inside setup routines/steps
     It will simply set the digit at the given position to the given hsv values */
  uint8_t ledAM = digitsLAM[pos]; // led access mode according to the position
  for (uint8_t segment = 0; segment < 7; segment++)
  {
    if (leds[pgm_read_word_near(&segGroups[segment + pos * 7][0])])
    { // if first led inside segment is lit...
      if (ledAM == 0)
      {
        for (uint8_t i = 0; i < 2; i++)
        { // assign hue to led 0 + 1 inside segment
          leds[pgm_read_word_near(&segGroups[segment + pos * 7][i])].setHSV(hue, sat, bri);
        }
      }
      if (ledAM == 1)
      {
        uint16_t startLed = pgm_read_word_near(&segGroups[segment + pos * 7][0]);
        uint16_t endLed = pgm_read_word_near(&segGroups[segment + pos * 7][1]);
        for (uint16_t i = startLed; i <= endLed; i++)
        { // assign hue to led 0 - 1 inside segment
          leds[i].setHSV(hue, sat, bri);
        }
      }
    }
  }
}

void displayTime(time_t t)
{
#ifdef AUTODST
  if (clockStatus < 90)
  {                            // display adjusted times only while NOT in setup
    t = myTimeZone.toLocal(t); // convert display time to local time zone according to rules on top of the sketch
  }
#endif
  if (clockStatus >= 90)
  {
    FastLED.clear();
  }
  /* hours */
  if (displayMode == 0)
  {
    if (hour(t) < 10)
    {
      if (leadingZero)
      {
        showDigit(0, digitPositions[0]);
      }
    }
    else
    {
      showDigit(hour(t) / 10, digitPositions[0]);
    }
    showDigit(hour(t) % 10, digitPositions[1]);
  }
  else if (displayMode == 1)
  {
    if (hourFormat12(t) < 10)
    {
      if (leadingZero)
      {
        showDigit(0, digitPositions[0]);
      }
    }
    else
    {
      showDigit(hourFormat12(t) / 10, digitPositions[0]);
    }
    showDigit(hourFormat12(t) % 10, digitPositions[1]);
  }
  /* minutes */
  showDigit(minute(t) / 10, digitPositions[2]);
  showDigit(minute(t) % 10, digitPositions[3]);
  if (LED_DIGITS == 6)
  {
    /* seconds */
    showDigit(second(t) / 10, digitPositions[4]);
    showDigit(second(t) % 10, digitPositions[5]);
  }
  if (clockStatus >= 90)
  { // in setup modes displayTime will also use colorizeOutput/FastLED.show!
    static unsigned long lastRefresh = millis();
    if (isAM(t) && displayMode == 1)
    { // in 12h mode and if it's AM only light up the upper dots (while setting time)
      showDots(1);
    }
    else
    {
      showDots(2);
    }
    showAmPm(isAM(t));
    if (millis() - lastRefresh >= 25)
    {
      colorizeOutput(colorMode);
      FastLED.show();
      lastRefresh = millis();
    }
    return;
  }
  /* dots */
  if (dotsBlinking)
  {
    if (second(t) % 2 == 0)
    {
      showDots(2);
    }
  }
  else
  {
    showDots(2);
  }
}

void showSegment(uint8_t segment, uint8_t segDisplay)
{
  // This shows the segments from top of the sketch on a given position (segDisplay). Order of positions/segDisplay is the order
  // of definitions on the top, first one defined is segDisplay 0, second one is segDisplay 1 and so on...
  // "firstLoop" is used to display all information only once per test if customHelper is defined
  uint8_t ledAM = digitsLAM[segDisplay]; // led access mode according to the position
#ifdef DEBUG
#ifdef CUSTOMHELPER
  if (firstLoop)
  {
    Serial.print(F("LED_ACCESS_MODE for segment "));
    Serial.print(segment);
    Serial.print(F(" at position "));
    Serial.print(segDisplay);
    Serial.print(F(" is "));
    Serial.print(ledAM);
  }
#endif
#endif
  if (ledAM == 0)
  { // using both values inside the array to light up two leds
#ifdef DEBUG
#ifdef CUSTOMHELPER
    if (firstLoop)
    {
      Serial.print(F(". Leds "));
    }
#endif
#endif
    segment += segDisplay * 7;
    for (uint8_t i = 0; i < 2; i++)
    {
      leds[pgm_read_word_near(&segGroups[segment][i])].setHSV(markerHSV[0], markerHSV[1], markerHSV[2]);
#ifdef DEBUG
#ifdef CUSTOMHELPER
      if (firstLoop)
      {
        if (i == 0)
        {
          Serial.print(pgm_read_word_near(&segGroups[segment][i]));
          Serial.print(F(" and "));
        }
        if (i == 1)
        {
          Serial.println(pgm_read_word_near(&segGroups[segment][i]));
        }
      }
#endif
#endif
    }
  }
  if (ledAM == 1)
  { // using both values inside the array as start and end to light up multiple leds
    segment += segDisplay * 7;
    uint16_t startLed = pgm_read_word_near(&segGroups[segment][0]);
    uint16_t endLed = pgm_read_word_near(&segGroups[segment][1]);
#ifdef DEBUG
#ifdef CUSTOMHELPER
    if (firstLoop)
    {
      Serial.print(F(". Leds "));
      Serial.print(startLed);
      Serial.print(F(" - "));
      Serial.println(endLed);
    }
#endif
#endif
    for (uint16_t i = startLed; i <= endLed; i++)
    {
      leds[i].setHSV(markerHSV[0], markerHSV[1], markerHSV[2]);
    }
  }
}

void showDots(uint8_t dots)
{
  //  // dots 0 = upper dots, dots 1 = lower dots, dots 2 = all dots (right/left/both on Lazy 7 - Quick Build Edition)
  //  if ( dots == 1  || dots == 2 ) {
  //    for ( uint8_t i = 0; i < ( sizeof(upperDots) / sizeof(upperDots[0]) ); i++ ) {
  //      leds[pgm_read_word_near(&upperDots[i])].setHSV(markerHSV[0], markerHSV[1], markerHSV[2]);
  //    }
  //  }
  //  if ( dots == 0 || dots == 2 ) {
  //    for ( uint8_t i = 0; i < ( sizeof(lowerDots) / sizeof(lowerDots[0]) ); i++ ) {
  //      leds[pgm_read_word_near(&lowerDots[i])].setHSV(markerHSV[0], markerHSV[1], markerHSV[2]);
  //    }
  //  }
}

void showAmPm(bool amFlag)
{
  if (amFlag == false)
  {
    leds[pgm_read_word_near(&pmLight[0])].setHSV(160, 255, brightness);
  }
  else
  {
    leds[pgm_read_word_near(&amLight[0])].setHSV(64, 255, brightness);
  }
}

void showDigit(uint8_t digit, uint8_t pos)
{
  // This draws numbers using the according segments as defined on top of the sketch (0 - 9) or symbols/characters (index 10+)
  for (uint8_t i = 0; i < 7; i++)
  {
    if (pgm_read_byte_near(&digits[digit][i]) != 0)
      showSegment(i, pos);
  }
}

void paletteSwitcher()
{
  /* As the name suggests this takes care of switching palettes. When adding palettes, make sure paletteCount increases
    accordingly.  A few examples of gradients/solid colors by using RGB values or HTML Color Codes  below               */
  static uint8_t paletteCount = 6;
  static uint8_t currentIndex = 0;
  if (clockStatus == 1)
  { // Clock is starting up, so load selected palette from eeprom...
    uint8_t tmp = EEPROM.read(0);
    if (tmp >= 0 && tmp < paletteCount)
    {
      currentIndex = tmp; // 255 from eeprom would mean there's nothing been written yet, so checking range...
    }
    else
    {
      currentIndex = 0; // ...and default to 0 if returned value from eeprom is not 0 - 6
    }
#ifdef DEBUG
    Serial.print(F("paletteSwitcher(): loaded EEPROM value "));
    Serial.println(tmp);
#endif
  }
  switch (currentIndex)
  {
  case 0:
    currentPalette = CRGBPalette16(CRGB(224, 0, 32),
                                   CRGB(0, 0, 244),
                                   CRGB(128, 0, 128),
                                   CRGB(224, 0, 64));
    break;
  case 1:
    currentPalette = CRGBPalette16(CRGB(224, 16, 0),
                                   CRGB(192, 64, 0),
                                   CRGB(192, 128, 0),
                                   CRGB(240, 40, 0));
    break;
  case 2:
    currentPalette = CRGBPalette16(CRGB::Aquamarine,
                                   CRGB::Turquoise,
                                   CRGB::Blue,
                                   CRGB::DeepSkyBlue);
    break;
  case 3:
    currentPalette = RainbowColors_p;
    break;
  case 4:
    currentPalette = PartyColors_p;
    break;
  case 5:
    currentPalette = CRGBPalette16(CRGB::LawnGreen);
    break;
  }
#ifdef DEBUG
  Serial.print(F("paletteSwitcher(): selected palette "));
  Serial.println(currentIndex);
#endif
  if (clockStatus == 0)
  { // only save selected palette to eeprom if clock is in normal running mode, not while in startup/setup/whatever
    EEPROM.put(0, currentIndex);
#ifdef NODEMCU
    EEPROM.commit();
#endif
#ifdef DEBUG
    Serial.print(F("paletteSwitcher(): saved index "));
    Serial.print(currentIndex);
    Serial.println(F(" to eeprom"));
#endif
  }
  if (currentIndex < paletteCount - 1)
  {
    currentIndex++;
  }
  else
  {
    currentIndex = 0;
  }
  if (colorPreview)
  {
    previewMode();
  }
#ifdef DEBUG
  Serial.println(F("paletteSwitcher() done"));
#endif
}

void brightnessSwitcher()
{
  static uint8_t currentIndex = 0;
  if (clockStatus == 1)
  { // Clock is starting up, so load selected palette from eeprom...
    uint8_t tmp = EEPROM.read(1);
    if (tmp >= 0 && tmp < 3)
    {
      currentIndex = tmp; // 255 from eeprom would mean there's nothing been written yet, so checking range...
    }
    else
    {
      currentIndex = 0; // ...and default to 0 if returned value from eeprom is not 0 - 2
    }
#ifdef DEBUG
    Serial.print(F("brightnessSwitcher(): loaded EEPROM value "));
    Serial.println(tmp);
#endif
  }
  switch (currentIndex)
  {
  case 0:
    brightness = brightnessLevels[currentIndex];
    break;
  case 1:
    brightness = brightnessLevels[currentIndex];
    break;
  case 2:
    brightness = brightnessLevels[currentIndex];
    break;
  }
#ifdef DEBUG
  Serial.print(F("brightnessSwitcher(): selected brightness index "));
  Serial.println(currentIndex);
#endif
  if (clockStatus == 0)
  { // only save selected brightness to eeprom if clock is in normal running mode, not while in startup/setup/whatever
    EEPROM.put(1, currentIndex);
#ifdef NODEMCU
    EEPROM.commit();
#endif
#ifdef DEBUG
    Serial.print(F("brightnessSwitcher(): saved index "));
    Serial.print(currentIndex);
    Serial.println(F(" to eeprom"));
#endif
  }
  if (currentIndex < 2)
  {
    currentIndex++;
  }
  else
  {
    currentIndex = 0;
  }
#ifdef DEBUG {
  Serial.println(F("brightnessSwitcher() done"));
#endif
}

void colorModeSwitcher()
{
  static uint8_t currentIndex = 0;
  if (clockStatus == 1)
  { // Clock is starting up, so load selected palette from eeprom...
    if (colorMode != 0)
      return; // 0 is default, if it's different on startup the config is set differently, so exit here
    uint8_t tmp = EEPROM.read(3);
    if (tmp >= 0 && tmp < 4)
    {                     // make sure tmp < 3 is increased if color modes are added in colorizeOutput()!
      currentIndex = tmp; // 255 from eeprom would mean there's nothing been written yet, so checking range...
    }
    else
    {
      currentIndex = 0; // ...and default to 0 if returned value from eeprom is not 0 - 2
    }
#ifdef DEBUG
    Serial.print(F("colorModeSwitcher(): loaded EEPROM value "));
    Serial.println(tmp);
#endif
  }
  colorMode = currentIndex;
#ifdef DEBUG
  Serial.print(F("colorModeSwitcher(): selected colorMode "));
  Serial.println(currentIndex);
#endif
  if (clockStatus == 0)
  { // only save selected colorMode to eeprom if clock is in normal running mode, not while in startup/setup/whatever
    EEPROM.put(3, currentIndex);
#ifdef NODEMCU
    EEPROM.commit();
#endif
#ifdef DEBUG
    Serial.print(F("colorModeSwitcher(): saved index "));
    Serial.print(currentIndex);
    Serial.println(F(" to eeprom"));
#endif
  }
  if (currentIndex < 3)
  {
    currentIndex++;
  }
  else
  {
    currentIndex = 0;
  }
  if (colorPreview)
  {
    previewMode();
  }
#ifdef DEBUG {
  Serial.println(F("colorModeSwitcher() done"));
#endif
}

void displayModeSwitcher()
{
  static uint8_t currentIndex = 0;
  if (clockStatus == 1)
  { // Clock is starting up, so load selected palette from eeprom...
    if (displayMode != 0)
      return; // 0 is default, if it's different on startup the config is set differently, so exit here
    uint8_t tmp = EEPROM.read(2);
    if (tmp >= 0 && tmp < 2)
    {                     // make sure tmp < 2 is increased if display modes are added
      currentIndex = tmp; // 255 from eeprom would mean there's nothing been written yet, so checking range...
    }
    else
    {
      currentIndex = 0; // ...and default to 0 if returned value from eeprom is not 0 - 1 (24h/12h mode)
    }
#ifdef DEBUG
    Serial.print(F("displayModeSwitcher(): loaded EEPROM value "));
    Serial.println(tmp);
#endif
  }
  displayMode = currentIndex;
#ifdef DEBUG
  Serial.print(F("displayModeSwitcher(): selected displayMode "));
  Serial.println(currentIndex);
#endif
  if (clockStatus == 0)
  { // only save selected colorMode to eeprom if clock is in normal running mode, not while in startup/setup/whatever
    EEPROM.put(2, currentIndex);
#ifdef NODEMCU
    EEPROM.commit();
#endif
#ifdef DEBUG
    Serial.print(F("displayModeSwitcher(): saved index "));
    Serial.print(currentIndex);
    Serial.println(F(" to eeprom"));
#endif
  }
  if (clockStatus == 0)
  { // show 12h/24h for 2 seconds after selected in normal run mode, don't show this on startup (status 1)
    FastLED.clear();
    unsigned long timer = millis();
    while (millis() - timer <= 2000)
    {
      if (currentIndex == 0)
      {
        showDigit(2, digitPositions[0]);
        showDigit(4, digitPositions[1]);
        showDigit(19, digitPositions[3]);
      }
      if (currentIndex == 1)
      {
        showDigit(1, digitPositions[0]);
        showDigit(2, digitPositions[1]);
        showDigit(19, digitPositions[3]);
      }
      colorizeOutput(colorMode);
      if (millis() % 50 == 0)
      {
        FastLED.show();
      }
#ifdef NODEMCU
      yield();
#endif
    }
  }
  if (currentIndex < 1)
  {
    currentIndex++;
  }
  else
  {
    currentIndex = 0;
  }
#ifdef DEBUG {
  Serial.println(F("displayModeSwitcher() done"));
#endif
}

void previewMode()
{
  /*  This will simply display "8" on all positions, speed up the color cyling and preview the
      selected palette or colorMode                                                            */
  if (clockStatus == 1)
    return; // don't preview when starting up
  unsigned long previewStart = millis();
  uint16_t colorSpeedBak = colorSpeed;
  colorSpeed = 5;
  while (millis() - previewStart <= uint16_t(colorPreviewDuration * 1000L))
  {
    for (uint8_t i = 0; i < LED_DIGITS; i++)
    {
      showDigit(8, i);
    }
    colorizeOutput(colorMode);
    FastLED.show();
#ifdef NODEMCU
    yield();
#endif
  }
  colorSpeed = colorSpeedBak;
  FastLED.clear();
}
#endif /* LEDSTUFF */

bool leapYear(uint16_t y)
{
  boolean isLeapYear = false;
  if (y % 4 == 0)
    isLeapYear = true;
  if (y % 100 == 0 && y % 400 != 0)
    isLeapYear = false;
  if (y % 400 == 0)
    isLeapYear = true;
  if (isLeapYear)
    return true;
  else
    return false;
}

uint8_t inputButtons()
{
  /* This scans for button presses and keeps track of delay/repeat for user inputs
     Short keypresses will only be returned when buttons are released before repeatDelay
     is reached. This is to avoid constantly sending 1 or 2 when executing a long button
     press and/or multiple buttons.
     Note: Buttons are using pinMode INPUT_PULLUP, so HIGH = not pressed, LOW = pressed! */
  static uint8_t scanInterval = 30;            // only check buttons every 30ms
  static uint16_t repeatDelay = 1000;          // delay in milliseconds before repeating detected keypresses
  static uint8_t repeatRate = 1000 / 10;       // 10 chars per 1000 milliseconds
  static uint8_t minTime = scanInterval * 2;   // minimum time to register a button as pressed
  static unsigned long lastReadout = millis(); // keeps track of when the last readout happened
  static unsigned long lastReturn = millis();  // keeps track of when the last readout value was returned
  static uint8_t lastState = 0;                // button state from previous scan
  uint8_t currentState = 0;                    // button state from current scan
  uint8_t retVal = 0;                          // return value, will be 0 if no button is pressed
  static unsigned long eventStart = millis();  // keep track of when button states are changing
  if (millis() - lastReadout < scanInterval)
    return 0; // only scan for button presses every <scanInterval> ms
  if (digitalRead(buttonA) == LOW)
    currentState += 1;
  if (digitalRead(buttonB) == LOW)
    currentState += 2;
  if (currentState == 0 && currentState == lastState)
  {
    btnRepeatCounter = 0;
  }
  if (currentState != 0 && currentState != lastState)
  {                        // if any button is pressed and different from the previous scan...
    eventStart = millis(); // ...reset eventStart to current time
    btnRepeatCounter = 0;  // ...and reset global variable btnRepeatCounter
  }
  if (currentState != 0 && currentState == lastState)
  { // if same input has been detected at least twice (2x scanInterval)...
    if (millis() - eventStart >= repeatDelay)
    { // ...and longer than repeatDelay...
      if (millis() - lastReturn >= repeatRate)
      {                        // ...check for repeatRate...
        retVal = currentState; // ...and set retVal to currentState
        btnRepeatCounter++;
        lastReturn = millis();
      }
      else
        retVal = 0; // return 0 if repeatDelay hasn't been reached yet
    }
  }
  if (currentState == 0 && currentState != lastState && millis() - eventStart >= minTime && btnRepeatCounter == 0)
  {
    retVal = lastState; // return lastState if all buttons are released after having been pressed for <minTime> ms
    btnRepeatCounter = 0;
  }
  lastState = currentState;
  lastReadout = millis();
#ifdef DEBUG // output some information and read serial input, if available
  uint8_t serialInput = dbgInput();
  if (serialInput != 0)
  {
    Serial.print(F("inputButtons(): Serial input detected: "));
    Serial.println(serialInput);
    retVal = serialInput;
  }
  if (retVal != 0)
  {
    Serial.print(F("inputButtons(): Return value is: "));
    Serial.print(retVal);
    Serial.print(F(" - btnRepeatCounter is: "));
    Serial.println(btnRepeatCounter);
  }
#endif
  return retVal;
}

// following will only be included if USENTP is defined
#ifdef USENTP
/* This syncs system time to the RTC at startup and will periodically do other sync related
   things, like syncing rtc to ntp time */
void syncHelper()
{
  static unsigned long lastSync = millis(); // keeps track of the last time a sync attempt has been made
  if (millis() - lastSync < 60000 && clockStatus != 1)
    return; // only allow one ntp request per minute
  if (WiFi.status() != WL_CONNECTED)
  {
#ifdef DEBUG
    Serial.println(F("syncHelper(): No WiFi connection"));
    return;
#endif
  }
#ifndef USERTC
#ifndef USENTP
#ifdef DEBUG
  Serial.println(F("syncHelper(): No RTC and no NTP configured, nothing to do..."));
  return;
#endif
#endif
#endif
  time_t ntpTime = 0;
#ifdef USERTC
  RtcDateTime ntpTimeConverted = ntpTime;
#endif
  if (clockStatus == 1)
  { // looks like the sketch has just started running...
#ifdef DEBUG
    Serial.println(F("syncHelper(): Initial sync on power up..."));
#endif
    ntpTime = getTimeNTP();
#ifdef DEBUG
    Serial.print(F("syncHelper(): NTP result is "));
    Serial.println(ntpTime);
#endif
    lastSync = millis();
  }
  else
  {
#ifdef DEBUG
    Serial.println(F("syncHelper(): Resyncing to NTP..."));
#endif
    ntpTime = getTimeNTP();
#ifdef DEBUG
    Serial.print(F("syncHelper(): NTP result is "));
    Serial.println(ntpTime);
#endif
    lastSync = millis();
  }
#ifdef USERTC
  ntpTimeConverted = {year(ntpTime), month(ntpTime), day(ntpTime),
                      hour(ntpTime), minute(ntpTime), second(ntpTime)};
  RtcDateTime rtcTime = Rtc.GetDateTime(); // get current time from the rtc....
#ifdef DEBUG
  if (ntpTime > 100)
  {
    Rtc.SetDateTime(ntpTimeConverted);
  }
#endif
#else
  time_t sysTime = now(); // ...or from system
#ifdef DEBUG
  Serial.println(F("syncHelper(): No RTC configured, using system time"));
  Serial.print(F("syncHelper(): sysTime was "));
  Serial.println(now());
#endif
  if (ntpTime > 100)
  {
    setTime(ntpTime);
  }
#endif
#ifdef DEBUG
  Serial.println(F("syncHelper() done"));
#endif
}

time_t getTimeNTP()
{
  unsigned long startTime = millis();
  time_t timeNTP;
  if (WiFi.status() != WL_CONNECTED)
  {
#ifdef DEBUG
    Serial.print(F("getTimeNTP(): Not connected, WiFi.status is "));
    Serial.println(WiFi.status());
#endif
  } // Sometimes the connection doesn't work right away although status is WL_CONNECTED...
  while (millis() - startTime < 2000)
  { // ...so we'll wait a moment before causing network traffic
#ifdef NODEMCU
    yield();
#endif
  }
  timeClient.update();
  timeNTP = timeClient.getEpochTime();
  if (timeNTP < 100)
  {
#ifdef DEBUG
    Serial.print(F("getTimeNTP(): NTP returned "));
    Serial.println(timeNTP);
    Serial.print(F(" - trying again..."));
#endif
  }
  timeClient.update();
  timeNTP = timeClient.getEpochTime();
  if (timeNTP < 100)
  {
#ifdef DEBUG
    Serial.print(F("getTimeNTP(): NTP returned "));
    Serial.println(timeNTP);
    Serial.print(F(" - giving up"));
#endif
  }
#ifdef DEBUG
  Serial.println(F("getTimeNTP() done"));
#endif
  return timeNTP;
}
#endif
// ---

// functions below will only be included if DEBUG is defined on top of the sketch
#ifdef DEBUG
void printTime()
{
  /* outputs current system and RTC time to the serial monitor, adds autoDST if defined */
  time_t tmp = now();
#ifdef USERTC
  RtcDateTime tmp2 = Rtc.GetDateTime().Epoch32Time();
  setTime(tmp2);
  tmp = now();
#endif
  Serial.println(F("-----------------------------------"));
  Serial.print(F("System time is : "));
  if (hour(tmp) < 10)
    Serial.print(F("0"));
  Serial.print(hour(tmp));
  Serial.print(F(":"));
  if (minute(tmp) < 10)
    Serial.print(F("0"));
  Serial.print(minute(tmp));
  Serial.print(F(":"));
  if (second(tmp) < 10)
    Serial.print(F("0"));
  Serial.println(second(tmp));
  Serial.print(F("System date is : "));
  Serial.print(year(tmp));
  Serial.print("-");
  Serial.print(month(tmp));
  Serial.print("-");
  Serial.print(day(tmp));
  Serial.println(F(" (Y/M/D)"));
#ifdef USERTC
  Serial.print(F("RTC time is    : "));
  if (hour(tmp2) < 10)
    Serial.print(F("0"));
  Serial.print(hour(tmp2));
  Serial.print(F(":"));
  if (minute(tmp2) < 10)
    Serial.print(F("0"));
  Serial.print(minute(tmp2));
  Serial.print(F(":"));
  if (second(tmp2) < 10)
    Serial.print(F("0"));
  Serial.println(second(tmp2));
  Serial.print(F("RTC date is    : "));
  Serial.print(year(tmp2));
  Serial.print("-");
  Serial.print(month(tmp2));
  Serial.print("-");
  Serial.print(day(tmp2));
  Serial.println(F(" (Y/M/D)"));
#endif
#ifdef AUTODST
  tmp = myTimeZone.toLocal(tmp);
  Serial.print(F("autoDST time is: "));
  if (hour(tmp) < 10)
    Serial.print(F("0"));
  Serial.print(hour(tmp));
  Serial.print(F(":"));
  if (minute(tmp) < 10)
    Serial.print(F("0"));
  Serial.print(minute(tmp));
  Serial.print(F(":"));
  if (second(tmp) < 10)
    Serial.print(F("0"));
  Serial.println(second(tmp));
  Serial.print(F("autoDST date is: "));
  Serial.print(year(tmp));
  Serial.print("-");
  Serial.print(month(tmp));
  Serial.print("-");
  Serial.print(day(tmp));
  Serial.println(F(" (Y/M/D)"));
#endif
  Serial.println(F("-----------------------------------"));
}

uint8_t dbgInput()
{
  /* this catches input from the serial console and hands it over to inputButtons() if DEBUG is defined
     Serial input "7" matches buttonA, "8" matches buttonB, "9" matches buttonA + buttonB */
  if (Serial.available() > 0)
  {
    uint8_t incomingByte = 0;
    incomingByte = Serial.read();
    if (incomingByte == 52)
    { // 4 - long press buttonA
      btnRepeatCounter = 10;
      return 1;
    }
    if (incomingByte == 53)
    { // 5 - long press buttonB
      btnRepeatCounter = 10;
      return 2;
    }
    if (incomingByte == 54)
    { // 6 - long press buttonA + buttonB
      btnRepeatCounter = 10;
      return 3;
    }
    if (incomingByte == 55)
      return 1; // 7 - buttonA
    if (incomingByte == 56)
      return 2; // 8 - buttonB
    if (incomingByte == 57)
      return 3; // 9 - buttonA + buttonB
  }
  return 0;
}
#endif
// ---

#ifdef USEWIFI
void connectWPS()
{ // join network using wps. Will try for 3 times before exiting...
#ifdef DEBUG
  Serial.println(F("connectWPS(): Initializing WPS setup..."));
#endif
  uint8_t counter = 1;
  static unsigned long startTimer = millis();
#ifdef LEDSTUFF
  FastLED.clear();
  showDigit(10, digitPositions[0]);
  showDigit(11, digitPositions[1]);
  showDigit(12, digitPositions[2]);
  showDigit(counter, digitPositions[3]);
  colorizeOutput(colorMode);
  FastLED.show();
#endif
  while (counter < 4)
  {
#ifdef LEDSTUFF
    if (millis() % 50 == 0)
    {
      FastLED.clear();
      showDigit(10, digitPositions[0]);
      showDigit(11, digitPositions[1]);
      showDigit(12, digitPositions[2]);
      showDigit(counter, digitPositions[3]);
      colorizeOutput(colorMode);
      FastLED.show();
    }
#endif
    if (millis() - startTimer > 300)
    {
#ifdef DEBUG
      Serial.print(F("connectWPS(): Waiting for WiFi/WPS, try "));
      Serial.println(counter);
#endif
      WiFi.beginWPSConfig();
      if (WiFi.SSID().length() <= 0)
        counter++;
      else
        counter = 4;
      startTimer = millis();
    }
#ifdef NODEMCU
    yield();
#endif
  }
  FastLED.clear();
  startTimer = millis();
  if (WiFi.SSID().length() > 0)
  {
#ifdef LEDSTUFF
    FastLED.clear();
    showDigit(5, digitPositions[0]);
    showDigit(5, digitPositions[1]);
    showDigit(1, digitPositions[2]);
    showDigit(13, digitPositions[3]);
    colorizeOutput(colorMode);
    FastLED.show();
#endif
#ifdef DEBUG
    Serial.print(F("connectWPS(): Connected to SSID: "));
    Serial.println(WiFi.SSID());
#endif
    while (millis() - startTimer < 2000)
    {
#ifdef NODEMCU
      yield();
#endif
    }
#ifdef USENTP
    clockStatus = 1;
    syncHelper();
    clockStatus = 0;
#endif USENTP
  }
  else
  {
#ifdef DEBUG
    Serial.println(F("connectWPS(): Failed, no WPS connection established"));
#endif
  }
#ifdef DEBUG
  Serial.println(F("connectWPS() done"));
#endif
}
#endif

#ifdef CUSTOMHELPER
/* This assists in troubleshooting and basic configuration. Testing all neccessary steps to get
  showSegment(), showDigit(), showDots() to work. Ugly and using delay() but does the job ^^         */
void customHelper()
{
  markerHSV[0] = 96;
  markerHSV[1] = 255;
  markerHSV[2] = 60;
  colorModeSwitcher();
  paletteSwitcher();
  brightness = 50;
  currentPalette = RainbowColors_p;
  uint8_t test = 1;
#ifdef DEBUG
  Serial.println(F("\n\n\nSome kind of troubleshooting/custom assistant... ^^"));
  Serial.println(F("\nTests will finish before proceeding to the next one.\n"));
  Serial.print(F("The first step is to check all leds, so this test will\nsimply light up all the leds from 0 to "));
  Serial.println(LED_COUNT - 1);
  Serial.println(F("Press button A (or send 7 using serial input) to advance to the next step...\n"));
#endif
  while (test == 1)
  {
    for (uint16_t i = 0; i < LED_COUNT; i++)
    {
      leds[i].setHSV(markerHSV[0], markerHSV[1], markerHSV[2]);
      FastLED.show();
      delay(75);
      if (inputButtons() != 0)
        test++;
    }
    FastLED.clear();
    delay(300);
  }
#ifdef DEBUG
  Serial.println(F("\n\n\nNext we will light up segments 0-6 (a-g) at position 0, this"));
  Serial.println(F("will show if all of the leds inside segArray[][] for position 0 are correct."));
  Serial.println(F("Press button A (or send 7 using serial input) to advance to the next step...\n"));
#endif
  FastLED.clear();
  while (test == 2)
  {
    for (uint8_t i = 0; i < 7; i++)
    {
      showSegment(i, 0);
      FastLED.show();
      delay(750);
      if (inputButtons() != 0)
        test++;
    }
    FastLED.clear();
    FastLED.show();
    delay(300);
    firstLoop = false;
  }
#ifdef DEBUG
  Serial.println(F("\n\nNow let's check this for all the positions as defined (LED_DIGITS 4 or 6), starting from 0..."));
  Serial.println(F("Press button A (or send 7 using serial input) to advance to the next step...\n"));
#endif
  firstLoop = true;
  while (test == 3)
  {
    for (uint8_t pos = 0; pos < LED_DIGITS; pos++)
    {
      for (uint8_t i = 0; i < 7; i++)
      {
        showSegment(i, pos);
        FastLED.show();
        delay(400);
        if (inputButtons() != 0)
          test++;
      }
      if (firstLoop)
        Serial.println();
    }
    FastLED.clear();
    FastLED.show();
    delay(300);
    firstLoop = false;
  }
#ifdef DEBUG
  Serial.println(F("\n\nTesting showDigit() on position 0, displaying 0-9"));
  Serial.println(F("Press button A (or send 7 using serial input) to advance to the next step...\n"));
#endif
  while (test == 4)
  {
    for (uint8_t i = 0; i < 10; i++)
    {
      FastLED.clear();
      showDigit(i, 0);
      if (inputButtons() != 0)
        test++;
      FastLED.show();
      delay(500);
    }
    FastLED.clear();
    FastLED.show();
    delay(300);
  }
#ifdef DEBUG
  Serial.println(F("\n\nTesting showDots() lighting up the upper/lower dots in a repeating pattern..."));
  Serial.println(F("Press button A (or send 7 using serial input) to advance to the next step...\n"));
#endif
  while (test == 5)
  {
    // if ( second() % 2 == 1 ) {
    //   showDots(0);
    // } else {
    //   showDots(1);
    // }
    if (inputButtons() != 0)
      test++;
    FastLED.show();
    delay(20);
    FastLED.clear();
  }
  FastLED.clear();
  FastLED.show();
  delay(300);
#ifdef DEBUG
  Serial.println(F("\n\nFinal test, displaying 0-9 on all positions, using colorizeOutput();"));
  Serial.println(F("Press button A (or send 7 using serial input) to start over...\n"));
#endif
  while (test == 6)
  {
    for (uint8_t i = 0; i < 10; i++)
    {
      for (uint8_t pos = 0; pos < LED_DIGITS; pos++)
      {
        showDigit(i, pos);
      }
      if (inputButtons() != 0)
        test++;
      colorizeOutput(1);
      FastLED.show();
      delay(500);
      FastLED.clear();
    }
  }
  FastLED.clear();
  FastLED.show();
  delay(500);
}
#endif

/* Wooohaa... this one took a bit longer than expected... ^^ /daniel cikic - 07/2021 */
