/*

ABOUT THIS:
digitfreequencer v17 by cellfactorysounds

released oct 2020 under CC BY-SA 4.0

the digit freequencer is a eurorack gate sequencer with millisecond resolution
i made it to allow quick manual programming of events outside of the grid - swing, humanisation, that sort of thing


PIN IO:
indicator LED control with CD4051 - ABC bits on PB0, PB1 and PB2, respectively (pins D8, D9 and D10)

gate output on PB3 (pin D11)

gate input (records high gates) on PD7 (pin D7)

reset button input (reset on rising edge once per divided clock) on PD6 (pin D6)

clock pulse input (advance on rising edge) on PD5 (pin D5)

enable toggle input (play button input directed to aux output on high, to sequence on low) on PD4 (pin D4)

delete all toggle input (gate input deletes gates on high) on PD3 (pin D3)

undo last button input (deletes latest gate in sequence on high) on PD2 (pin D2)

aux gate output on PB4 (pin D12)

NOTES:
todo software:
- move clock divider mode to loop, moving everything else to a while loop, allowing clock divider mode without resetting

todo hardware:
- design pcb frontplate
- negative voltage input protection
- measure final power consumption

timing measurement:
41-46micros/cycle depending on input

power consumption on elegoo nano v3:
<30 mA 12 V

*/

// timekeeping:
unsigned long zerotime = 0UL;
int currenttime = 0;
int gateon[32];
int gateoff[32];
int gatestate[32];
byte gatesum = 0;

// step sequence:
byte currentstep = 0;
byte ledsupdated = 0;
byte resetstate = 0;
byte resetbutton = 0;

// clock input state management:
byte clockstate = 0;
byte lastclockstate = 0;

// button input and aux output state management:
byte buttonstate = 0;
byte lastbuttonstate = 0;
byte buttonpushes = 0;
byte buttonpushesoff = 0;
byte auxstate = 0;
byte lastauxstate = 0;

// delete and enable input management:
byte deleteflag = 0;
byte enableflag = 0;

// undo management:
byte undostate = 0;
byte lastundostate = 0;
byte undodebounce = 20;
unsigned long undopress = 0UL;
byte undobutton = 0;

// cycle counter for prioritising code blocks:
byte cyclecount = 0;

// clock divider mode management:
byte divmode = 0;
byte divider = 0;
byte clocknumerator = 0;


// timing the sequence for troubleshooting: 
unsigned long timingcounter = 0UL;
unsigned long timingstart = 0UL;
unsigned long timingend = 0UL;


//---------------------------------------------------------------------------------------------------------------------
void setup() {
  
  DDRB |= B00011111; // pins D8-12 are defined as outputs

  // setup gate arrays:
  for (int i = 0; i < 32; i++) {
    gateon[i] = 32767;
    gateoff[i] = 0;
    gatestate[i] = 0;
  }
  
  // flashing lights to check LED outputs:
  for (int i = 0; i < 8; i++) {
    ledupdate(i);
    delay(13);
  }
  
  delay(100);
  
  // display firmware version on LEDs as binary:
  for (int i = 0; i < 120; i++) {
    ledupdate(0);
    delay(1);
    ledupdate(4);
    delay(1);
    ledupdate(0);
    delay(1);
    ledupdate(4);
    delay(1);
  }
  
  // flash LEDs again:
  for (int i = 0; i < 8; i++) {
    ledupdate(i);
    delay(13);
  }

  // clock divider mode check:
  enableflag = (PIND >> 4) & 1;
  divmode = (PIND >> 6) & 1; 
  if (divmode && enableflag) { // set clock divider mode flag if reset is pressed during setup
    while (divmode) { // clock divider mode
      divmode = (PIND >> 4) & 1; // exit clock divider mode if enable is toggled
      ledupdate(divider); // dislay current division
      buttonstate = (PIND >> 7) & 1; // read button input on PD7
      if (buttonstate == 1 && lastbuttonstate == 0) { // rising edge detection
        if (divider < 7) {
          divider++; // increment divider
        }
        else {
          divider = 0; // reset divider after 7
        }
        lastbuttonstate = 1; // input is now high
        delay(15); // debouncing delay
      }
      else if (buttonstate == 0 && lastbuttonstate == 1) { // falling edge detection
        lastbuttonstate = 0; // input is now low
        delay(15); // debouncing delay
      }
      else {
        delay(15); // wait a while
      }
    }
  }
  
  zerotime = millis(); // time at the start of step 0
  
  //Serial.begin(9600); // for debugging
}

