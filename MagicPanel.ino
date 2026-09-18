// Magic Panel FX by IA-PARTS.com 
//
//// Release History
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

void setup()
{
  Wire.begin(I2CAdress);                   // Start I2C Bus as Slave at I2C Address
  Wire.onReceive(receiveEvent);            // register event so when we receive something we jump to receiveEvent();
  /*
   The MAX72XX is in power-saving mode on startup,
   we have to do a wakeup call
   */
  lc.shutdown(0,false);
  lc.shutdown(1,false);
  /* Set the brightness to a medium values */
  lc.setIntensity(0,15);
  lc.setIntensity(1,15);

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
// Triggers: an I2C write (receiveEvent only records it) or a debounced change of the rotary/jumper
// code abandons the running sequence at its next yield and starts the new one; the same trigger
// restarts it. GPIO modes loop until the next trigger; an I2C sequence that ends while a GPIO
// mode is selected resumes that mode from its start.
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

enum { PROG_NONE, PROG_I2C, PROG_GPIO };
byte progKind = PROG_NONE;
byte progCode = 0;
SeqEntry progEntry;               // entry of the running program (or of the Random() mode)

byte gpioCode = 0;                // accepted rotary/jumper code (0 = no GPIO mode)
#define DEBOUNCE_MS 20            // a new rotary/jumper code must be stable this long to count
byte gpioCandidate = 0;           // code currently being debounced
unsigned long gpioCandidateSince = 0;

volatile int  i2cCmd = 0;         // last byte received over I2C (-1 for an empty write)
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
  if (kind == PROG_I2C) memcpy_P(&progEntry, &I2C_TABLE[code], sizeof(SeqEntry));
  else                  memcpy_P(&progEntry, &GPIO_TABLE[code], sizeof(SeqEntry));
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
  if (progKind == PROG_I2C) {
    PROG_RUN_ENTRY();
    if (progCode == 2) {                    // case 2 has no break: falls into case 3
      RandomTime = RandomOnTime + 1;
      startPattern(P_ALLONTIMED, 5000, 0);
      while (runPattern()) CO_YIELD(progPC);
    }
    if (progEntry.flags & SQ_RT) RandomTime = RandomOnTime + 1;
    if (progEntry.flags & SQ_POST) allOFF();
    CO_END(progPC);
  }

  if (isRandomMode(progCode)) {
    // Port of Random(int RandomInterval): one state step per Speed-gated loop pass, exactly as
    // loop() called it once per pass. The argument is drawn every pass, and truncated to int.
    for (;;) {
      RandomInterval = (progCode == 7) ? random(40000, 60000) : random(8000, 14000);
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
  byte ended = progKind;
  progKind = PROG_NONE;
  if (ended == PROG_I2C && gpioCode != 0) startGpio(gpioCode, true);
}

// I2C: take what receiveEvent recorded. Each received write consumes one random() and resets
// RandomTime, as the original receiveEvent did; known commands (0-39) switch the sequence.
void consumeI2C() {
  if (i2cCount == 0) return;
  noInterrupts();
  byte n = i2cCount;
  int cmd = i2cCmd;
  i2cCount = 0;
  interrupts();
  while (n--) {
    RandomOnTime = random(1000, 1500);
    RandomTime = 0;
  }
  if (cmd >= 0 && cmd < 40) startProgram(PROG_I2C, cmd);
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
  if (code != 0) {
    if (blank) blankPANEL();
    startGpio(code, false);
  } else if (progKind != PROG_I2C) {
    if (blank) blankPANEL();
    stopProgram();
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

// Runs inside the TWI interrupt: only record the command; loop() acts on it. Exactly one byte is
// read, as in the original, so a multi-byte write still leaves the receive buffer undrained and
// deafens the receiver until reset (firmware-map 3.3; baselined as-is).
void receiveEvent(int eventCode) {
  i2cCmd = Wire.read();
  i2cCount++;
}
