// Magic Panel FX by IA-PARTS.com 
//
//// Release History
// v011.0 - I2C register interface (docs/i2c-protocol.md): start/stop/status/catalogue/brightness,
//          legacy one-byte commands kept; every animation runs from loop()
// v010.5 - Re-added Working I2C and additional display sequences  (FlthyMcNsty 05-21-2014)
// v010 - Remove I2C code and clean up
// v009 - Combine Big Happy Dude functions to v008 + allow for 3 pin binary input
// v008 - Final Code Release May 24 2013 - D Dobyns
// v007 - New Functions for Program Pin Control - D Dobyns
// v006 - Jumper Pin Program - D Dobyns
// v005 - Cleaned code - Production
// v004 - Added JEDI support for JEDI Serial Address 10 decimal 
// v003 - Decode events... next add selectable jumper support.
// v002 - Added default operation & I2C support
// v001 - Initial Demo Sketch
//
//We always have to include the library
#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
  #else
   #include "WProgram.h"
#endif
#include "LedControl.h"
#include "Wire.h"
#include <avr/eeprom.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////  Assign IC2 Address Below   //////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   byte I2CAdress = 20;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


unsigned long time       	= 0;
unsigned long last_time  	= 0;
byte Speed 			= 1;

byte first_time       = 1;	// used for 4-bit (0 2 3 5) input reading - reset all variables
  			        // when 4-bit address value changes (except the first time on power up)
byte DigInState       = 0;
byte lastDigInState;  //= 0;

// State Variables (Random() state machine; counters of the pattern functions now live in `sq`)

byte RandomState = 0;
byte RandomMode;     // selected mode

unsigned long RandomTime = 0;
unsigned long RandomOnTime = 0;
int RandomInterval = 0;   // per-pass argument of the original Random(int); int on purpose (see mode 7)

/*
To load a sketch onto Magic Panel as Arduino Duemilanove w/ ATmega328

 Now we need a LedControl to work with.
 ***** These pin numbers will probably not work with your hardware *****

7221
Pin 1 - Data IN
Pin 12 - Load
Pin 13 - CLK
Pin 24 - Data Out

Top 7221 = 0
Bottom 7221 =1

Assign the pins from the 328p to LedControl

 pin D8 is connected to the DataIn
 pin D7 is connected to the CLK
 pin D6 is connected to LOAD
 We have two MAX7221 on the Magic Panel, so we use 2.
 */
LedControl lc=LedControl(8,7,6,2);

unsigned long delaytime=30;

boolean VMagicPanel[16][8];  // [Row][Col]; only rows 0-7 are displayed. Rows 8-15 are a spill area:
                             // FadeOutIn sets 16 rows (as the original did), which used to overrun the array
unsigned char MagicPanel[16];
int NumLoops=2;

// Register interface (docs/i2c-protocol.md; values mirror docs/magicpanel_i2c.h)
#define PROTO_MAJOR 1
#define PROTO_MINOR 0
#define FW_MAJOR    0
#define FW_MINOR    11
#define FW_PATCH    0
#define CAPS        0x1F          // legacy, repeat, brightness, names, EEPROM config
#define REG_BIT     0x80
#define MAX_WRITE_DATA 8
#define REG_STATUS             0x10
#define REG_START              0x20
#define REG_STOP               0x21
#define REG_BRIGHTNESS         0x22
#define REG_CONFIG             0x30
#define REG_DEFAULT_BRIGHTNESS 0x31
#define REG_SAVE               0x3F
#define REG_INFO_INDEX         0x40
#define CFG_LEGACY       0x01
#define CFG_GPIO_ENABLE  0x02
#define CFG_GPIO_RESUME  0x04
#define CFG_VALID        0x07
#define CFG_DEFAULT      0x07
#define SAVE_MAGIC       0xA5
#define FACTORY_MAGIC    0x5A
#define EE_MAGIC         'M'
#define EE_LAYOUT        1
enum { ERR_NONE, ERR_UNKNOWN_REG, ERR_READ_ONLY, ERR_BAD_LENGTH, ERR_BAD_VALUE, ERR_LEGACY_OFF, ERR_BAD_MAGIC };
enum { ACT_NONE, ACT_START, ACT_STOP_BLANK, ACT_STOP_FREEZE };

volatile byte regPtr = 0;         // register pointer for reads
volatile byte infoIndex = 0;      // INFO_INDEX
volatile byte lastError = ERR_NONE;
volatile byte errorCount = 0;
volatile byte cfgConfig = CFG_DEFAULT;
volatile byte cfgDefaultBrightness = 15;
byte cfgApplied = CFG_DEFAULT;    // CONFIG as consumeI2C last saw it
volatile byte brightness = 15;
// Actions posted by receiveEvent(); a later start/stop replaces an earlier one not yet carried out.
volatile byte pendAction = ACT_NONE;
volatile byte pendSeq, pendRepeat, pendEnd, pendSource;
volatile bool pendBrightness = false;
volatile byte pendSave = 0;