//---------------------------------------------------------------------------------------------------------------------
void loop() {

  // only reset sequence once per (divided) clock if reset input is high:
  
  if (resetstate) {
    if ((PIND >> 6) & 1) { // read reset input on D6
      currentstep = 0; // reset to step 0
      ledsupdated = 0; // flag that the current step has changed and LEDs have to be updated
      // if button is held during reset; set gate off time and set button states as if the button is not pressed:
        if (buttonstate == 1 && lastbuttonstate == 1) { // button is held 
          buttonpushesoff = buttonpushes - 1; // correction as the index is incremented on rising edge
          gateoff[buttonpushesoff] = millis() - zerotime; // end time of gate relative to first step
          lastbuttonstate = 0;
          buttonstate = 0;
        }
      zerotime = millis(); // time at start of step 0
      resetstate = 0; // disable reset until next clock pulse
    }
  }
  clockstate = (PIND >> 5) & 1; // read clock input on D5
  if (clockstate == 1 && lastclockstate == 0) { // rising edge detection
    clocknumerator++; // count clocks
    lastclockstate = 1; // clock input is now high
    if (clocknumerator > divider) { // check what division was set
      currentstep++; // advance the sequence
      resetstate = 1; // allow reset
      ledsupdated = 0; // flag that the current step has changed and LEDs have to be updated
      clocknumerator = 0; // reset clock count
      if (currentstep > 7) {
        currentstep = 0; // reset the sequence if last step was 7
        // if button is held during reset; set gate off time and set button states as if the button is not pressed:
        if (buttonstate == 1 && lastbuttonstate == 1) { // button is held 
          buttonpushesoff = buttonpushes - 1; // correction as the index is incremented on rising edge
          gateoff[buttonpushesoff] = millis() - zerotime; // end time of gate relative to first step
          lastbuttonstate = 0;
          buttonstate = 0;
        }
        zerotime = millis(); // time at start of step 0
      }
    }
  }

  // update LED display only if a change has happened and the LEDs are not yet updated
  if (ledsupdated == 0) {
    ledupdate(currentstep); // update LEDs
    ledsupdated = 1; // flag that the LEDs have been updated to reflect the current step
  }

  // detect falling clock edge and reset clock state
  if (clockstate == 0 && lastclockstate == 1) { // no debounce assuming clean clock/gate signal
    lastclockstate = 0;
  }
  
  // check delete and enable toggle positions:
  if (cyclecount == 64 || cyclecount == 192) { // deprioritised to once every 128 cycles
    deleteflag = (PIND >> 3) & 1; // read delete input on D3
    enableflag = (PIND >> 4) & 1; // read enable input on D4
  }
  
  // only check for button input when enabled and not deleting:
  if (deleteflag == 0 && enableflag == 0) {
    buttonstate = (PIND >> 7) & 1; // read button input on PD7
    // detect rising edge and note time:
    if (buttonpushes < 32) { // only look for rising edges when less than 32 gates have been stored
      if (buttonstate == 1 && lastbuttonstate == 0) {
        lastbuttonstate = 1;
        gateon[buttonpushes] = millis() - zerotime; // start time of gate relative to first step
        gateoff[buttonpushes] = 32767; // set gate off time to maximum until button is released
        buttonpushes++; // increment index for gate arrays
        delay(5); // debouncing delay based on scope measurements of tactile switch bounce - if clock pulsewidth is less than this delay some clocks might be skipped
      }
    }
    
    // detect falling edge:
    if (buttonstate == 0 && lastbuttonstate == 1) {
      buttonpushesoff = buttonpushes - 1; // correction as the index is incremented on rising edge
      gateoff[buttonpushesoff] = millis() - zerotime; // end time of gate relative to first step
      lastbuttonstate = 0;
      delay(5); // debouncing delay based on scope measurements of tactile switch bounce - if clock pulsewidth is less than this delay some clocks might be skipped
    }
  }

  // if enable is high, recording gates is disabled - send a gate out of auxiliary output when button is pressed:
  if (enableflag) {
    auxstate = (PIND >> 7) & 1; // read button input on PD7
    if (auxstate == 1 && lastauxstate == 0) { // detect rising edge
      PORTB |= B00010000; // turn on auxgate
      lastauxstate = 1;
      delay(5); // debouncing delay
    }
    
    if (auxstate == 0 && lastauxstate == 1) { // detect falling edge
      PORTB &= B11101111; // turn off auxgate
      lastauxstate = 0;
      delay(5); // debouncing delay
    }
  }

  // check what time it is - how long since step 0 started:
  if (cyclecount & 1) { // deprioritise to every other cycle (odd numbers)
    currenttime = millis() - zerotime; // millisecond resolution with accuracy of however long it takes to cycle through the loop
  }

  // set gatestate based on the gateon/off times:
  if (cyclecount % 3 == 0) { // deprioritise to every third cycle
    for (int i = 0; i < 32; i++) {
      if (currenttime >= gateon[i] && currenttime < gateoff[i]){ // flag gate if current time is within on time for gate
        gatestate[i] = 1;
      }
      else {
        gatestate[i] = 0;
      }
    }
  
    // collect gatestates in gatesum:
    gatesum = 0;
    for (int i = 0; i < 32; i++) {
      gatesum += gatestate[i];
    }
  }
  
  // turn on or off gate based on gatesum:
  if (gatesum == 0) { // if no gates are currently supposed to be on
    PORTB &= B11110111; // turn off gate
  }
  else {
    PORTB |= B00001000; // otherwise turn on gate
  }

  // if delete is enabled, delete gate information on button press:
  if (deleteflag) {
    if ((PIND >> 7) & 1) { // not debounced
      buttonpushes = 0;  // reset button push counter
      for (int i = 0; i < 32; i++) { // reset all gates
        gateon[i] = 32767;
        gateoff[i] = 0;
        gatestate[i] = 0;
      }
    }
  }

  // deprioritise undo function to once every 128 cycles:
  if (cyclecount == 0 || cyclecount == 128) {
    undobutton = (PIND >> 2) & 1; // read undo button on D2
    // undo function only enabled when one or more gates have been recorded:
    if (buttonpushes > 0) {
      if (undobutton) {  // check for undo button press
        if (undostate == 0) {  // detect rising edge
          undostate = 1;  // undo button is high, set flag for debounce time start
          undopress = millis(); // store time for debouncing
        }
      }

      if (undostate == 1) {
        if (undopress - millis() > undodebounce) {  // check if debounce time has passed
          if (undobutton) { // check if undo button is still pressed after debounce time
            buttonpushes--;  // go back to latest gate recorded and reset gate information
            gateon[buttonpushes] = 32767;
            gateoff[buttonpushes] = 0;
            gatestate[buttonpushes] = 0;
            undostate = 2; // state indicates undo done and button still pressed
          }
          else {
            undostate = 0; // if the button is not pressed after the debounce time, it was not a valid button press
          }
        }
      }
    }
    
    if (undostate == 2) {
      if (undobutton == 0) { // detect falling edge
        undostate = 0; // reset undostate (rising edge detecting will debounce)
      }
    }
  }
  
  // incrementing cycle counter for deprioritising code, rolls over after 255:
  cyclecount++;

  /*
  // debugging and timing:
  if (timingcounter == 100000) {
    timingend = millis();
    Serial.println(timingend - timingstart);
    timingcounter = 0;
    timingstart = timingend;
  }
  timingcounter++;
  */
}


//---------------------------------------------------------------------------------------------------------------------
// LED display update function (modified to improve pcb layout):
void ledupdate(int i){
  switch (i) {
    case 1:
      PORTB &= B11111000; // CD4051 controlled by first three bits of PB (D8, D9 and D10)
      break;
      
    case 2:
      PORTB &= B11111000;
      PORTB |= B00000001;
      break;
      
    case 3:
      PORTB &= B11111000;
      PORTB |= B00000010;
      break;
      
    case 0:
      PORTB &= B11111000;
      PORTB |= B00000011;
      break;
      
    case 7:
      PORTB &= B11111000;
      PORTB |= B00000100;
      break;
      
    case 4:
      PORTB &= B11111000;
      PORTB |= B00000101;
      break;
      
    case 6:
      PORTB &= B11111000;
      PORTB |= B00000110;
      break;
      
    case 5:
      PORTB &= B11111000;
      PORTB |= B00000111;
      break;
  }
}
