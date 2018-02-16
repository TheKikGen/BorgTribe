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

#define RX 2
#define TX 3

SoftwareSerial midiSerial(RX, TX); // RX, TX

#define ROOT_KEY 0x24 // C2

// Specific SYSEX for ES-1 
#define SYSEX_KORG_ID 0x42
#define SYSEX_ES1_ID 0x57


using namespace mcp4x;
// Digital potentiometer.
MCP4XXX mcp4251;

int analogVal   = 0;  
int previousVal = 0;  
float r=0; // Analog ratio
int maxVal = 0;

uint8_t ES1CurrentPart = 0xFF;   
uint8_t ES1MidiChannel = 0xFF;
uint8_t ES1Version = 0XFF;
byte    ES1SysExBuffer[256];


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


struct
{
    byte MidiCH;
    byte Metronome;
    byte WriteProtect;
    byte Part1NoteNumber;
    byte Part2NoteNumber;
    byte Part3NoteNumber;
    byte Part4NoteNumber;
    byte Part5NoteNumber;
    byte Part6ANoteNumber;
    byte Part6BNoteNumber;
    byte Part7ANoteNumber;
    byte Part7BNoteNumber;    
    byte SliceNoteNumber;
    byte AudioInNoteNumber;
    byte Clock;
    byte AudioInMode;
    byte dummy[48];
    byte PatternSetParameters[64];
} stGlobalParameters;

/////////////////////////////////////////////////
// SYSTEM EXCLUSIVE
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


long getES1SysEx( byte outData[], unsigned maxBuffLen, unsigned long timeout ) {

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

void processES1SysExMsg(uint8_t sysExBuffer[], unsigned long msgLen) {

  uint8_t  sysExDecodedBuffer[256];
  
  // Non realtime
  if (sysExBuffer[1] == 0x7E ) {
      if (msgLen == 15 ) {

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
              ES1MidiChannel = sysExBuffer[2] ;
              ES1Version = sysExBuffer[12];
             }
      }
  }

  else
  if ( sysExBuffer[1] == 0x42 && sysExBuffer[2] == (0x30 + ES1MidiChannel) && sysExBuffer[3] == 0x57 ) {

      // GLOBAL DATA DUMP REQUEST
      if ( sysExBuffer[4]  == 0x51 ) {
          // Decode Sysex Message          
        decodeSysEx(&sysExBuffer[5],(byte *)&stGlobalParameters , msgLen - 6);
      }    
  }    
}

void midiUniversalDeviceInquiry() {
  Serial.write(0xF0);
  Serial.write(0x7E);
  Serial.write(0x7F);
  Serial.write(0x06);
  Serial.write(0x01);
  Serial.write(0xF7);
}


void midiGlobalDataDumpRequest(uint8_t channel) {
  Serial.write(0xF0);
  Serial.write(0x42);
  Serial.write(0x30 + channel);
  Serial.write(0x57);
  Serial.write(0x0E);
  Serial.write(0xF7);
}

void setGlobalParameters(uint8_t channel) {
  
  // 153 bytes encoded
  
  byte sysExBuffer[150];
  uint16_t len; 
  uint16_t i; 


  // Notes values used for BorgTribe to avoid conflicts
  
  stGlobalParameters.Part1NoteNumber    = ROOT_KEY -12  ;
  stGlobalParameters.Part2NoteNumber    = stGlobalParameters.Part1NoteNumber +1 ;
  stGlobalParameters.Part3NoteNumber    = stGlobalParameters.Part2NoteNumber +1 ;
  stGlobalParameters.Part4NoteNumber    = stGlobalParameters.Part3NoteNumber +1 ;
  stGlobalParameters.Part5NoteNumber    = stGlobalParameters.Part4NoteNumber +1 ;
  stGlobalParameters.Part6ANoteNumber   = stGlobalParameters.Part5NoteNumber +1 ;
  stGlobalParameters.Part6BNoteNumber   = stGlobalParameters.Part6ANoteNumber +1 ;
  stGlobalParameters.Part7ANoteNumber   = stGlobalParameters.Part6BNoteNumber +1 ;
  stGlobalParameters.Part7BNoteNumber   = stGlobalParameters.Part7ANoteNumber +1 ;    
  stGlobalParameters.SliceNoteNumber    = stGlobalParameters.Part7BNoteNumber +1 ;
  stGlobalParameters.AudioInNoteNumber  = stGlobalParameters.SliceNoteNumber +1 ;
  
  len = encodeSysEx((byte *)&stGlobalParameters,sysExBuffer, sizeof(stGlobalParameters));
  
  Serial.write(0xF0);
  Serial.write(0x42);
  Serial.write(0x30 + channel);
  Serial.write(0x57);
  Serial.write(0x51);
  for (i=0; i<len;i++) {
    Serial.write(sysExBuffer[i]);
  }
  Serial.write(0xF7);

  // Wait the ACK  F0 42 30 57 23 F7
  
}