void setup()
{
  loadConfig();                            // CONFIG and DEFAULT_BRIGHTNESS from EEPROM (factory values if blank)
  brightness = cfgDefaultBrightness;
  Wire.begin(I2CAdress);                   // Start I2C Bus as Slave at I2C Address
  Wire.onReceive(receiveEvent);            // register event so when we receive something we jump to receiveEvent();
  Wire.onRequest(requestEvent);            // register reads (docs/i2c-protocol.md)
  /*
   The MAX72XX is in power-saving mode on startup,
   we have to do a wakeup call
   */
  lc.shutdown(0,false);
  lc.shutdown(1,false);
  /* Set the brightness to a medium values */
  lc.setIntensity(0,brightness);
  lc.setIntensity(1,brightness);

  /* and clear the display */
  lc.clearDisplay(0);
  lc.clearDisplay(1);

  randomSeed(analogRead(A3));           // Randomizer

  // SETUP 6 DIGITAL PINS FOR MANUAL CONTROL

  // Jumpe Pins
  pinMode(11, INPUT);             // set pin 11 to input - input 3
  pinMode(12, OUTPUT);            // set pin PB4 to output - pin 4 - used to allow a jumper from pin 4 to adjacent pin to pull down the adjacent pin
  pinMode(13, INPUT);             // set pin 13 to input - input 5


  digitalWrite(A0, HIGH);         // turn on pullup resistors
  digitalWrite(A1, HIGH);         // turn on pullup resistors
  digitalWrite(A2, HIGH);         // turn on pullup resistors

  digitalWrite(11, HIGH);         // turn on pullup resistors
  digitalWrite(13, HIGH);         // turn on pullup resistors

  digitalWrite(12, LOW);         // set pin PC1 to output - pin 1
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sequence engine
//
// Every animation runs from loop(), one frame at a time; no display work happens in an ISR.
// Each pattern is a stackless coroutine: PAT_DELAY(ms) replaces the original delay(ms), records
// micros() and returns to loop(); the scheduler resumes the pattern right after the PAT_DELAY once
// micros() - start >= ms*1000 (the same exit condition as the core's delay()). Frames
// (MapBoolGrid + PrintGrid) are clocked out atomically in main context.
//
// Coroutine rule: no local variable may be live across a yield. Loop counters that span a
// PAT_DELAY live in `sq`; loops without a yield inside may use ordinary locals.
//
// Triggers: an I2C start (receiveEvent only validates and posts it) or a debounced change of the
// rotary/jumper code abandons the running sequence at its next yield and starts the new one; the
// same trigger restarts it. GPIO modes loop until the next trigger; an I2C sequence that ends while
// a GPIO mode is selected resumes that mode from its start. The I2C register interface is
// specified in docs/i2c-protocol.md; its constants mirror docs/magicpanel_i2c.h.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CO_CAT2(a, b) a##b
#define CO_CAT(a, b)  CO_CAT2(a, b)
#define CO_BEGIN(pcv) do { if (pcv) goto *(pcv); } while (0)
#define CO_YIELD(pcv) do { (pcv) = &&CO_CAT(co_resume_, __LINE__); return true; CO_CAT(co_resume_, __LINE__): ; } while (0)
#define CO_END(pcv)   do { (pcv) = 0; return false; } while (0)

// Scheduler compensation: the deadline is polled from loop() and the pattern is resumed through
// two coroutine levels, which lands each frame on average ~8 us later than the original's
// delay() did. Ending the wait this much earlier keeps the frame timing within the tolerance
// recorded in docs/decisions.md (D-20). micros() has 4 us resolution.
#ifndef SCHED_COMP_US
#define SCHED_COMP_US 8
#endif

enum { WAIT_NONE, WAIT_US, WAIT_PASS };
byte waitKind = WAIT_NONE;
unsigned long waitStart = 0;
unsigned long waitUs = 0;
unsigned int passCount = 0;      // incremented on every Speed-gated loop pass
unsigned int waitPassMark = 0;

void* patPC  = 0;                // resume point of the running pattern
void* progPC = 0;                // resume point of the running program
struct { int r; int i; int j; } sq;   // loop counters of the running pattern

#define PAT_DELAY(ms)    do { waitStart = micros(); waitUs = (unsigned long)(ms) * 1000UL - SCHED_COMP_US; waitKind = WAIT_US; CO_YIELD(patPC); } while (0)
#define PROG_WAIT_PASS() do { waitKind = WAIT_PASS; waitPassMark = passCount; CO_YIELD(progPC); } while (0)

enum {
  P_NONE, P_EYESCAN, P_CYLONCOL, P_CYLONROW, P_FLASHV, P_FLASHQ, P_FLASHALL, P_ONELOOP, P_TWOLOOP,
  P_FADEOUTIN, P_THETEST, P_ONETEST, P_SYMBOL, P_CROSS, P_ALLONTIMED, P_TRACEDOWN, P_TRACEUP,
  P_TRACELEFT, P_TRACERIGHT, P_RANDOMPIXEL, P_QUADRANT, P_TOGGLE, P_ALERT, P_EXPAND, P_COMPRESS,
  P_MYSYMBOL
};

// How a caller wraps a pattern: allOFF() before, allOFF() after, and whether it sets
// RandomTime = RandomOnTime + 1 afterwards (as every original call site with a pattern did).
#define SQ_PRE  1
#define SQ_POST 2
#define SQ_RT   4
#define SQ_WRAP (SQ_PRE | SQ_POST | SQ_RT)
struct SeqEntry { byte flags; byte pat; int a; int b; };

// I2C command byte -> sequence (transcribed from the original receiveEvent switch).
const SeqEntry I2C_TABLE[40] PROGMEM = {
  { SQ_PRE,           P_NONE,        0,     0 },  //  0 panel off
  { SQ_RT,            P_ALLONTIMED,  0,     0 },  //  1 on "indefinitely" (1000 s)
  { SQ_RT,            P_ALLONTIMED,  2000,  0 },  //  2 on 2 s, then falls through into 3 (no break in the original)
  { SQ_RT,            P_ALLONTIMED,  5000,  0 },  //  3 on 5 s (stays on: `allOFF;` is a no-op)
  { SQ_RT,            P_ALLONTIMED,  10000, 0 },  //  4 on 10 s
  { SQ_WRAP,          P_TOGGLE,      10,    0 },  //  5
  { SQ_WRAP,          P_ALERT,       8,     0 },  //  6
  { SQ_WRAP,          P_ALERT,       20,    0 },  //  7
  { SQ_WRAP,          P_TRACEUP,     5,     1 },  //  8
  { SQ_WRAP,          P_TRACEUP,     5,     2 },  //  9
  { SQ_WRAP,          P_TRACEDOWN,   5,     1 },  // 10
  { SQ_WRAP,          P_TRACEDOWN,   5,     2 },  // 11
  { SQ_WRAP,          P_TRACERIGHT,  5,     1 },  // 12
  { SQ_WRAP,          P_TRACERIGHT,  5,     2 },  // 13
  { SQ_WRAP,          P_TRACELEFT,   5,     1 },  // 14
  { SQ_WRAP,          P_TRACELEFT,   5,     2 },  // 15
  { SQ_WRAP,          P_EXPAND,      5,     1 },  // 16
  { SQ_WRAP,          P_EXPAND,      5,     2 },  // 17
  { SQ_WRAP,          P_COMPRESS,    5,     1 },  // 18
  { SQ_WRAP,          P_COMPRESS,    5,     2 },  // 19
  { SQ_WRAP,          P_CROSS,       0,     0 },  // 20
  { SQ_WRAP,          P_CYLONCOL,    2,     140 },// 21
  { SQ_WRAP,          P_CYLONROW,    2,     140 },// 22
  { SQ_POST | SQ_RT,  P_EYESCAN,     2,     100 },// 23
  { SQ_POST | SQ_RT,  P_FADEOUTIN,   1,     0 },  // 24
  { SQ_POST | SQ_RT,  P_FADEOUTIN,   2,     0 },  // 25
  { SQ_POST | SQ_RT,  P_FLASHALL,    8,     200 },// 26
  { SQ_POST | SQ_RT,  P_FLASHV,      8,     200 },// 27
  { SQ_POST | SQ_RT,  P_FLASHQ,      8,     200 },// 28
  { SQ_WRAP,          P_TWOLOOP,     2,     0 },  // 29
  { SQ_WRAP,          P_ONELOOP,     2,     0 },  // 30
  { SQ_WRAP,          P_THETEST,     30,    0 },  // 31
  { SQ_WRAP,          P_ONETEST,     30,    0 },  // 32
  { SQ_WRAP,          P_SYMBOL,      0,     0 },  // 33
  { SQ_WRAP,          P_MYSYMBOL,    0,     0 },  // 34
  { SQ_WRAP,          P_QUADRANT,    5,     1 },  // 35
  { SQ_WRAP,          P_QUADRANT,    5,     2 },  // 36
  { SQ_WRAP,          P_QUADRANT,    5,     3 },  // 37
  { SQ_WRAP,          P_QUADRANT,    5,     4 },  // 38
  { SQ_WRAP,          P_RANDOMPIXEL, 40,    0 },  // 39
};

// Random() mode -> sequence (transcribed from the original Random() nested switch; random(0,35)
// never yields 35, and mode 1 has no case).
const SeqEntry RANDOM_TABLE[36] PROGMEM = {
  { SQ_PRE,           P_NONE,        0,     0 },  //  0 panel off (counts passes)
  { 0,                P_NONE,        0,     0 },  //  1 no case (counts passes)
  { SQ_WRAP,          P_ALLONTIMED,  2000,  0 },  //  2
  { SQ_WRAP,          P_TOGGLE,      10,    0 },  //  3
  { SQ_WRAP,          P_ALERT,       8,     0 },  //  4
  { SQ_WRAP,          P_TRACEUP,     5,     1 },  //  5
  { SQ_WRAP,          P_TRACEUP,     5,     2 },  //  6
  { SQ_WRAP,          P_TRACEDOWN,   5,     1 },  //  7
  { SQ_WRAP,          P_TRACEDOWN,   5,     2 },  //  8
  { SQ_WRAP,          P_EXPAND,      5,     1 },  //  9
  { SQ_WRAP,          P_EXPAND,      5,     2 },  // 10
  { SQ_WRAP,          P_COMPRESS,    5,     1 },  // 11
  { SQ_WRAP,          P_COMPRESS,    5,     2 },  // 12
  { SQ_WRAP,          P_CROSS,       0,     0 },  // 13
  { SQ_WRAP,          P_CYLONCOL,    2,     140 },// 14
  { SQ_WRAP,          P_CYLONROW,    2,     140 },// 15
  { SQ_POST | SQ_RT,  P_EYESCAN,     2,     100 },// 16
  { SQ_POST | SQ_RT,  P_FADEOUTIN,   1,     0 },  // 17
  { SQ_POST | SQ_RT,  P_FADEOUTIN,   2,     0 },  // 18
  { SQ_POST | SQ_RT,  P_FLASHALL,    8,     200 },// 19
  { SQ_POST | SQ_RT,  P_FLASHV,      8,     200 },// 20
  { SQ_POST | SQ_RT,  P_FLASHQ,      8,     200 },// 21
  { SQ_WRAP,          P_TWOLOOP,     2,     0 },  // 22
  { SQ_WRAP,          P_ONELOOP,     2,     0 },  // 23
  { SQ_WRAP,          P_THETEST,     30,    0 },  // 24
  { SQ_WRAP,          P_ONETEST,     30,    0 },  // 25
  { SQ_WRAP,          P_SYMBOL,      0,     0 },  // 26
  { SQ_WRAP,          P_QUADRANT,    5,     1 },  // 27
  { SQ_WRAP,          P_QUADRANT,    5,     2 },  // 28
  { SQ_WRAP,          P_QUADRANT,    5,     3 },  // 29
  { SQ_WRAP,          P_QUADRANT,    5,     4 },  // 30
  { SQ_WRAP,          P_RANDOMPIXEL, 40,    0 },  // 31
  { SQ_WRAP,          P_TRACERIGHT,  5,     1 },  // 32
  { SQ_WRAP,          P_TRACERIGHT,  5,     2 },  // 33
  { SQ_WRAP,          P_TRACELEFT,   5,     1 },  // 34
  { SQ_WRAP,          P_TRACELEFT,   5,     2 },  // 35
};

// Rotary/jumper code -> looping sequence (transcribed from the original loop() switch).
// Codes 6, 7 and 9 run the Random() state machine instead.
const SeqEntry GPIO_TABLE[10] PROGMEM = {
  { 0,                P_NONE,        0,     0 },  // 0 nothing (I2C only)
  { SQ_PRE | SQ_POST, P_FADEOUTIN,   1,     0 },  // 1
  { SQ_PRE | SQ_POST, P_FLASHALL,    8,     200 },// 2
  { SQ_PRE | SQ_POST, P_TWOLOOP,     2,     0 },  // 3
  { SQ_PRE | SQ_POST, P_TRACEDOWN,   5,     1 },  // 4
  { SQ_PRE | SQ_POST, P_ONETEST,     30,    0 },  // 5
  { 0,                P_NONE,        0,     0 },  // 6 Random(random(8000,14000))
  { 0,                P_NONE,        0,     0 },  // 7 Random(random(40000,60000))
  { 0,                P_ALLONTIMED,  0,     0 },  // 8 on for 1000 s, repeating
  { 0,                P_NONE,        0,     0 },  // 9 Random(random(8000,14000))
};

#define SEQ_COUNT        42
#define SEQ_RANDOM_SHOW  40       // Random() with the short off interval (GPIO modes 6 and 9)
#define SEQ_RANDOM_LONG  41       // Random() with the long off interval (GPIO mode 7)
#define SEQ_NONE         0xFF
#define SUB_SEQ_OFF      0xFE

// Catalogue: what an I2C controller reads through INFO_INDEX. Layout = INFO_FLAGS, INFO_LENGTH_MS
// (little-endian, as AVR stores it), INFO_NAME; 21 bytes, no padding on AVR.
#define INFO_LOOPS    0x01
#define INFO_RANDOM   0x02
#define INFO_ENDS_LIT 0x04
#define INFO_HOLD     0x08
#define LEN_INDEFINITE 0xFFFFFFFFUL
struct SeqInfo { byte flags; unsigned long lengthMs; char name[16]; };
const SeqInfo SEQ_INFO[SEQ_COUNT] PROGMEM = {
  { 0,                          7,       "All off" },
  { INFO_ENDS_LIT | INFO_HOLD,  1000015, "On 1000s" },
  { INFO_ENDS_LIT | INFO_HOLD,  7031,    "On 2s+5s" },
  { INFO_ENDS_LIT | INFO_HOLD,  5015,    "On 5s" },
  { INFO_ENDS_LIT | INFO_HOLD,  10016,   "On 10s" },
  { 0,                          10172,   "Toggle" },
  { 0,                          4569,    "Alert" },
  { 0,                          11398,   "Alert long" },
  { 0,                          8359,    "Trace up" },
  { 0,                          8668,    "Trace up line" },
  { 0,                          8359,    "Trace down" },
  { 0,                          8668,    "Trace down line" },
  { 0,                          8365,    "Trace right" },
  { 0,                          8365,    "Trace right line" },
  { 0,                          8365,    "Trace left" },
  { 0,                          8365,    "Trace left line" },
  { 0,                          5213,    "Expand" },
  { 0,                          5213,    "Expand ring" },
  { 0,                          5213,    "Compress" },
  { 0,                          5213,    "Compress ring" },
  { INFO_HOLD,                  3024,    "Cross" },
  { 0,                          4150,    "Cylon column" },
  { 0,                          4150,    "Cylon row" },
  { 0,                          3885,    "Eye scan" },
  { INFO_RANDOM,                4541,    "Fade out/in" },
  { INFO_RANDOM,                2274,    "Fade out" },
  { 0,                          3334,    "Flash all" },
  { 0,                          3337,    "Flash halves" },
  { 0,                          3337,    "Flash quadrants" },
  { 0,                          5122,    "Two loop" },
  { 0,                          5122,    "One loop" },
  { 0,                          4837,    "Test fill" },
  { 0,                          2427,    "Test pixel" },
  { INFO_HOLD,                  3024,    "Symbol AI" },
  { INFO_HOLD,                  4047,    "Symbol 2GWD" },
  { 0,                          4247,    "Quadrant 1" },
  { 0,                          4247,    "Quadrant 2" },
  { 0,                          4323,    "Quadrant 3" },
  { 0,                          4323,    "Quadrant 4" },
  { INFO_RANDOM,                6636,    "Random pixel" },
  { INFO_LOOPS | INFO_RANDOM,   LEN_INDEFINITE,"Random show" },
  { INFO_LOOPS | INFO_RANDOM,   LEN_INDEFINITE,"Random show long" },
};

// Rotary/jumper code -> catalogue ID reported in the status block.
const byte GPIO_SEQ[10] PROGMEM = { SEQ_NONE, 24, 26, 29, 10, 32, SEQ_RANDOM_SHOW, SEQ_RANDOM_LONG, 1, SEQ_RANDOM_SHOW };

// RANDOM_TABLE index -> catalogue ID of the pattern the random show is playing (SUB_SEQ). Mode 1
// draws nothing; mode 2 (on 2 s) reports "On 2s+5s", whose first 2 s it is.
const byte RANDOM_SEQ[36] PROGMEM = {
  0, SUB_SEQ_OFF, 2, 5, 6, 8, 9, 10, 11, 16, 17, 18, 19, 20, 21, 22, 23, 24,
  25, 26, 27, 28, 29, 30, 31, 32, 33, 35, 36, 37, 38, 39, 12, 13, 14, 15
};

enum { PROG_NONE, PROG_I2C, PROG_GPIO };
byte progKind = PROG_NONE;
byte progCode = 0;                // I2C: catalogue ID; GPIO: rotary/jumper code
bool progRandom = false;          // the program is a Random() show
bool progRandomLong = false;      // ... with the long off interval
SeqEntry progEntry;               // entry of the running program (or of the Random() mode)

// Status of the current (or most recent) run, as the status registers report it. Written by the
// main loop with interrupts off, read by requestEvent() in the TWI interrupt.
enum { SRC_NONE, SRC_I2C, SRC_LEGACY, SRC_GPIO, SRC_GPIO_RESUME };
enum { ST_IDLE, ST_RUNNING, ST_COMPLETE, ST_STOPPED };
#define END_DEFAULT 0
#define END_BLANK   1
byte runSeq = SEQ_NONE;
byte runSource = SRC_NONE;
byte runState = ST_IDLE;
byte runRepeat = 1;               // 0 = forever
byte runEnd = END_DEFAULT;
byte runIteration = 0;            // completed iterations, saturating at 255
byte runCounter = 0;
unsigned long runStartMs = 0;
unsigned long runEndMs = 0;
unsigned long iterStartMs = 0;

byte gpioCode = 0;                // accepted rotary/jumper code (0 = no GPIO mode)
#define DEBOUNCE_MS 20            // a new rotary/jumper code must be stable this long to count
byte gpioCandidate = 0;           // code currently being debounced
unsigned long gpioCandidateSince = 0;

volatile byte i2cCount = 0;       // writes received since loop() last looked

byte patId = P_NONE;
int patA = 0;
int patB = 0;

bool isRandomMode(byte code) { return code == 6 || code == 7 || code == 9; }

void clearVGrid() {
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++)
      VMagicPanel[row][col] = false;
}

