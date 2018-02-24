/*
  BORGTRIBE
  A Korg Electribe ES1 Mod allowing playing and recording chromatic notes
  Copyright (C) 2017/2018 by The KikGen labs.

  MIDI ES1 FUNCS
  
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

  Encoding and decoding sysex : imported from Arduino midi library.
  Credit to François Best. https://github.com/FortySevenEffects
  Functions were modified due to the inverted encoding logic for Korg synths.
 
*/

#include "ES1Midi.h"

using namespace mcp4x;

extern MCP4XXX mcp4251;
extern SoftwareSerial midiSerial;

ES1GlobalParameters_t       ES1GlobalParameters;
ES1partParameters_t         ES1part1Parameters;

uint8_t  borgTribeMode =   BORGTRIBE_CLASSIC_MODE;
bool     borgTribeVelocityOn = false;
uint8_t  borgTribeCurrentPart = 0;
unsigned borgTribeAnalogPotVal = 0;
unsigned borgTribePrevAnalogPotVal = 0; 
uint8_t  sysExBuffer[256];

// PITCH

// This specifies the playback pitch of the sample. Raising the pitch will speed up the playback, and lowering it will slow down the
// playback. The pitch can be adjusted over a range of +/–2 octaves.
// For the Audio In part, this will function as a gate time (duration of the sound) synchronized to the temp

