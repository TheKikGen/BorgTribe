/*

  BORGTRIBE
  A Korg Electribe ES1 Mod allowing playing and recording chromatic notes
  Copyright (C) 2017/2018 by The KikGen labs.

  MAIN FILE - ARDUINO SKETCH
  
  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.

   Licence : MIT.
*/

#include <SoftwareSerial.h>
#include "mcp4x.h"
#include "ES1Midi.h"
#include "midiXparser.h"

#define RX 2
#define TX 3

SoftwareSerial midiSerial(RX, TX); // RX, TX

using namespace mcp4x;
// Digital potentiometer.
MCP4XXX mcp4251;

float r=0; // Analog ratio
unsigned maxVal = 0;

// Midi parsers
midiXparser midiInParser;
midiXparser midiOutParser;


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
  while ( ! ES1getGlobalParameters() ) {
    midiStop();
    delay(1000);
  }

  // Set parameters for BorgTribe (notes number)
  while ( ! ES1setGlobalParameters() ) delay(2000);

  // Set midi parsers  
  midiInParser.setMidiChannelFilter(ES1GlobalParameters.midiGlobalParameters.MidiCH);
  midiInParser.setChannelMsgFilterMask(midiXparser::noteOffMsk | midiXparser::noteOnMsk 
                                | midiXparser::controlChangeMsk | midiXparser::pitchBendMsk);

  midiOutParser.setMidiChannelFilter(ES1GlobalParameters.midiGlobalParameters.MidiCH);
  midiOutParser.setChannelMsgFilterMask(midiXparser::noteOnMsk);

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

  
  if ( borgTribeMode != BORGTRIBE_POTPITCHEDNOTE_MODE ) {
      // Listen analog Pitch potentiometer and change value if moved
      borgTribeAnalogPotVal = analogRead(2) / r;
      if ( borgTribeAnalogPotVal != borgTribePrevAnalogPotVal ) {
         borgTribePrevAnalogPotVal = borgTribeAnalogPotVal;
         mcp4251.set(borgTribeAnalogPotVal);
      }
  }

  // MIDI IN
  if (Serial.available() ) {

    if ( midiInParser.parse(  Serial.read() ) ) 
      ES1processMidiMsg(midiInParser.getMidiMsg()) ;
    else if( ! midiInParser.isByteCaptured()) 
        Serial.write(midiInParser.getByte());
  } else
  // MIDI out (listening note on only here)
  if (midiSerial.available() ) {
    if ( midiOutParser.parse(  midiSerial.read() ) ) 
        borgTribeSetPartFromNote(midiOutParser.getMidiMsg()[1]) ;
  }
}