// Abandon whatever runs and start a new program. The in-memory grid is cleared (not drawn) only
// when a sequence was interrupted, so leftovers of the abandoned sequence cannot bleed into
// patterns that do not start with allOFF(); an idle panel keeps its grid as the original did.
void startProgram(byte kind, byte code) {
  if (progKind != PROG_NONE) clearVGrid();
  progKind = kind;
  progCode = code;
  progPC = 0;
  patPC = 0;
  waitKind = WAIT_NONE;
  if (kind == PROG_I2C) {
    progRandom = code >= SEQ_RANDOM_SHOW;
    progRandomLong = code == SEQ_RANDOM_LONG;
    if (!progRandom) memcpy_P(&progEntry, &I2C_TABLE[code], sizeof(SeqEntry));
  } else {
    progRandom = isRandomMode(code);
    progRandomLong = code == 7;
    memcpy_P(&progEntry, &GPIO_TABLE[code], sizeof(SeqEntry));
  }
}

void beginRun(byte seq, byte source, byte repeat, byte end) {
  noInterrupts();
  runSeq = seq; runSource = source; runRepeat = repeat; runEnd = end;
  runIteration = 0;
  runState = ST_RUNNING;
  runStartMs = iterStartMs = millis();
  runCounter++;
  interrupts();
}

