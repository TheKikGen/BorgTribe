/*
 * BORGTRIBE
  Korg Electribe ES1 Mod

 ES1 specific MIDI and Sysex functions

*/
#include "ES1Midi.h"
#include "mcp4x.h"

using namespace mcp4x;

extern MCP4XXX mcp4251;
extern SoftwareSerial midiSerial;

ES1GlobalParameters_t ES1GlobalParameters;

uint8_t borgTribeMode =   BORGTRIBE_CLASSIC_MODE;
bool    borgTribeVelocityOn = false;
bool    borgTribeCommandKeyPressed = false;
uint8_t borgTribeCurrentPart = 0;
unsigned borgTribeAnalogPotVal = 0;
unsigned borgTribePrevAnalogPotVal = 0; 

/////////////////////////////////////////////////
// ES1 SYSEX  Get midi channel, and global parameters
/////////////////////////////////////////////////
bool ES1getGlobalParameters(unsigned int timeout) {
    int channel = -1;
    unsigned long len=0;
    uint8_t  sysExBuffer[256];
  
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
        decodeSysEx(&sysExBuffer[5],(byte *)&ES1GlobalParameters.midiGlobalParameters , len - 6);
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

  byte sysExBuffer[150];
  uint16_t len;
  uint16_t i;


  // Notes values used for BorgTribe to avoid conflicts

  ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[0] = ROOT_KEY -12  ;
  for ( i=1 ; i<=11 ; i++ ) {    
    ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[i] = ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[i-1]+1;
  }

  len = encodeSysEx((byte *)&(ES1GlobalParameters.midiGlobalParameters),sysExBuffer, sizeof(ES1midiGlobalParameters_t));

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
// Flash the part1 n times
/////////////////////////////////////////////////
void borgTribeFlashPart(uint8_t nn,uint8_t channel) {   
  uint8_t i;
    
  for (i=1; i<= nn ; i++ ) {
    Serial.write(0x90+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[0]);Serial.write(1);  // Part 1 Note Number
    Serial.write(0x90+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[1]);Serial.write(1);  // Part 2 Note Number
    Serial.write(0x90+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[2]);Serial.write(1);  // Part 3 Note Number
    Serial.write(0x90+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[3]);Serial.write(1);  // Part 4 Note Number
    Serial.write(0x90+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[4]);Serial.write(1);  // Part 5 Note Number
    Serial.write(0x90+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[5]);Serial.write(1);  // Part 6 Note Number
    Serial.write(0x90+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[6]);Serial.write(1);  // Part 6A Note Number
    Serial.write(0x90+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[7]);Serial.write(1);  // Part 7A Note Number
    Serial.write(0x90+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[8]);Serial.write(1);  // Part 7B Note Number
    
    // Note off not necessary but stay midi compliant    
    Serial.write(0x80+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[0]);Serial.write(1);  // Part 1 Note Number    
    Serial.write(0x80+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[1]);Serial.write(1);  // Part 2 Note Number
    Serial.write(0x80+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[2]);Serial.write(1);  // Part 3 Note Number
    Serial.write(0x80+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[3]);Serial.write(1);  // Part 4 Note Number
    Serial.write(0x80+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[4]);Serial.write(1);  // Part 5 Note Number
    Serial.write(0x80+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[5]);Serial.write(1);  // Part 6 Note Number
    Serial.write(0x80+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[6]);Serial.write(1);  // Part 6A Note Number
    Serial.write(0x80+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[7]);Serial.write(1);  // Part 7A Note Number
    Serial.write(0x80+channel);Serial.write(ES1GlobalParameters.midiGlobalParameters.PartsNoteNumber[8]);Serial.write(1);  // Part 7B Note Number

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
// Specific ES1 process for Note on
/////////////////////////////////////////////////
void ES1processNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
  unsigned long currentMillis = millis();

  uint8_t velo;
  
  borgTribeSetPartFromNote(note);

  if (note == BORGTRIBE_CMD_MODE_KEY ) borgTribeCommandKeyPressed=true;
  else if ( borgTribeCommandKeyPressed ) {        // C0 is holded : command mode
      // Borgtribe commands
        switch (note) {
          case BORGTRIBE_CMD_SET_MODE_KEY:
             borgTribeSetMode(channel);
             break;
          case BORGTRIBE_CMD_TOGGLE_VELOCITY_KEY:
             borgTribeVelocityOn = !borgTribeVelocityOn ;                                                                
             borgTribeFlashPart(borgTribeVelocityOn +1,channel );
             break;          
        }          
  }    

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
      mcp4251.set(NOTE_TO_POTVAL[note-ROOT_KEY]);
      return;      
  }
  
}

/////////////////////////////////////////////////
// Specific ES1 process for Note off
/////////////////////////////////////////////////
void ES1processNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
  
  if (note == BORGTRIBE_CMD_MODE_KEY ) borgTribeCommandKeyPressed=false;
  
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
  uint8_t i = midiMessage[0];
  
  // Process only message on the ES1 midi channel
  if ( (midiMessage[1] & 0x0F ) !=   ES1GlobalParameters.midiGlobalParameters.MidiCH ) {      
      while ( i ) {
         Serial.write(midiMessage[4-i]); i--;
      }
      return;
  }

  switch (midiMessage[1] >> 4)
  {
    case 0x9: // note On
        ES1processNoteOn(midiMessage[2],midiMessage[3],midiMessage[1] & 0x0F);
        break;
    case 0x8: // note Off
        ES1processNoteOff(midiMessage[2],midiMessage[3],midiMessage[1] & 0x0F);
        break;
    case 0xB: // Control change
        ES1processCC(midiMessage[2],midiMessage[3],midiMessage[1] & 0x0F);
        break;
    case 0xE: // Pitch bend
        ES1processPitchBend(midiMessage[2],midiMessage[3],midiMessage[1] & 0x0F);
        break;
    default: // All others
        while ( i ) {
          Serial.write(midiMessage[4-i]);
          i--;
        }
  }
}