byte NOTE_TO_POTVAL[] = {
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

byte PITCH_TO_POTVAL[128];

/////////////////////////////////////////////////
// ES1 SYSEX  Get midi channel, and global parameters
/////////////////////////////////////////////////
bool ES1getGlobalParameters(unsigned int timeout) {
    int channel = -1;
    unsigned long len=0;
  
    midiUniversalDeviceInquiry();
    len = getSysEx( sysExBuffer, sizeof(sysExBuffer), timeout );
 
    if (sysExBuffer[1] == 0x7E ) {
      if (len == 15 ) {

      // Device Inquiry
      // Example of reply :
      // F0 7E  00 (<= Current MIDI channel) 06 02 42 (KorkID) 57 (ES-1) 00 00 00 00 01 (Major version) 00 F7
          if (    sysExBuffer[3] == 0x06
               && sysExBuffer[4] == 0x02
               && sysExBuffer[5] == SYSEX_KORG_ID
               && sysExBuffer[6] == SYSEX_ES1_ID
             )

             {
              // We are sure here we are talking to the ES-1
              // Save the current channel, so channel can be set on the Electribe itself
              ES1GlobalParameters.midiGlobalParameters.MidiCH  = sysExBuffer[2] ;
              ES1GlobalParameters.version = sysExBuffer[12];
             }
      }
      else return false;
   }    

   // Get midi global parameters
   Serial.write(0xF0);  Serial.write(0x42);   
   Serial.write(0x30 + ES1GlobalParameters.midiGlobalParameters.MidiCH);
   Serial.write(0x57);   Serial.write(0x0E);   Serial.write(0xF7);
   len = getSysEx( sysExBuffer, sizeof(sysExBuffer), timeout );
   
   if (     sysExBuffer[1] == 0x42
        &&  sysExBuffer[2] == (0x30 + ES1GlobalParameters.midiGlobalParameters.MidiCH )
        &&  sysExBuffer[3] == 0x57 ) 
   {
      // GLOBAL DATA DUMP REQUEST
      if ( sysExBuffer[4]  == 0x51 ) {
          // Decode Sysex Message
        midiXparser::decodeSysEx(&sysExBuffer[5],(byte *)&ES1GlobalParameters.midiGlobalParameters , len - 6,false);
        return true;        
      }
   } 
   
   return false;
}

/////////////////////////////////////////////////
// ES1 set Global Data Dump Request
/////////////////////////////////////////////////
bool ES1setGlobalParameters(unsigned int timeout) {

  // 153 bytes encoded
  uint16_t len;
  uint16_t i;


  // Notes values used for BorgTribe to avoid conflicts

  ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[0] = ROOT_KEY -12  ;
  for ( i=1 ; i<=11 ; i++ ) {    
    ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[i] = ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[i-1]+1;
  }

  len = midiXparser::encodeSysEx((byte *)&(ES1GlobalParameters.midiGlobalParameters),sysExBuffer, sizeof(ES1midiGlobalParameters_t),false);

  Serial.write(0xF0);
  Serial.write(0x42);
  Serial.write(0x30 + ES1GlobalParameters.midiGlobalParameters.MidiCH);
  Serial.write(0x57);
  Serial.write(0x51);
  for (i=0; i<len;i++) {
    Serial.write(sysExBuffer[i]);
  }
  Serial.write(0xF7);

  // Wait the ACK  F0 42 30 57 23 F7
  len = getSysEx( sysExBuffer, sizeof(sysExBuffer), timeout );
  if (len == 6 ) {
    if (     sysExBuffer[0] == 0xF0
         &&  sysExBuffer[1] == 0x42
         &&  sysExBuffer[2] == (0x30 + ES1GlobalParameters.midiGlobalParameters.MidiCH)
         &&  sysExBuffer[3] == 0x57
         &&  sysExBuffer[4] == 0x23 )
    return true;    
  }
  
  return  false;
}


/////////////////////////////////////////////////
// ES1 Clear current pattern
/////////////////////////////////////////////////
void ES1clearCurrentPattern() {
   int i,j,len;
   int size=0;
   byte buffIn[14];
   byte buffOut[150];
   
   // Due to the Arduino memory limitation, we must set pattern parameters by 7 bytes block
   
   // Get midi global parameters
   Serial.write(0xF0);  Serial.write(0x42);   
   Serial.write(0x30 );
   Serial.write(0x57);   Serial.write(0x40);   

   // Bloc 1 - 7 Bytes => 8 bytes
   buffIn[0] =   0X3C   ;// 0- Tempo MSB
   buffIn[1] =   00     ;// 1- Tempo LSB
   buffIn[2] =   0x00   ;// 2- Roll Type - Scale beat - Length
   buffIn[3] =   0      ;// 3- Swing
   buffIn[4] =   0      ;// 4- Effect type
   buffIn[5] =   0      ;// 5- Effect Edit 1
   buffIn[6] =   0      ;// 6- Effect Edit 2  
   buffIn[7] =   0      ;//  7- Effect motion Seq stat
   buffIn[8] =   0      ;//  8- Delay Depth
   buffIn[9] =   0      ;//  9- Delay Time
   buffIn[10] =  0      ;// 10- Delay BPM sync / Motion Seq
   buffIn[11] =  40     ;// 11- Accent level / Motion SEQ

   ///////
   buffIn[12] =  0X80   ;// 12- (NO Fx Edit 1 MotionSEQ Motion OFF )
   buffIn[13] =  0X80   ;// 13- (       "            )

   size += (len = midiXparser::encodeSysEx(buffIn,buffOut, 14,false) );
   for (i = 0 ; i<len ; i++) Serial.write(buffOut[i]);

   // Write 36 block * (1+7) 
   for (i = 1 ; i<= 36; i++) {
        Serial.write(0x7F);
        for (j=1; j <=7 ; j++) Serial.write(0);
   }
   size += 288;

   // We are only able to write Part1 parameters eventually
   // The struct is padded to be aligned on 7 bytes
   // Part 1 parameters. 128 bytes 
   // Pos 266 + 2 dummy bytes before + 3 dummy bytes after part1 params
   memset(&ES1part1Parameters,0x00,sizeof(ES1part1Parameters)); 
   ES1part1Parameters.SamplePrm.sampleNo  = 0;
   ES1part1Parameters.SamplePrm.filter    = 127;
   ES1part1Parameters.SamplePrm.level     = 127;
   ES1part1Parameters.SamplePrm.panPot    = 64;
   ES1part1Parameters.SamplePrm.pitch     = 64;
   ES1part1Parameters.SamplePrm.reverse   = 0;
   ES1part1Parameters.SamplePrm.roll      = 0;
   ES1part1Parameters.SamplePrm.effect    = 0;
   ES1part1Parameters.StepSeqData[0]      = B00010001;
   ES1part1Parameters.StepSeqData[1]      = B00010001;
   
   len = midiXparser::encodeSysEx((byte *)&ES1part1Parameters,buffOut, sizeof(ES1part1Parameters),false);   
   for (i = 0 ; i<len ; i++) { Serial.write(buffOut[i]);  }
   size += len;

   size = 1980 - size;
   // Fill the rest of the sysex message
   for ( i = 1 ; i<=size ; i++ ) { Serial.write(0x00);  }

   Serial.write(0xF7);
  
}
/////////////////////////////////////////////////
// ES1 send Pitch From Note
/////////////////////////////////////////////////
void ES1sendPitchFromNote(uint8_t note,uint8_t channel) {
  
  Serial.write(0xB0+channel);Serial.write(0x63);Serial.write(0x05);                  // NRPN MSB   
  Serial.write(0xB0+channel);Serial.write(0x62);Serial.write(borgTribeCurrentPart*8);      // NRPN LSB PITCH
  Serial.write(0xB0+channel);Serial.write(0x06);Serial.write(NOTE_TO_CCPITCH[note-ROOT_KEY]); // Data Entry MSB 
}

/////////////////////////////////////////////////
// Set current BorgTribe mode
/////////////////////////////////////////////////
void borgTribeSetMode(uint8_t channel) {
  
  uint8_t i;
  
  if ( ++ borgTribeMode >  BORGTRIBE_MODE_MAX ) borgTribeMode = 0 ;
  // Send note to confirm command by flashing part 1 a number of time corresponding to the mode
  // Set globals for BorgTribe command keys 7A/7B

  borgTribeFlashPart((borgTribeMode+1),channel);

}

/////////////////////////////////////////////////
// Flash the parts n times
/////////////////////////////////////////////////
void borgTribeFlashPart(uint8_t nn,uint8_t channel) {   
  uint8_t i,j;
    
  for (i=1; i<= nn ; i++ ) {
    for (j=0; j<= 8 ; j++ ) {
        Serial.write(0x90+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[j]);Serial.write(1);  // Part j Note Number   
        // Note off not necessary but stay midi compliant    
        Serial.write(0x80+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[j]);Serial.write(1);  // Part j Note Number    
    }
    delay(250);
  }
}


/////////////////////////////////////////////////
// Get the current part from Note on
/////////////////////////////////////////////////
void borgTribeSetPartFromNote(uint8_t note) {   
  if (note >= ROOT_KEY -12 && note < ROOT_KEY-1  ) {    
    borgTribeCurrentPart = note + 12 - ROOT_KEY  ;
  }
}
/////////////////////////////////////////////////
// Software reset 
/////////////////////////////////////////////////
void(* borgTribeReset) (void) = 0; //declare reset function @ address 0

/////////////////////////////////////////////////
// BorgTribe commands from midi keyboard
/////////////////////////////////////////////////
bool borgTribeCommandExec(uint8_t note,uint8_t channel,bool isOn){
  static bool commandModeEnabled = false;
  static int  commandToConfirm = -1;

  if (note == BORGTRIBE_CMD_MODE_KEY ) {
    commandModeEnabled = isOn ;
    if ( !commandModeEnabled ) {
        commandToConfirm = -1;        
    }
    return true;
  }

  if  ( ! commandModeEnabled ) return false;

  // Borgtribe commands
  
  switch (note) {
    case BORGTRIBE_CMD_SET_MODE_KEY:
       if (isOn) borgTribeSetMode(channel);
       break;
    
    case BORGTRIBE_CMD_TOGGLE_VELOCITY_KEY:
       if (isOn) {
          borgTribeVelocityOn = !borgTribeVelocityOn ;                                                                
          borgTribeFlashPart(borgTribeVelocityOn +1,channel );
       }
       break;  
  
    case BORGTRIBE_CMD_CLEAR_PATTERN_KEY:
       if (isOn ) {
          if (commandToConfirm == BORGTRIBE_CMD_CLEAR_PATTERN_KEY ) {
              ES1clearCurrentPattern();
              commandToConfirm = -1;
          } else {
            commandToConfirm = BORGTRIBE_CMD_CLEAR_PATTERN_KEY;
            borgTribeFlashPart(5,channel );
          }
       }
       break;                                  

    case BORGTRIBE_CMD_AUTO_TUNE_KEY:
       if (isOn ) {
          if (commandToConfirm == BORGTRIBE_CMD_AUTO_TUNE_KEY ) {
              commandToConfirm = -1;
              borgTribeAutoTune();              
              borgTribeFlashPart(2,channel ); 
          } else {
            commandToConfirm = BORGTRIBE_CMD_AUTO_TUNE_KEY;
            borgTribeFlashPart(5,channel );
          }
       }
       break;                                  

    case BORGTRIBE_CMD_RESET_BORGTRIBE:
       if (isOn ) {
          if (commandToConfirm == BORGTRIBE_CMD_RESET_BORGTRIBE ) {
              commandToConfirm = -1;
              borgTribeReset();              
              //(....) software reset.....
 
          } else {
            commandToConfirm = BORGTRIBE_CMD_RESET_BORGTRIBE;
            borgTribeFlashPart(5,channel );
          }
       }
       break;                                  

       
  }          

  return true;
}

/////////////////////////////////////////////////
// BorgTribe autotune
// Adjust the tune pot table from the midi pitch notes table
// NOT REALLY EFFICIENT.....NOT IMPLEMENTED.
/////////////////////////////////////////////////
void borgTribeAutoTune() {


  int pos=0;
  uint8_t j=0;
  uint8_t count=0;
  uint8_t value = 0;
  unsigned long currentMillis;
 
  delay(5000);

  memset(PITCH_TO_POTVAL,0xFF,sizeof(PITCH_TO_POTVAL)); 
  midiXparser midiOutParser(midiXparser::controlChangeMsk,ES1GlobalParameters.midiGlobalParameters.MidiCH ) ;  

  mcp4251.set(0);
  delay(10);
  mcp4251.set(255);
  
  // Clear CC
  currentMillis = millis();
  while ( midiSerial.available() && millis() < currentMillis + 500 ) 
         midiOutParser.parse(  midiSerial.read() ) ;                     

  for ( pos = 0; pos <=255 ; pos++ )  {

     mcp4251.set(pos);
     delay(10);
     Serial.write(0x90+ES1GlobalParameters.midiGlobalParameters.MidiCH);Serial.write(ROOT_KEY-12 + borgTribeCurrentPart);Serial.write(100);
     currentMillis = millis(); 
     while ( millis() < (currentMillis + 50) ) {
        if (midiSerial.available() ) {
          if ( midiOutParser.parse(  midiSerial.read() ) ) {
            if (midiOutParser.getMidiMsg()[1] == 0x06) {                
              value = midiOutParser.getMidiMsg()[2];
              // Set the pos on the average pos if already filled
              //if ( POTVAL_PITCH[value] == 0xFF ) POTVAL_PITCH[value] = pos;
                // else POTVAL_PITCH[value] = round((POTVAL_PITCH[value] + pos) / 2.0);
              
              PITCH_TO_POTVAL[value] = pos;
              break;
            }
          }
        }  
     }
  }

  // Pass 2 : fine tuning

  // Clear CC
    pos = 0;
  mcp4251.set(128);
  delay(10);

  
  currentMillis = millis();
  while ( midiSerial.available() && millis() < currentMillis + 500 ) 
         midiOutParser.parse(  midiSerial.read() ) ;                     
  

 
  while ( pos <126) { 
    pos = 0 ;
    for (j=0; j<sizeof(PITCH_TO_POTVAL) ; j++  ) {
    
       mcp4251.set(PITCH_TO_POTVAL[j]);
       delay(10);
        Serial.write(0x90+ES1GlobalParameters.midiGlobalParameters.MidiCH);Serial.write(ROOT_KEY-12 + borgTribeCurrentPart);Serial.write(100);
       value = 0;  
       currentMillis = millis(); 
       while ( millis() < (currentMillis + 50) ) {
          if (midiSerial.available() ) {
            if ( midiOutParser.parse(  midiSerial.read() ) ) {
              if (midiOutParser.getMidiMsg()[1] == 0x06) {                
                value = 1;
                pos++;
                // Set the pos on the average pos if already filled
                //if ( POTVAL_PITCH[value] == 0xFF ) POTVAL_PITCH[value] = pos;
                  // else POTVAL_PITCH[value] = round((POTVAL_PITCH[value] + pos) / 2.0);              
                break;
              }
            }
          }  
       }
       if (value = 0) PITCH_TO_POTVAL[j]++;   
    }
    // Force 0 to restart 
    PITCH_TO_POTVAL[0]= 0;   
 }
  
 
  for ( j = 0; j <= sizeof(NOTE_TO_CCPITCH) ; j++ )  {
      
      value = NOTE_TO_CCPITCH[j];
      NOTE_TO_POTVAL[j] = PITCH_TO_POTVAL[value];
      
  }
  
}
/////////////////////////////////////////////////
// Specific ES1 process for Note on
/////////////////////////////////////////////////
void ES1processNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
  unsigned long currentMillis = millis();

  uint8_t velo;
  int value;
  
  
  borgTribeSetPartFromNote(note);

  if (borgTribeCommandExec(note,channel,true)) return;
  
  // Case of Note off beeing note on at a zero velocity
  if (velocity == 0) {
    ES1processNoteOff(note,velocity,channel);
    return;
  }

  if (borgTribeVelocityOn) velo = velocity; else velo = 127;

  // Check Modes
  // Clasic mode : do nothing
  if ( borgTribeMode == BORGTRIBE_CLASSIC_MODE) {
        Serial.write(0x90+channel);Serial.write(note);Serial.write(velo);
        return;  
  }
        
  // Check if we are in the pitch table range, else send the note.
  if (note < ROOT_KEY || note >= ( ROOT_KEY + sizeof(NOTE_TO_CCPITCH) ) ) {
      Serial.write(0x90+channel);Serial.write(note);Serial.write(velo);
      return;
  }
   
  // Play MIDI pitched note. NB : Midi pitched note are not recorded by the ES1 
  if ( borgTribeMode == BORGTRIBE_MIDIPITCHEDNOTE_MODE ) {            
    ES1sendPitchFromNote(note,channel);
    Serial.write(0x90+channel);Serial.write(ROOT_KEY-12 + borgTribeCurrentPart);Serial.write(velo);
    return;    
  }

  // Move the digital potentiometer to the right pitch
  if ( borgTribeMode == BORGTRIBE_POTPITCHEDNOTE_MODE ) {
//      value= NOTE_TO_POTVAL[note-ROOT_KEY]-mcp4251.get() ;  
//      if ( abs(value) < 4) mcp4251.set( value >0?mcp4251.get()-4:mcp4251.get()+4  );
//      delay(1);     
      mcp4251.set(NOTE_TO_POTVAL[note-ROOT_KEY]);
      delay(2);
      Serial.write(0x90+channel);Serial.write(ROOT_KEY-12 + borgTribeCurrentPart);Serial.write(velo);
      return;      
  }
  
}