void endRun(byte state) {
  if (runState != ST_RUNNING) return;
  noInterrupts();
  runState = state;
  runEndMs = millis();
  interrupts();
}

void nextIteration() {
  noInterrupts();
  if (runIteration < 255) runIteration++;
  iterStartMs = millis();
  interrupts();
}

void stopProgram() {
  if (progKind != PROG_NONE) clearVGrid();
  progKind = PROG_NONE;
  progPC = 0;
  patPC = 0;
  waitKind = WAIT_NONE;
}

// resume = true when a GPIO mode comes back after an I2C sequence: the Random() modes then
// continue in their off state with a fresh count, as the original's receiveEvent reset RandomTime.
void startGpio(byte code, bool resume) {
  startProgram(PROG_GPIO, code);
  beginRun(pgm_read_byte(&GPIO_SEQ[code]), resume ? SRC_GPIO_RESUME : SRC_GPIO, 0, END_DEFAULT);
  if (isRandomMode(code)) {
    RandomState = resume ? 2 : 0;
    RandomTime = 0;
  }
}

void startPattern(byte id, int a, int b) {
  patId = id; patA = a; patB = b;
  patPC = 0;
  sq.r = 0; sq.i = 0; sq.j = 0;
}

bool runPattern();

// Runs the wrapped pattern of `progEntry`: allOFF() before, the pattern, then the RandomTime
// update and allOFF() after, in the order of the original call sites.
#define PROG_RUN_ENTRY()                                                        \
  do {                                                                          \
    if (progEntry.flags & SQ_PRE) allOFF();                                     \
    if (progEntry.pat != P_NONE) {                                              \
      startPattern(progEntry.pat, progEntry.a, progEntry.b);                    \
      while (runPattern()) CO_YIELD(progPC);                                    \
    }                                                                           \
  } while (0)

bool runProgram() {
  CO_BEGIN(progPC);
  if (progKind == PROG_I2C && !progRandom) {
    for (;;) {                              // one iteration per pass; repeat 0 = forever
      PROG_RUN_ENTRY();
      if (progCode == 2) {                  // case 2 has no break: falls into case 3
        RandomTime = RandomOnTime + 1;
        startPattern(P_ALLONTIMED, 5000, 0);
        while (runPattern()) CO_YIELD(progPC);
      }
      if (progEntry.flags & SQ_RT) RandomTime = RandomOnTime + 1;
      if (progEntry.flags & SQ_POST) allOFF();
      nextIteration();
      if (runRepeat != 0 && runIteration >= runRepeat) break;
      CO_YIELD(progPC);                     // let loop() run between iterations (All off never yields)
    }
    if (runEnd == END_BLANK) allOFF();
    CO_END(progPC);
  }

  if (progRandom) {
    // Port of Random(int RandomInterval): one state step per Speed-gated loop pass, exactly as
    // loop() called it once per pass. The argument is drawn every pass, and truncated to int.
    for (;;) {
      RandomInterval = progRandomLong ? random(40000, 60000) : random(8000, 14000);
      switch (RandomState) {
        case 0:
          RandomMode = random(0, 35);
          RandomOnTime = random(1000, 1500);
          RandomTime = 0;
          RandomState++;
          break;
        case 1:
          memcpy_P(&progEntry, &RANDOM_TABLE[RandomMode], sizeof(SeqEntry));
          PROG_RUN_ENTRY();
          if (progEntry.flags & SQ_RT) RandomTime = RandomOnTime + 1;
          if (progEntry.flags & SQ_POST) allOFF();
          if (RandomTime++ > RandomOnTime) {
            RandomTime = 0;
            RandomState++;
            nextIteration();
          }
          break;
        case 2:
          allOFF();
          if (RandomTime++ > RandomInterval) {
            RandomTime = 0;
            RandomState = 0;
          }
          break;
      }
      PROG_WAIT_PASS();
    }
  }

  for (;;) {                                // GPIO modes 1-5 and 8: one run per loop pass, forever
    PROG_RUN_ENTRY();
    if (progEntry.flags & SQ_POST) allOFF();
    nextIteration();
    PROG_WAIT_PASS();
  }
}

void stepEngine() {
  if (progKind == PROG_NONE) return;
  if (waitKind == WAIT_US) {
    if (micros() - waitStart < waitUs) return;
  } else if (waitKind == WAIT_PASS) {
    if (passCount == waitPassMark) return;
  }
  waitKind = WAIT_NONE;
  if (runProgram()) return;
  progKind = PROG_NONE;                     // only I2C programs end; GPIO modes loop
  endRun(ST_COMPLETE);
  if (gpioCode != 0 && (cfgConfig & CFG_GPIO_ENABLE) && (cfgConfig & CFG_GPIO_RESUME)) startGpio(gpioCode, true);
}

// I2C: carry out what receiveEvent posted. Each received write consumes one random() and resets
// RandomTime, as the original receiveEvent did; then the last start/stop request, a brightness
// change, a configuration change and a SAVE are applied, in that order.
void consumeI2C() {
  if (i2cCount == 0 && !pendBrightness && pendSave == 0 && cfgConfig == cfgApplied) return;
  noInterrupts();
  byte n = i2cCount;
  i2cCount = 0;
  byte act = pendAction;
  pendAction = ACT_NONE;
  byte seq = pendSeq, repeat = pendRepeat, end = pendEnd, source = pendSource;
  bool bright = pendBrightness;
  pendBrightness = false;
  byte save = pendSave;
  pendSave = 0;
  interrupts();
  while (n--) {
    RandomOnTime = random(1000, 1500);
    RandomTime = 0;
  }
  if (act == ACT_START) {
    startProgram(PROG_I2C, seq);
    beginRun(seq, source, repeat, end);
    if (progRandom) { RandomState = 0; RandomTime = 0; }
  } else if (act == ACT_STOP_BLANK || act == ACT_STOP_FREEZE) {
    if (progKind != PROG_NONE) {
      stopProgram();
      endRun(ST_STOPPED);
    }
    if (act == ACT_STOP_BLANK) allOFF();
  }
  if (bright) {
    lc.setIntensity(0, brightness);
    lc.setIntensity(1, brightness);
  }
  applyConfig();
  if (save) saveConfig(save);
}

byte readInputs() {
  byte code = 0;
  if (digitalRead(A0) == LOW) { code = code + 4; }   // rotary bit 2
  if (digitalRead(A1) == LOW) { code = code + 2; }   // rotary bit 1
  if (digitalRead(A2) == LOW) { code = code + 1; }   // rotary bit 0
  if (digitalRead(11) == LOW) { code = 8; }          // Jumper 1 overrides the rotary switch
  if (digitalRead(13) == LOW) { code = 9; }          // Jumper 2 overrides everything
  return code;
}

// A newly accepted rotary/jumper code. Codes 1-9 blank the panel (as the original did on every
// mode change) and start that mode, abandoning whatever runs. Code 0 blanks and stops a running
// GPIO mode; an I2C sequence is left to finish, and nothing resumes after it.
void acceptGpio(byte code, bool blank) {
  gpioCode = code;
  if (!(cfgConfig & CFG_GPIO_ENABLE)) return;   // tracked for GPIO_CODE, but starts nothing
  if (code != 0) {
    if (blank) blankPANEL();
    startGpio(code, false);
  } else if (progKind != PROG_I2C) {
    if (blank) blankPANEL();
    if (progKind != PROG_NONE) {
      stopProgram();
      endRun(ST_STOPPED);
    }
  }
}

// GPIO trigger on a stabilised value: the decoded code (not each pin) is debounced. A code is
// accepted once it has read the same for DEBOUNCE_MS; accepting a code different from the last
// accepted one is the trigger. Codes seen while a rotary switch is moving are not stable and are
// ignored. The power-on code is accepted immediately, so a fitted jumper starts at once.
void gpioPass() {
  DigInState = readInputs();
  if (first_time) {
    first_time = 0;
    lastDigInState = DigInState;
    gpioCandidate = DigInState;
    acceptGpio(DigInState, false);    // power-on code counts as a trigger, without blanking
    return;
  }
  if (DigInState != gpioCandidate) {
    gpioCandidate = DigInState;
    gpioCandidateSince = time;
    return;
  }
  if (gpioCandidate != lastDigInState && time - gpioCandidateSince >= DEBOUNCE_MS) {
    lastDigInState = gpioCandidate;
    acceptGpio(gpioCandidate, true);
  }
}