// Reset all controllers 
void midiResetAllControllers(uint8_t channel) {
  Serial.write(0xB0+channel);Serial.write(0x79);Serial.write(0x00);
}

// All notes off
// This message force Note Off to Audio In Part only.
// Do not work P1-P7B,Slice part.
void midiAllNotesOff(uint8_t channel) {
  Serial.write(0xB0+channel);Serial.write(0x7B);Serial.write(0x00);
}

// Program change. From 0 to 0x7F
void midiProgramChange(uint8_t channel, uint8_t pp) {
  Serial.write(0xC0+channel);Serial.write(pp);
}

// Pitch bend change - Value is +/- 8192
void pitchBendChange(uint8_t channel, int value) {
    unsigned int change = 0x2000 + value;  //  0x2000 == No Change

    // Low, High
    Serial.write(0xE0 + channel);
    Serial.write(change & 0x7F); Serial.write ( (change >> 7) & 0x7F );
}

void processNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {

  uint8_t pitch =0;
  uint8_t semiTone=0;
  int val =0;
  static uint8_t lastNote=0; 
      
  // Case of Note off beeing note on at a zero velocity
  if (velocity == 0) {
    processNoteOff(note,velocity,channel);
    return;
  }
                
  // Check if we are in the pitch table range, and if not send the note. 
  // That's allow to play part with a keyboard as usual, without changing the pitch.
  if (note < ROOT_KEY || note >= ( ROOT_KEY + sizeof(NOTE_TO_CCPITCH) ) ) {
      Serial.write(0x90+channel);Serial.write(note);Serial.write(velocity);
      return;     
  }

  
//  Serial.write(0xB0); Serial.write(0x63); Serial.write(0x05); // Pitch
//  Serial.write(0xB0); Serial.write(0x62); Serial.write(0x20); // part 5
//  Serial.write(0xB0); Serial.write(0x06); Serial.write(NOTE_TO_PITCH2[note-ROOT_KEY]); // data entry
//  //Serial.write(0xB0); Serial.write(0x06); Serial.write(note); // data entry

  
// mcp4251.set(128);    
// delay(2);
//
// if (lastNote < note) val = round(NOTE_TO_PITCH[note-ROOT_KEY]*2 - 5);    
// else if (lastNote > note) val = round(NOTE_TO_PITCH[note-ROOT_KEY]*2 +5);    
// else  val = round(NOTE_TO_PITCH[note-ROOT_KEY]*2);       
//
// mcp4251.set(val);    
// delay(1);    
 
  mcp4251.set(NOTE_TO_POTVAL[note-ROOT_KEY]);    

  //Serial.write(0x90+channel);Serial.write(0x1C);Serial.write(velocity);
  return;     
}

void processNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
  
  // Check if we are in the pitch table range, and if not send the note. 
  // That's allow to play part with a keyboard as usual, without changing the pitch.
  if (note < ROOT_KEY || note >= ( ROOT_KEY + sizeof(NOTE_TO_CCPITCH) ) ) {
      Serial.write(0x80+channel);Serial.write(note);Serial.write(velocity);
      return;     
  }

}

/*
000014D0   7  --     B0    63    05    1  ---  CC: NRPN MSB          
000014D1   7  --     B0    62    6B    1  ---  CC: NRPN LSB          
000014D1   7  --     B0    06    00    1  ---  CC: Data Entry MSB    
000014D3   7  --     B0    63    05    1  ---  CC: NRPN MSB          
000014D3   7  --     B0    62    6C    1  ---  CC: NRPN LSB          
000014D5   7  --     B0    06    00    1  ---  CC: Data Entry MSB    
000014D5   7  --     90    18    7F    1  C  1 Note On               
00001539   7  --     80    18    40    1  C  1 Note Off              
*/
// Bn | 06 (06) | dd | Data Entry(MSB) [TABLE1]|
// Bn | 62 (98) | nl | NRPN LSB [TABLE1]|
// Bn | 63 (99) | nm | NRPN MSB [TABLE1]|