/////////////////////////////////////////////////
// Specific ES1 process for Note off
/////////////////////////////////////////////////
void ES1processNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
  
  if (borgTribeCommandExec(note,channel,false)) return;
  Serial.write(0x80+channel);Serial.write(note);Serial.write(velocity);
  
}

/////////////////////////////////////////////////
// Specific ES1 process for CC
/////////////////////////////////////////////////
void ES1processCC(uint8_t second, uint8_t third, uint8_t channel) {
static uint8_t nprnMSB      = 0xFF;
static uint8_t nprnLSB      = 0xFF;
static uint8_t dataEntryMSB = 0xFF;

  switch (second)
  {
    case 0X01: // Modulation Wheel
     mcp4251.set(third*2);
     break;
    case 0x06: // Data entry MSB
    case 0x63: // NRPN MSB
    case 0x62: // NRPN LSB
    default:
        Serial.write(0xB0+channel);Serial.write(second);Serial.write(third);
  }
}

/////////////////////////////////////////////////
// Specific ES1 process for Pitch bend
/////////////////////////////////////////////////
void ES1processPitchBend(uint8_t LSByte, uint8_t MSByte, uint8_t channel) {

  // Set the potentiometer with the pitch bend wheel MSB value
  mcp4251.set(MSByte*2); 
//  Serial.write(0xE0+channel);Serial.write(LSByte);Serial.write(MSByte);

}