void loop() {
  consumeI2C();
  time = millis();
  if (time - last_time > Speed)  	// Speed-gated pass: read the inputs (the original loop pass)
  {
    last_time = time;
    passCount++;
    gpioPass();
  }
  stepEngine();
}

////////////////////////////////////////////////////////////////////////////////////////
// BHD Functions (coroutines; each PAT_DELAY was a delay() in the original)
////////////////////////////////////////////////////////////////////////////////////////

void Frame() {
  MapBoolGrid();
  PrintGrid();
}

void ShowRows(byte r0, byte r1, byte r2, byte r3, byte r4, byte r5, byte r6, byte r7) {
  SetRow(0, r0); SetRow(1, r1); SetRow(2, r2); SetRow(3, r3);
  SetRow(4, r4); SetRow(5, r5); SetRow(6, r6); SetRow(7, r7);
  Frame();
}

bool EyeScan(int Repeats, int FlashDelay) {
  CO_BEGIN(patPC);
  for (sq.i = 0; sq.i < Repeats; sq.i++) {
    for (sq.j = 0; sq.j < 8; sq.j++) {
      SetRow(sq.j, B11111111);
      Frame();
      PAT_DELAY(FlashDelay);
      SetRow(sq.j, B00000000);
    }
    allOFF();
    PAT_DELAY(FlashDelay);
    for (sq.j = 0; sq.j < 8; sq.j++) {
      SetCol(7 - sq.j, B11111111);
      Frame();
      PAT_DELAY(FlashDelay);
      SetCol(7 - sq.j, B00000000);
    }
    allOFF();
    PAT_DELAY(FlashDelay);
  }
  CO_END(patPC);
}

bool CylonCol(int Repeats, int FlashDelay) {
  CO_BEGIN(patPC);
  for (sq.i = 0; sq.i < Repeats; sq.i++) {
    for (sq.j = 0; sq.j < 8; sq.j++) {
      SetCol(sq.j, B11111111);
      Frame();
      PAT_DELAY(FlashDelay);
      SetCol(sq.j, B00000000);
    }
    for (sq.j = 0; sq.j < 6; sq.j++) {
      SetCol(6 - sq.j, B11111111);
      Frame();
      PAT_DELAY(FlashDelay);
      SetCol(6 - sq.j, B00000000);
    }
  }
  CO_END(patPC);
}

bool CylonRow(int Repeats, int FlashDelay) {
  CO_BEGIN(patPC);
  for (sq.i = 0; sq.i < Repeats; sq.i++) {
    for (sq.j = 0; sq.j < 8; sq.j++) {
      SetRow(sq.j, B11111111);
      Frame();
      PAT_DELAY(FlashDelay);
      SetRow(sq.j, B00000000);
    }
    for (sq.j = 0; sq.j < 6; sq.j++) {
      SetRow(6 - sq.j, B11111111);
      Frame();
      PAT_DELAY(FlashDelay);
      SetRow(6 - sq.j, B00000000);
    }
  }
  CO_END(patPC);
}

void FlashH(int Repeats, int FlashDelay){   // unreachable in the original; kept as dead code
  for(int i=0; i<Repeats; i++){
    for(int j=0; j<4; j++){
      SetRow(j, B11111111);
      SetRow(j+4, B00000000);
    }
    MapBoolGrid();
    PrintGrid();
    delay(FlashDelay);
    for(int j=0; j<4; j++){
      SetRow(j, B00000000);
      SetRow(j+4, B11111111);
    }
    MapBoolGrid();
    PrintGrid();
    delay(FlashDelay);
  }
}

bool FlashV(int Repeats, int FlashDelay) {
  CO_BEGIN(patPC);
  for (sq.i = 0; sq.i < Repeats; sq.i++) {
    for (int j = 0; j < 4; j++) {
      SetCol(j, B11111111);
      SetCol(j + 4, B00000000);
    }
    Frame();
    PAT_DELAY(FlashDelay);
    for (int j = 0; j < 4; j++) {
      SetCol(j, B00000000);
      SetCol(j + 4, B11111111);
    }
    Frame();
    PAT_DELAY(FlashDelay);
  }
  CO_END(patPC);
}

bool FlashQ(int Repeats, int FlashDelay) {
  CO_BEGIN(patPC);
  for (sq.i = 0; sq.i < Repeats; sq.i++) {
    ShowRows(B00001111, B00001111, B00001111, B00001111, B11110000, B11110000, B11110000, B11110000);
    PAT_DELAY(FlashDelay);
    ShowRows(B11110000, B11110000, B11110000, B11110000, B00001111, B00001111, B00001111, B00001111);
    PAT_DELAY(FlashDelay);
  }
  CO_END(patPC);
}

bool FlashAll(int Repeats, int FlashDelay) {
  CO_BEGIN(patPC);
  for (sq.i = 0; sq.i < Repeats; sq.i++) {
    allON();
    PAT_DELAY(FlashDelay);
    allOFF();
    PAT_DELAY(FlashDelay);
  }
  CO_END(patPC);
}

bool OneLoop(int Repeats) {
  CO_BEGIN(patPC);
  for (sq.j = 0; sq.j < Repeats; sq.j++) {
    for (sq.i = 0; sq.i < 6; sq.i++) {
      VMagicPanel[1][7 - (1 + sq.i)] = true;
      Frame();
      PAT_DELAY(100);
      VMagicPanel[1][7 - (1 + sq.i)] = false;
    }
    for (sq.i = 0; sq.i < 4; sq.i++) {
      VMagicPanel[2 + sq.i][1] = true;
      Frame();
      PAT_DELAY(150);
      VMagicPanel[2 + sq.i][1] = false;
    }
    for (sq.i = 0; sq.i < 6; sq.i++) {
      VMagicPanel[6][(1 + sq.i)] = true;
      Frame();
      PAT_DELAY(100);
      VMagicPanel[6][(1 + sq.i)] = false;
    }
    for (sq.i = 0; sq.i < 4; sq.i++) {
      VMagicPanel[7 - (2 + sq.i)][6] = true;
      Frame();
      PAT_DELAY(150);
      VMagicPanel[7 - (2 + sq.i)][6] = false;
    }
  }
  CO_END(patPC);
}

bool TwoLoop(int Repeats) {
  CO_BEGIN(patPC);
  for (sq.j = 0; sq.j < Repeats; sq.j++) {
    for (sq.i = 0; sq.i < 6; sq.i++) {
      VMagicPanel[1][7 - (1 + sq.i)] = true;
      VMagicPanel[6][(1 + sq.i)] = true;
      Frame();
      PAT_DELAY(100);
      VMagicPanel[1][7 - (1 + sq.i)] = false;
      VMagicPanel[6][(1 + sq.i)] = false;
    }
    for (sq.i = 0; sq.i < 4; sq.i++) {
      VMagicPanel[2 + sq.i][1] = true;
      VMagicPanel[7 - (2 + sq.i)][6] = true;
      Frame();
      PAT_DELAY(150);
      VMagicPanel[2 + sq.i][1] = false;
      VMagicPanel[7 - (2 + sq.i)][6] = false;
    }
    for (sq.i = 0; sq.i < 6; sq.i++) {
      VMagicPanel[6][(1 + sq.i)] = true;
      VMagicPanel[1][7 - (1 + sq.i)] = true;
      Frame();
      PAT_DELAY(100);
      VMagicPanel[6][(1 + sq.i)] = false;
      VMagicPanel[1][7 - (1 + sq.i)] = false;
    }
    for (sq.i = 0; sq.i < 4; sq.i++) {
      VMagicPanel[7 - (2 + sq.i)][6] = true;
      VMagicPanel[2 + sq.i][1] = true;
      Frame();
      PAT_DELAY(150);
      VMagicPanel[7 - (2 + sq.i)][6] = false;
      VMagicPanel[2 + sq.i][1] = false;
    }
  }
  CO_END(patPC);
}

