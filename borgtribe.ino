/*
 * BORGTRIBE
  Korg Electribe ES1 Mod
  1/ Control the pitch with a digital potentiometer to enable recording notes via MIDI
  2/ Intercept MIDI notes messages to parse them and send the right pitch

  Note below C2 are purely ignored
  4 octaves from C2 til C5

  Parts notes on Electribe ES1 must be configured below C2 to avoid conflicts

  Part 1-9 : C1-G#1
  Time Slice : A1
  Audio IN : A1#

*/

#include <SoftwareSerial.h>
#include "mcp4x.h"
#include "ES1Midi.h"

#define RX 2
#define TX 3

SoftwareSerial midiSerial(RX, TX); // RX, TX

using namespace mcp4x;
// Digital potentiometer.
MCP4XXX mcp4251;

float r=0; // Analog ratio
unsigned maxVal = 0;

extern ES1GlobalParameters_t ES1GlobalParameters;
extern unsigned borgTribeAnalogPotVal ;
extern unsigned borgTribePrevAnalogPotVal ; 
extern uint8_t borgTribeMode;

/////////////////////////////////////////////////
// MAIN
/////////////////////////////////////////////////

void setup() {
   
  Serial.begin(31250);      // Midi baud rate to MIDI IN of ES1
  midiSerial.begin(31250);  // used to read SysEx from midi out when necessary
 
  memset(&ES1GlobalParameters,0xFF,sizeof(ES1GlobalParameters)); 

  // Wait for Electribe ready and get the current channel  
  // We can't go further without the MIDI channel and global parameters....
  while ( ! ES1getGlobalParameters() ) delay(1000);

  // Set parameters for BorgTribe (notes number)
  while ( ! ES1setGlobalParameters() ) delay(2000);

  // Digital potentiometer init
  mcp4251.begin();
  maxVal = mcp4251.max_value()-1;
  r = 1023.00 / maxVal;

  // Force digit potentiometer sync with analogic
  borgTribePrevAnalogPotVal = 999;
  
  // Say ready
  borgTribeFlashPart(borgTribeMode+1,ES1GlobalParameters.midiGlobalParameters.MidiCH);
}


void loop() {

  // Listen analog Pitch potentiometer and change value if moved
  borgTribeAnalogPotVal = analogRead(2) / r;
  if ( borgTribeAnalogPotVal != borgTribePrevAnalogPotVal ) {
     borgTribePrevAnalogPotVal = borgTribeAnalogPotVal;
     mcp4251.set(constrain(borgTribeAnalogPotVal,0,maxVal));
  }

  if (Serial.available() ) midiParser(Serial.read());

}