/////////////////////////////////////////////////
// Specific ES1 MIDI Messages processing
/////////////////////////////////////////////////
void ES1processMidiMsg(uint8_t midiMessage [] ) {
 
  switch (midiMessage[0] >> 4)
  {
    case 0x9: // note On
        ES1processNoteOn(midiMessage[1],midiMessage[2],midiMessage[0] & 0x0F);
        break;
    case 0x8: // note Off
        ES1processNoteOff(midiMessage[1],midiMessage[2],midiMessage[0] & 0x0F);
        break;
    case 0xB: // Control change
        ES1processCC(midiMessage[1],midiMessage[2],midiMessage[0] & 0x0F);
        break;
    case 0xE: // Pitch bend
    
        ES1processPitchBend(midiMessage[1],midiMessage[2],midiMessage[0] & 0x0F);
        break;
  }
}


/////////////////////////////////////////////////
// GET A SYSEX REPLY MSG
/////////////////////////////////////////////////
long getSysEx( byte outData[], unsigned maxBuffLen, unsigned long timeout ) {

  unsigned long currentMillis = millis();
  unsigned len=0;
  int readByte=0;

  if (maxBuffLen < 2) return -1;

  // Wait SYSEX Start
  while ( ( readByte = midiSerial.read() )  != 0xF0 && ( millis()  < (currentMillis + timeout) ) );
  if (readByte != 0xF0 ) return -1;
  outData[len++] = readByte;

  // Store SYSEX msg in the buffer
  currentMillis = millis();

  while ( (readByte = midiSerial.read() ) != 0xF7 && ( millis() < (currentMillis + timeout )  ) ) {
      if ( readByte >= 0 && readByte <= 0x7F ) {
        if ( len >= maxBuffLen ) return -1;
        outData[len++] = readByte ;
      }
  }

  if (readByte == 0xF7) {
    outData[len++] = readByte ;
    return len;
  }
  return -1;
}