bool FadeOutIn(byte type) {                                // FlthyMcNsty added a variable to this function pass a type value to allow for just a fade out sequence as well
  CO_BEGIN(patPC);
  for (sq.i = 0; sq.i < 2*NumLoops; sq.i++) {
    for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
      SetRow(i, (random(256)|random(256)));
    }
    Frame();
    PAT_DELAY(150);
  }
  for (sq.i = 0; sq.i < NumLoops; sq.i++) {
    for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
      SetRow(i, random(256));
    }
    Frame();
    PAT_DELAY(150);
  }
  for (sq.i = 0; sq.i < NumLoops; sq.i++) {
    for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
      SetRow(i, (random(256)&random(256)));
    }
    Frame();
    PAT_DELAY(150);
  }
  for (sq.i = 0; sq.i < NumLoops; sq.i++) {
    for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
      SetRow(i, (random(256)&random(256)&random(256)));
    }
    Frame();
    PAT_DELAY(150);
  }
  for (sq.i = 0; sq.i < NumLoops; sq.i++) {
    for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
      SetRow(i, (random(256)&random(256)&random(256)&random(256)));
    }
    Frame();
    PAT_DELAY(150);
  }
  for (sq.i = 0; sq.i < NumLoops; sq.i++) {
    for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
      SetRow(i, (random(256)&random(256)&random(256)&random(256)&random(256)));
    }
    Frame();
    PAT_DELAY(150);
  }
  if (type == 1) {
    for (sq.i = 0; sq.i < NumLoops; sq.i++) {
      for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
        SetRow(i, (random(256)&random(256)&random(256)&random(256)&random(256)));
      }
      Frame();
      PAT_DELAY(150);
    }
    for (sq.i = 0; sq.i < NumLoops; sq.i++) {
      for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
        SetRow(i, (random(256)&random(256)&random(256)&random(256)));
      }
      Frame();
      PAT_DELAY(150);
    }
    for (sq.i = 0; sq.i < NumLoops; sq.i++) {
      for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
        SetRow(i, (random(256)&random(256)&random(256)));
      }
      Frame();
      PAT_DELAY(150);
    }
    for (sq.i = 0; sq.i < NumLoops; sq.i++) {
      for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
        SetRow(i, (random(256)&random(256)));
      }
      Frame();
      PAT_DELAY(150);
    }
    for (sq.i = 0; sq.i < NumLoops; sq.i++) {
      for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
        SetRow(i, random(256));
      }
      Frame();
      PAT_DELAY(150);
    }
    for (sq.i = 0; sq.i < 2*NumLoops; sq.i++) {
      for (int i = 0; i < 16; i++) {     // rows 8-15 land in the spill area
        SetRow(i, (random(256)|random(256)));
      }
      Frame();
      PAT_DELAY(150);
    }
  }
  CO_END(patPC);
}

bool TheTest(int FlashDelay) {
  CO_BEGIN(patPC);
  for (sq.i = 0; sq.i < 8; sq.i++) {
    for (sq.j = 0; sq.j < 8; sq.j++) {
      VMagicPanel[sq.i][sq.j] = true;
      Frame();
      PAT_DELAY(FlashDelay);
    }
  }
  for (sq.i = 0; sq.i < 8; sq.i++) {
    for (sq.j = 0; sq.j < 8; sq.j++) {
      VMagicPanel[sq.i][sq.j] = false;
      Frame();
      PAT_DELAY(delaytime);
    }
  }
  CO_END(patPC);
}

bool OneTest(int FlashDelay) {
  CO_BEGIN(patPC);
  for (sq.i = 0; sq.i < 8; sq.i++) {
    for (sq.j = 0; sq.j < 8; sq.j++) {
      VMagicPanel[sq.i][sq.j] = true;
      Frame();
      PAT_DELAY(FlashDelay);
      VMagicPanel[sq.i][sq.j] = false;
    }
  }
  CO_END(patPC);
}

bool Symbol() {
  CO_BEGIN(patPC);
  ShowRows(B00000100, B00000010, B11111111, B00000000, B11100111, B00100100, B00100100, B01000010);
  PAT_DELAY(3000);
  CO_END(patPC);
}

bool Cross() {
  CO_BEGIN(patPC);
  ShowRows(B00000000, B01000010, B00100100, B00011000, B00011000, B00100100, B01000010, B00000000);
  PAT_DELAY(3000);
  CO_END(patPC);
}

void MapBoolGrid(){
  for(int Row=0; Row<8; Row++){
    MagicPanel[2*Row]=128*VMagicPanel[Row][7]+64*VMagicPanel[Row][6]+32*VMagicPanel[Row][5]+16*VMagicPanel[Row][4];       // 0, 2, 4, 6, 8, 10, 12, 14
    MagicPanel[2*Row+1]=8*VMagicPanel[Row][3]+4*VMagicPanel[Row][2]+2*VMagicPanel[Row][1]+VMagicPanel[Row][0];            // 1, 3, 5, 7, 9, 11, 13, 15
  }
}

void PrintGrid(){
  for(int i=0; i<16; i++){
    if(i<8){
      lc.setRow(0, i, MagicPanel[i]);
    }else{
      lc.setRow(1, i-8, MagicPanel[i]);
    }
  }
}

void SetRow(int LEDRow, unsigned char RowState){
  for(int Col=0; Col<8; Col++){
    VMagicPanel[LEDRow][Col]=((RowState >> Col) & 1);
  }
}

void SetCol(int LEDCol, unsigned char ColState){
  for(int Row=0; Row<8; Row++){
    VMagicPanel[Row][LEDCol]=((ColState >> Row) & 1);
  }
}

void allON() {  //all LEDs ON simple style - Huh how does this work?
  for(int row=0;row<8;row++) {
    for(int col=0;col<8;col++) {
      VMagicPanel[row][col]=true;
    }
  }
  MapBoolGrid();
  PrintGrid();
}

void allOFF() {
  for(int row=0;row<8;row++) {
    for(int col=0;col<8;col++) {
      VMagicPanel[row][col]=false;
    }
  }
  MapBoolGrid();
  PrintGrid();
}


////////////////////////////////////// end BHD //////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////
// FlthyMcNsty Functions (coroutines)
////////////////////////////////////////////////////////////////////////////////////////
bool allONTimed(int timer)
{
  CO_BEGIN(patPC);
  allOFF();
  for (int row = 0; row < 8; row++) {
    SetRow(row, B11111111);
  }
  Frame();
  if (timer < 1) {          // Passing a value of 0 or below turns panel on indefinately (well for 1000s anyway)
    PAT_DELAY(1000000UL);
  } else {
    PAT_DELAY(timer);       // Otherwise it stays on for the number of ms passed.
    // the original had `allOFF;` here: a no-op statement, so the panel stays on
  }
  CO_END(patPC);
}

bool TraceDown(int timer, byte type)
{
  CO_BEGIN(patPC);
  while (sq.r < timer) {
    for (sq.i = 0; sq.i < 8; sq.i++) {
      SetRow(sq.i, B11111111);
      Frame();
      PAT_DELAY(200);
      if (type == 2) {
        SetRow(sq.i, B00000000);
        Frame();
      }
    }
    allOFF();
    sq.r++;
  }
  CO_END(patPC);
}

bool TraceUp(int timer, byte type)
{
  CO_BEGIN(patPC);
  while (sq.r < timer) {
    for (sq.i = 7; sq.i >= 0; --sq.i) {
      SetRow(sq.i, B11111111);
      Frame();
      PAT_DELAY(200);
      if (type == 2) {
        SetRow(sq.i, B00000000);
        Frame();
      }
    }
    allOFF();
    sq.r++;
  }
  CO_END(patPC);
}

// TraceLeft/TraceRight: the original unrolls 8 frames; frame k sets every row to one mask.
// Left, type 1: 0x01, 0x03 ... 0xFF; type 2: 0x01, 0x02 ... 0x80.
// Right, type 1: 0x80, 0xC0 ... 0xFF; type 2: 0x80, 0x40 ... 0x01.
void SetAllRows(byte mask) {
  for (int row = 7; row >= 0; --row) {
    SetRow(row, mask);
  }
}

bool TraceLeft(int timer, byte type)
{
  CO_BEGIN(patPC);
  while (sq.r < timer) {
    if (type == 1 || type == 2) {
      for (sq.i = 0; sq.i < 8; sq.i++) {
        SetAllRows(type == 1 ? (byte)((2 << sq.i) - 1) : (byte)(1 << sq.i));
        Frame();
        PAT_DELAY(200);
      }
    }
    allOFF();
    sq.r++;
  }
  CO_END(patPC);
}

bool TraceRight(int timer, byte type)
{
  CO_BEGIN(patPC);
  while (sq.r < timer) {
    if (type == 1 || type == 2) {
      for (sq.i = 0; sq.i < 8; sq.i++) {
        SetAllRows(type == 1 ? (byte)(0xFF << (7 - sq.i)) : (byte)(0x80 >> sq.i));
        Frame();
        PAT_DELAY(200);
      }
    }
    allOFF();
    sq.r++;
  }
  CO_END(patPC);
}

bool RandomPixel(int timer) {
  CO_BEGIN(patPC);
  while (sq.r < timer) {
    {                                                   // scope ends before the yield below
      int randRow = random(0,7);
      int randCol = random(0,7);
      SetRow(randRow, (byte)(B10000000 >> randCol));   // the original's switch: col c -> bit 7-c
    }
    Frame();
    PAT_DELAY(150);
    allOFF();
    sq.r++;
  }
  CO_END(patPC);
}