/////////////////////////////////////////////////
// ENCODE 8BITS TO 7BITS SYSEX
/////////////////////////////////////////////////
unsigned encodeSysEx(const byte* inData, byte* outSysEx, unsigned inLength)
{
    unsigned outLength  = 0;     // Num bytes in output array.
    byte count          = 0;     // Num 7bytes in a block.
    outSysEx[0]         = 0;

    for (unsigned i = 0; i < inLength; ++i)
    {
        const byte data = inData[i];
        const byte msb  = data >> 7;
        const byte body = data & 0x7f;

        outSysEx[0] |= (msb << (6 - count));
        outSysEx[1 + count] = body;

        if (count++ == 6)
        {
            outSysEx   += 8;
            outLength  += 8;
            outSysEx[0] = 0;
            count       = 0;
        }
    }
    return outLength + count + (count != 0 ? 1 : 0);
}

/////////////////////////////////////////////////
// DECODE 7BITS SYSEX TO 8BITS
/////////////////////////////////////////////////
unsigned decodeSysEx(const byte* inSysEx, byte* outData, unsigned inLength)
{
    unsigned count  = 0;
    byte msbStorage = 0;
    byte byteIndex  = 0;

    for (unsigned i = 0; i < inLength; ++i)
    {
        if ((i % 8) == 0)
        {
            msbStorage = inSysEx[i];
            byteIndex  = 6;
        }
        else
        {
            const byte body = inSysEx[i];
            const byte msb  = ((msbStorage >> byteIndex--) & 1) << 7;
            outData[count++] = msb | body;
        }
    }
    return count;
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
// Small footprint midi parser
/////////////////////////////////////////////////
void midiParser(int readByte) {

  static uint8_t  midiMessage[4];  // First byte is the length
  static uint8_t  msgLen = 0;
  static bool     sysExMode = false;

  if ( readByte < 0  ) return false;

  // Real time
  if  ( readByte >= 0xF8) {
      Serial.write(readByte);
      return;
  }

  // System exclusive . Stay transparent...
  if  ( readByte >= 0xF0) {
    if ( readByte == 0xF0 ) sysExMode = true;
    else sysExMode = false;
    Serial.write(readByte);
    return;
  }

  // Channel messages
  if ( readByte >= 0x80 ) {
    sysExMode = false;

    // Capture Note on
    if ( (readByte & 0xF0) == 0x90 ) {
      msgLen = 2;
      midiMessage[0] = msgLen+1;
      midiMessage[1] = readByte;
    }
    else

    // Capture Note off
    if ( (readByte & 0xF0) == 0x80 ) {
      msgLen = 2;
      midiMessage[0] = msgLen+1;
      midiMessage[1] = readByte;
    }
    else

    // Capture CC
    if ( (readByte & 0xF0) == 0xB0 ) {
      msgLen = 2;
      midiMessage[0] = msgLen+1;
      midiMessage[1] = readByte;
    }

    // Capture pitch bend
    if ( (readByte & 0xF0) == 0xE0 ) {
      msgLen = 2;
      midiMessage[0] = msgLen+1;
      midiMessage[1] = readByte;
    }
    else Serial.write(readByte);
  }

  else { // Midi Data from 00 to 0X7F

    // Capture the message eventually
    if ( msgLen && !sysExMode ) {
      midiMessage[4-msgLen] = readByte;
      msgLen--;
      if (msgLen == 0) ES1processMidiMsg(midiMessage);
    }
    else Serial.write(readByte);
  }
}