/////////////////////////////////////////////////
// Universal Device Inquiry (generic)
/////////////////////////////////////////////////
void midiUniversalDeviceInquiry() {
  Serial.write(0xF0);
  Serial.write(0x7E);
  Serial.write(0x7F);
  Serial.write(0x06);
  Serial.write(0x01);
  Serial.write(0xF7);
}

/////////////////////////////////////////////////
// Reset all controllers
/////////////////////////////////////////////////
void midiResetAllControllers(uint8_t channel) {
  Serial.write(0xB0+channel);Serial.write(0x79);Serial.write(0x00);
}

/////////////////////////////////////////////////
// All notes off
// This message force Note Off to Audio In Part only.
// Do not work P1-P7B,Slice part.
/////////////////////////////////////////////////
void midiAllNotesOff(uint8_t channel) {
  Serial.write(0xB0+channel);Serial.write(0x7B);Serial.write(0x00);
}

/////////////////////////////////////////////////
// Program change. From 0 to 0x7F
/////////////////////////////////////////////////
void midiProgramChange(uint8_t channel, uint8_t pp) {
  Serial.write(0xC0+channel);Serial.write(pp);
}

/////////////////////////////////////////////////
// Pitch bend change - Value is +/- 8192
/////////////////////////////////////////////////
void midiPitchBendChange(uint8_t channel, int value) {
    unsigned int change = 0x2000 + value;  //  0x2000 == No Change

    // Low, High
    Serial.write(0xE0 + channel);
    Serial.write(change & 0x7F); Serial.write ( (change >> 7) & 0x7F );
}

/////////////////////////////////////////////////
// Real time - STOP
/////////////////////////////////////////////////

void midiStop() {
  Serial.write(0xFC);
}


