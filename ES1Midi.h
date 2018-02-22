/*

  BORGTRIBE
  A Korg Electribe ES1 Mod allowing playing and recording chromatic notes
  Copyright (C) 2017/2018 by The KikGen labs.

  MIDI FUNCS - HEADER FILE
  
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

#include <arduino.h>
#include <SoftwareSerial.h>
#include "mcp4x.h"
#include "midiXparser.h"

// BorgTribe config
#define BORGTRIBE_CMD_MODE_KEY            12 // Command mode root Key - C0
#define BORGTRIBE_CMD_SET_MODE_KEY        14 // D0           
#define BORGTRIBE_CMD_TOGGLE_VELOCITY_KEY 16 // E0           
#define BORGTRIBE_CMD_CLEAR_PATTERN_KEY   17 // F0
#define BORGTRIBE_CMD_RESET_BORGTRIBE     23 // B0
           
#define BORGTRIBE_CLASSIC_MODE            0
#define BORGTRIBE_MIDIPITCHEDNOTE_MODE    1
#define BORGTRIBE_POTPITCHEDNOTE_MODE     2
#define BORGTRIBE_MODE_MAX                2
#define BORGTRIBE_KEYHOLD_TIME            3000


#define ROOT_KEY 0x24 // C2


// Specific SYSEX for ES-1
#define SYSEX_KORG_ID 0x42
#define SYSEX_ES1_ID 0x57

// PITCH

// This specifies the playback pitch of the sample. Raising the pitch will speed up the playback, and lowering it will slow down the
// playback. The pitch can be adjusted over a range of +/–2 octaves.
// For the Audio In part, this will function as a gate time (duration of the sound) synchronized to the temp

const byte NOTE_TO_POTVAL[] = {
// C     c#    D     d#    E    F     f#   G     g#    A     a#    B
// x     x     x           x    x            x           x          x
   0,    4,    8,    12,   15,  19,   23,   26,   30,   34,  37,   41,
  44,   51,   56,    62,   68,  73,   79,   85,   90,   96,  101,  107,
 128,  143,  148,   154,  160, 166,  171,  177,  183,  189,  194,  200,
 206,  210,  214,   217,  221, 225,  229,  233,  236,  240,  244,  248,
 255
};

const byte NOTE_TO_CCPITCH[] = {
// C     c#    D     d#    E    F     f#   G     g#    A     a#    B
   0,    2,    4,    6,    8,  10,   12,   14,   16,   18,   20,   22,
  24,   27,   30,   33,   36,  39,   42,   45,   48,   51,   54,   57,
  64,   70,   73,   76,   79,  82,   85,   88,   91,   94,   97,  100,
 103,  105,  107,  109,  111, 113,  115,  117,  119,  121,  123,  125,
127
};


// GLOBAL PARAMETERS FROM MIDI
typedef struct 
{
    byte MidiCH;
    byte Metronome;
    byte WriteProtect;
    byte PartsNoteNumber[11];
    byte Clock;
    byte AudioInMode;
    byte dummy[48];
    byte PatternSetParameters[64];
} ES1midiGlobalParameters_t;

// GLOBAL PARAMETERS 
typedef struct 
{
    byte version;    
    ES1midiGlobalParameters_t midiGlobalParameters ;
} ES1GlobalParameters_t;


typedef struct {
  byte sampleNo:7;
  byte sampleMonoSt:1;
  byte filter;
  byte level;
  byte panPot;
  byte pitch;
  byte effect:1;   
  byte roll:1;
  byte reverse:1;
  byte :4;
  byte sampleUse:1;  
} ES1partSampleParameters_t;


typedef struct {
  byte type;
  byte destination;
  byte knobValue[64];
  byte reverseMotionSW[8];
  byte reverseMotionValue[8];
  byte effectMotionSW[8];
  byte effectMotionValue[8];
  byte rollMotionSW[8];
  byte rollMotionValue[8];
} ES1partMotionSequenceData_t;

typedef struct {
  byte dummy1[2] = {0,0} ;   
  ES1partSampleParameters_t     SamplePrm;
  byte                          StepSeqData[8];
  ES1partMotionSequenceData_t   MotionSeqData;    
  byte dummy2[3] = {0,0,0} ;
} ES1partParameters_t;

/////////////////////////////////////////////////
// Funcs
/////////////////////////////////////////////////

bool ES1getGlobalParameters(unsigned int timeout=5000);
bool ES1setGlobalParameters(unsigned int timeout=5000);
void ES1processNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
void ES1processNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
void ES1processCC(uint8_t second, uint8_t third, uint8_t channel);
void ES1processPitchBend(uint8_t LSByte, uint8_t MSByte, uint8_t channel);
void ES1processMidiMsg(uint8_t midiMessage [] );
void ES1processMidiMsg2(uint8_t midiMessage [] );
void ES1sendPitchFromNote(uint8_t note,uint8_t channel);
void borgTribeSetPartFromNote(uint8_t note);
bool borgTribeCommandExec(uint8_t note,bool isOn);
void borgTribeSetMode(uint8_t channel);
void borgTribeFlashPart(uint8_t n,uint8_t channel);

long getSysEx( byte outData[], unsigned maxBuffLen, unsigned long timeout );
void midiUniversalDeviceInquiry() ;
void midiResetAllControllers(uint8_t channel) ;
void midiAllNotesOff(uint8_t channel);
void midiProgramChange(uint8_t channel, uint8_t pp) ;
void midiPitchBendChange(uint8_t channel, int value) ;
void midiStop();

void midiParser(int readByte);