// Quadrant helpers: rows 0-3 (top) or 4-7 (bottom) set to a nibble mask.
void SetTop(byte mask)    { for (int row = 0; row < 4; row++) SetRow(row, mask); }
void SetBottom(byte mask) { for (int row = 4; row < 8; row++) SetRow(row, mask); }

bool Quadrant(int timer, byte type)
{
  CO_BEGIN(patPC);
  while (sq.r < timer) {
    if (type == 1) {
      SetTop(B11110000);    Frame(); PAT_DELAY(200);
      SetTop(B00001111);    Frame(); PAT_DELAY(200);
      allOFF();
      SetBottom(B11110000); Frame(); PAT_DELAY(200);
      SetBottom(B00001111); Frame(); PAT_DELAY(200);
    }
    else if (type == 2) {
      SetTop(B00001111);    Frame(); PAT_DELAY(200);
      SetTop(B11110000);    Frame(); PAT_DELAY(200);
      allOFF();
      SetBottom(B00001111); Frame(); PAT_DELAY(200);
      SetBottom(B11110000); Frame(); PAT_DELAY(200);
    }
    else if (type == 3) {
      SetTop(B00001111);    Frame(); PAT_DELAY(200);
      allOFF();
      SetBottom(B00001111); Frame(); PAT_DELAY(200);
      allOFF();
      SetBottom(B11110000); Frame(); PAT_DELAY(200);
      allOFF();
      SetTop(B11110000);    Frame(); PAT_DELAY(200);
    }
    else if (type == 4) {
      SetTop(B11110000);    Frame(); PAT_DELAY(200);
      allOFF();
      SetBottom(B11110000); Frame(); PAT_DELAY(200);
      allOFF();
      SetBottom(B00001111); Frame(); PAT_DELAY(200);
      allOFF();
      SetTop(B00001111);    Frame(); PAT_DELAY(200);
    }
    allOFF();
    sq.r++;
  }
  CO_END(patPC);
}

bool Toggle(int timer)
{
  CO_BEGIN(patPC);
  while (sq.r < timer) {
    SetTop(B11111111); SetBottom(B00000000);
    Frame();
    PAT_DELAY(500);
    SetTop(B00000000); SetBottom(B11111111);
    Frame();
    PAT_DELAY(500);
    sq.r++;
  }
  CO_END(patPC);
}

bool Alert(int timer)
{
  CO_BEGIN(patPC);
  while (sq.r < timer) {
    for (int row = 0; row < 8; row++)   // no braces in the original: allON() really runs 8 times
      allON();
    PAT_DELAY(250);
    allOFF();
    PAT_DELAY(250);
    sq.r++;
  }
  CO_END(patPC);
}

bool Expand(int timer, byte type)
{
  CO_BEGIN(patPC);
  while (sq.r < timer) {
    if (type == 1) {
      ShowRows(B00000000, B00000000, B00000000, B00011000, B00011000, B00000000, B00000000, B00000000);
      PAT_DELAY(200);
      ShowRows(B00000000, B00000000, B00111100, B00111100, B00111100, B00111100, B00000000, B00000000);
      PAT_DELAY(200);
      ShowRows(B00000000, B01111110, B01111110, B01111110, B01111110, B01111110, B01111110, B00000000);
      PAT_DELAY(200);
      allON();
      PAT_DELAY(200);
    }
    else if (type == 2) {
      ShowRows(B00000000, B00000000, B00000000, B00011000, B00011000, B00000000, B00000000, B00000000);
      PAT_DELAY(200);
      ShowRows(B00000000, B00000000, B00111100, B00100100, B00100100, B00111100, B00000000, B00000000);
      PAT_DELAY(200);
      ShowRows(B00000000, B01111110, B01000010, B01000010, B01000010, B01000010, B01111110, B00000000);
      PAT_DELAY(200);
      ShowRows(B11111111, B10000001, B10000001, B10000001, B10000001, B10000001, B10000001, B11111111);
      PAT_DELAY(200);
    }
    allOFF();
    PAT_DELAY(200);
    sq.r++;
  }
  CO_END(patPC);
}

bool Compress(int timer, byte type)
{
  CO_BEGIN(patPC);
  while (sq.r < timer) {
    if (type == 1) {
      allON();
      PAT_DELAY(200);
      ShowRows(B00000000, B01111110, B01111110, B01111110, B01111110, B01111110, B01111110, B00000000);
      PAT_DELAY(200);
      ShowRows(B00000000, B00000000, B00111100, B00111100, B00111100, B00111100, B00000000, B00000000);
      PAT_DELAY(200);
      ShowRows(B00000000, B00000000, B00000000, B00011000, B00011000, B00000000, B00000000, B00000000);
      PAT_DELAY(200);
    }
    else if (type == 2) {
      ShowRows(B11111111, B10000001, B10000001, B10000001, B10000001, B10000001, B10000001, B11111111);
      PAT_DELAY(200);
      ShowRows(B00000000, B01111110, B01000010, B01000010, B01000010, B01000010, B01111110, B00000000);
      PAT_DELAY(200);
      ShowRows(B00000000, B00000000, B00111100, B00100100, B00100100, B00111100, B00000000, B00000000);
      PAT_DELAY(200);
      ShowRows(B00000000, B00000000, B00000000, B00011000, B00011000, B00000000, B00000000, B00000000);
      PAT_DELAY(200);
    }
    allOFF();
    PAT_DELAY(200);
    sq.r++;
  }
  CO_END(patPC);
}

bool MySymbol() {
  CO_BEGIN(patPC);
  ShowRows(B00000000, B01111110, B00000010, B01111110, B01000000, B01000000, B01111110, B00000000);
  PAT_DELAY(1000);
  ShowRows(B00000000, B01111110, B01000010, B01000000, B01001110, B01000010, B01111110, B00000000);
  PAT_DELAY(1000);
  ShowRows(B00000000, B01000010, B01000010, B01000010, B01011010, B01100110, B01000010, B00000000);
  PAT_DELAY(1000);
  ShowRows(B00000000, B01111100, B01000010, B01000010, B01000010, B01000010, B01111100, B00000000);
  PAT_DELAY(1000);
  CO_END(patPC);
}

bool runPattern() {
  switch (patId) {
    case P_EYESCAN:     return EyeScan(patA, patB);
    case P_CYLONCOL:    return CylonCol(patA, patB);
    case P_CYLONROW:    return CylonRow(patA, patB);
    case P_FLASHV:      return FlashV(patA, patB);
    case P_FLASHQ:      return FlashQ(patA, patB);
    case P_FLASHALL:    return FlashAll(patA, patB);
    case P_ONELOOP:     return OneLoop(patA);
    case P_TWOLOOP:     return TwoLoop(patA);
    case P_FADEOUTIN:   return FadeOutIn(patA);
    case P_THETEST:     return TheTest(patA);
    case P_ONETEST:     return OneTest(patA);
    case P_SYMBOL:      return Symbol();
    case P_CROSS:       return Cross();
    case P_ALLONTIMED:  return allONTimed(patA);
    case P_TRACEDOWN:   return TraceDown(patA, patB);
    case P_TRACEUP:     return TraceUp(patA, patB);
    case P_TRACELEFT:   return TraceLeft(patA, patB);
    case P_TRACERIGHT:  return TraceRight(patA, patB);
    case P_RANDOMPIXEL: return RandomPixel(patA);
    case P_QUADRANT:    return Quadrant(patA, patB);
    case P_TOGGLE:      return Toggle(patA);
    case P_ALERT:       return Alert(patA);
    case P_EXPAND:      return Expand(patA, patB);
    case P_COMPRESS:    return Compress(patA, patB);
    case P_MYSYMBOL:    return MySymbol();
    default:            return false;
  }
}

//////
//////////////////////////////// end FlthyMcNsty //////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