// Each time, a part is elected on the Electribe, the following messages are sent :

// 00002292   7  --     80    18    40    1  C  1 Note Off              
// 000031E0   7  --     B0    63    05    1  ---  CC: NRPN MSB          
// 000031E6   7  --     B0    62    6B    1  ---  CC: NRPN LSB          
// 000031E6   7  --     B0    06    00    1  ---  CC: Data Entry MSB    

// 000031E8   7  --     B0    63    05    1  ---  CC: NRPN MSB          
// 000031E8   7  --     B0    62    6C    1  ---  CC: NRPN LSB          
// 000031EA   7  --     B0    06    00    1  ---  CC: Data Entry MSB    



void processCC(uint8_t second, uint8_t third, uint8_t channel) {

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
// PITCH BEND
/////////////////////////////////////////////////
void processPitchBend(uint8_t LSByte, uint8_t MSByte, uint8_t channel) {
  
  // Set the potentiometer with the pitch bend wheel MSB value
  mcp4251.set(MSByte*2);

//  Serial.write(0xE0+channel);Serial.write(LSByte);Serial.write(MSByte);
 
}

/////////////////////////////////////////////////
// Process MIDI Messages
/////////////////////////////////////////////////
void processMidiMsg(uint8_t midiMessage [] ) {

  switch (midiMessage[1] >> 4)
  {
    case 0x9: // note On
        processNoteOn(midiMessage[2],midiMessage[3],midiMessage[1] & 0x0F);
        break;
    case 0x8: // note Off
        processNoteOff(midiMessage[2],midiMessage[3],midiMessage[1] & 0x0F);    
        break;
    case 0xB: // Control change
        processCC(midiMessage[2],midiMessage[3],midiMessage[1] & 0x0F);    
        break;
    case 0xE: // Pitch bend
        processPitchBend(midiMessage[2],midiMessage[3],midiMessage[1] & 0x0F);    
        break;
       
    default: // All others
        uint8_t i = midiMessage[0];
        while ( i ) {
          Serial.write(midiMessage[4-i]);
          i--;
        }                          
  }
  
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
      if (msgLen == 0) processMidiMsg(midiMessage);
    } 
    else Serial.write(readByte);
  }    
}


/////////////////////////////////////////////////
// MAIN
/////////////////////////////////////////////////

void setup() {

  Serial.begin(31250);      // Midi baud rate to MIDI IN of ES1
  midiSerial.begin(31250);  // used to read SysEx from midi out when necessary
  
  // Wait for Electribe ready and get the current channel
  delay(5000);
  
  byte readByte;
  unsigned long len=0;
  ES1MidiChannel == 0xFF ;
  memset(&stGlobalParameters,0xFF,sizeof(stGlobalParameters));
  
  // We can't go further without the MIDI channel
  while ( ES1MidiChannel == 0xFF ) {
    midiUniversalDeviceInquiry();      
    len = getES1SysEx( ES1SysExBuffer, sizeof(ES1SysExBuffer), 5000 );  
    processES1SysExMsg(ES1SysExBuffer, len);
    delay(1000);
  }

  // Get current parameters   
  midiGlobalDataDumpRequest(ES1MidiChannel); 
  len = getES1SysEx( ES1SysExBuffer, sizeof(ES1SysExBuffer), 1000 );
  processES1SysExMsg(ES1SysExBuffer, len);

  // Set parameters for BorgTribe (notes number)
  setGlobalParameters(ES1MidiChannel);

  // Digital potentiometer init
  mcp4251.begin();
  maxVal = mcp4251.max_value()-1;
  r = 1023.00 / maxVal;
  analogVal = previousVal = analogRead(2);
}


void loop() {

  // Listen analog Pitch potentiometer and change value if moved
  analogVal = analogRead(2) / r;
  if ( analogVal != previousVal ) {
     previousVal = analogVal;
     mcp4251.set(constrain(analogVal,0,maxVal));
  }

  if (Serial.available() ) midiParser(Serial.read());  

}