void blankPANEL() {
  lc.clearDisplay(0);
  lc.clearDisplay(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////
// I2C register interface (docs/i2c-protocol.md). receiveEvent() and requestEvent() run in the TWI
// interrupt: they decode, validate, move the register pointer, update INFO_INDEX, CONFIG and the
// error registers, and post actions; consumeI2C() carries the actions out between frames.
////////////////////////////////////////////////////////////////////////////////////////////////

// Called from consumeI2C() whenever CONFIG may have changed: re-enabling GPIO starts the selected
// rotary/jumper mode, unless an I2C sequence is running (it resumes the mode when it ends).
void applyConfig() {
  byte was = cfgApplied;
  cfgApplied = cfgConfig;
  if (!(was & CFG_GPIO_ENABLE) && (cfgApplied & CFG_GPIO_ENABLE) && gpioCode != 0 && progKind != PROG_I2C) {
    blankPANEL();
    startGpio(gpioCode, false);
  }
}

byte eeChecksum(byte cfg, byte bright) { return EE_MAGIC ^ EE_LAYOUT ^ cfg ^ bright ^ 0xA5; }

void loadConfig() {
  byte magic = eeprom_read_byte((const uint8_t*)0);
  byte layout = eeprom_read_byte((const uint8_t*)1);
  byte cfg = eeprom_read_byte((const uint8_t*)2);
  byte bright = eeprom_read_byte((const uint8_t*)3);
  byte sum = eeprom_read_byte((const uint8_t*)4);
  if (magic == EE_MAGIC && layout == EE_LAYOUT && (cfg & ~CFG_VALID) == 0 && bright <= 15 &&
      sum == eeChecksum(cfg, bright)) {
    cfgConfig = cfg;
    cfgDefaultBrightness = bright;
  }
  cfgApplied = cfgConfig;
}

void saveConfig(byte magic) {
  if (magic == FACTORY_MAGIC) {
    noInterrupts();
    cfgConfig = CFG_DEFAULT;
    cfgDefaultBrightness = 15;
    interrupts();
    applyConfig();
  }
  byte cfg = cfgConfig, bright = cfgDefaultBrightness;
  eeprom_update_byte((uint8_t*)0, EE_MAGIC);
  eeprom_update_byte((uint8_t*)1, EE_LAYOUT);
  eeprom_update_byte((uint8_t*)2, cfg);
  eeprom_update_byte((uint8_t*)3, bright);
  eeprom_update_byte((uint8_t*)4, eeChecksum(cfg, bright));
}

void i2cError(byte code) {
  lastError = code;
  errorCount++;
}

// Validates a write of n bytes to plain registers starting at reg; returns an error code or 0.
byte checkPlain(byte reg, const byte* d, byte n) {
  for (byte i = 0; i < n; i++) {
    byte r = reg + i, v = d[i];
    switch (r) {
      case REG_BRIGHTNESS:
      case REG_DEFAULT_BRIGHTNESS: if (v > 15) return ERR_BAD_VALUE; break;
      case REG_CONFIG:             if (v & ~CFG_VALID) return ERR_BAD_VALUE; break;
      case REG_INFO_INDEX:         if (v >= SEQ_COUNT) return ERR_BAD_VALUE; break;
      default:
        if (r < 0x0A || (r >= REG_STATUS && r < REG_STATUS + 16) || (r > REG_INFO_INDEX && r < 0x56))
          return ERR_READ_ONLY;
        return ERR_UNKNOWN_REG;
    }
  }
  return 0;
}

void writeRegisters(byte reg, const byte* d, byte n) {
  byte err = 0;
  switch (reg) {
    case REG_START:
      if (n > 3) { err = ERR_BAD_LENGTH; break; }
      if (d[0] >= SEQ_COUNT || (n > 2 && d[2] > END_BLANK)) { err = ERR_BAD_VALUE; break; }
      pendAction = ACT_START;
      pendSeq = d[0];
      pendRepeat = n > 1 ? d[1] : 1;
      pendEnd = n > 2 ? d[2] : END_DEFAULT;
      pendSource = SRC_I2C;
      break;
    case REG_STOP:
      if (n != 1) { err = ERR_BAD_LENGTH; break; }
      if (d[0] > 1) { err = ERR_BAD_VALUE; break; }
      pendAction = d[0] ? ACT_STOP_FREEZE : ACT_STOP_BLANK;
      break;
    case REG_SAVE:
      if (n != 1) { err = ERR_BAD_LENGTH; break; }
      if (d[0] != SAVE_MAGIC && d[0] != FACTORY_MAGIC) { err = ERR_BAD_MAGIC; break; }
      pendSave = d[0];
      break;
    default:
      err = checkPlain(reg, d, n);
      if (err) break;
      for (byte i = 0; i < n; i++) {
        switch ((byte)(reg + i)) {
          case REG_BRIGHTNESS:         brightness = d[i]; pendBrightness = true; break;
          case REG_DEFAULT_BRIGHTNESS: cfgDefaultBrightness = d[i]; break;
          case REG_CONFIG:             cfgConfig = d[i]; break;
          case REG_INFO_INDEX:         infoIndex = d[i]; break;
        }
      }
  }
  if (err) i2cError(err);
}

// A write: byte 0 with bit 7 set addresses a register, otherwise it is a legacy command. All bytes
// are drained, so a long or malformed write can no longer leave the receiver deaf.
void receiveEvent(int count) {
  byte buf[1 + MAX_WRITE_DATA];
  byte len = 0;
  while (Wire.available()) {
    byte b = Wire.read();
    if (len < sizeof buf) buf[len] = b;
    if (len < 255) len++;
  }
  i2cCount++;                              // every write advances random() once (D-16)
  if (len == 0) return;                    // address-only probe
  if (!(buf[0] & REG_BIT)) {
    if (len > 1) i2cError(ERR_BAD_LENGTH);
    else if (!(cfgConfig & CFG_LEGACY)) i2cError(ERR_LEGACY_OFF);
    else if (buf[0] < 40) {
      pendAction = ACT_START;
      pendSeq = buf[0];
      pendRepeat = 1;
      pendEnd = END_DEFAULT;
      pendSource = SRC_LEGACY;
    }
    return;
  }
  regPtr = buf[0] & 0x7F;
  if (len == 1) return;                    // pointer set for a following read
  if (len > sizeof buf) { i2cError(ERR_BAD_LENGTH); return; }
  writeRegisters(regPtr, buf + 1, len - 1);
}

const byte IDENTITY[10] PROGMEM = { 'M', 'P', PROTO_MAJOR, PROTO_MINOR, FW_MAJOR, FW_MINOR, FW_PATCH,
                                    SEQ_COUNT, CAPS, 0x14 };

void statusBlock(byte* st) {
  unsigned long now = millis();
  bool running = runState == ST_RUNNING;
  unsigned long elapsed = runState == ST_IDLE ? 0 : (running ? now : runEndMs) - runStartMs;
  unsigned int remaining = 0;
  if (running) {
    unsigned long len = pgm_read_dword(&SEQ_INFO[runSeq].lengthMs);
    unsigned long inIter = now - iterStartMs;
    if (len == LEN_INDEFINITE) remaining = 0xFFFF;
    else if (inIter < len) {
      unsigned long s10 = (len - inIter + 9) / 10;
      remaining = s10 > 0xFFFE ? 0xFFFF : (unsigned int)s10;
    }
  }
  byte sub = runSeq;
  if (running && progRandom) sub = RandomState == 1 ? pgm_read_byte(&RANDOM_SEQ[RandomMode]) : SUB_SEQ_OFF;
  st[0] = runSeq;
  st[1] = runSource;
  st[2] = runState;
  st[3] = sub;
  st[4] = runIteration;
  st[5] = runRepeat;
  memcpy(st + 6, &elapsed, 4);             // AVR is little-endian, as the protocol
  memcpy(st + 10, &remaining, 2);
  st[12] = runCounter;
  st[13] = gpioCode;
  st[14] = lastError;
  st[15] = errorCount;
}

byte registerByte(byte r, const byte* st) {
  if (r < sizeof IDENTITY) return pgm_read_byte(&IDENTITY[r]);
  if (r >= REG_STATUS && r < REG_STATUS + 16) return st[r - REG_STATUS];
  if (r > REG_INFO_INDEX && r < 0x56) return pgm_read_byte((const byte*)&SEQ_INFO[infoIndex] + (r - REG_INFO_INDEX - 1));
  switch (r) {
    case REG_BRIGHTNESS:         return brightness;
    case REG_CONFIG:             return cfgConfig;
    case REG_DEFAULT_BRIGHTNESS: return cfgDefaultBrightness;
    case REG_INFO_INDEX:         return infoIndex;
    default:                     return 0;
  }
}

// A read: up to 32 bytes from the pointer. The status block is built once, inside the interrupt,
// so its multi-byte fields cannot tear; the pointer does not move (it stays until the next write).
void requestEvent() {
  byte st[16];
  byte out[32];
  statusBlock(st);
  for (byte i = 0; i < sizeof out; i++) {
    byte r = regPtr + i;
    out[i] = (regPtr + i < 0x80) ? registerByte(r, st) : 0;
  }
  Wire.write(out, sizeof out);
}
