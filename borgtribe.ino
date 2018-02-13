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
#include "mcp4x.h"

using namespace mcp4x;

#define ROOT_KEY 0x24 // C2
#define PITCH_PRM


// Digital potentiometer.
MCP4XXX mcp4251;



int analogVal   = 0;  
int previousVal = 0;  
float r=0; // Analog ratio
int maxVal = 0;

int readByte=0;
uint8_t msgLen=0;
uint8_t CurrentPart = 0;


// PITCH

// This specifies the playback pitch of the sample. Raising the pitch will speed up the playback, and lowering it will slow down the
// playback. The pitch can be adjusted over a range of +/–2 octaves.
// For the Audio In part, this will function as a gate time (duration of the sound) synchronized to the temp


const byte NOTE_TO_POT[] = {
// C     c#    D     d#    E    F     f#   G     g#    A     a#    B   
// x     x     x           x    x            x           x          x
   0,    4,    8,    12,   15,  19,   23,   26,   30,   34,  37,   41, 
  44,   51,   56,    62,   68,  73,   79,   85,   90,   96,  101,  107,
 128,  143,  148,   154,  160, 166,  171,  177,  183,  189,  194,  200,
 206,  210,  214,   217,  221, 225,  229,  233,  236,  240,  244,  248,
 255
};



const byte NOTE_TO_PITCH[] = {
// C     c#    D     d#    E    F     f#   G     g#    A     a#    B   
   0,    2,    4,    6,    8,  10,   12,   14,   16,   18,   20,   22, 
  24,   27,   30,   33,   36,  39,   42,   45,   48,   51,   54,   57,
  64,   70,   73,   76,   79,  82,   85,   88,   91,   94,   97,  100,
 103,  105,  107,  109,  111, 113,  115,  117,  119,  121,  123,  125,
127
};



byte toneTable[128];










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
  if (note < ROOT_KEY || note >= ( ROOT_KEY + sizeof(NOTE_TO_PITCH) ) ) {
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
 
  mcp4251.set(NOTE_TO_POT[note-ROOT_KEY]);    
 

  //Serial.write(0x90+channel);Serial.write(0x1C);Serial.write(velocity);
  return;     
}

void processNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
  
  // Check if we are in the pitch table range, and if not send the note. 
  // That's allow to play part with a keyboard as usual, without changing the pitch.
  if (note < ROOT_KEY || note >= ( ROOT_KEY + sizeof(NOTE_TO_PITCH) ) ) {
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

void processCC(uint8_t second, uint8_t third, uint8_t channel) {

  switch (second)
  {
//    case 0X01: // Modulation Wheel   
//      break;

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
  
  static uint8_t midiMessage[4]; // First byte is the length

  if ( readByte < 0  ) return false;
        
  // Real time
  if  ( readByte >= 0xF0 ) Serial.write(readByte); 
  else 

  // Channel messages  
  if ( readByte >= 0x80 ) {       

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
    if (msgLen) {
      midiMessage[4-msgLen] = readByte;
      msgLen--;
      if (msgLen == 0) processMidiMsg(midiMessage);
    } else Serial.write(readByte);
  }    

}



/////////////////////////////////////////////////
// MAIN
/////////////////////////////////////////////////

void setup() {
  Serial.begin(31250); // Midi baud rate
  // Serial.begin(115200); // Midi baud rate

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


  
  
  
  

//  
//  analogVal = analogRead(2)/r;
//  Serial.write("Val = ");
//  Serial.print(analogVal);
//  Serial.print(" Val/2 = ");
//  Serial.println(analogVal/2);
//
//  mcp4251.write(0, analogVal);

// 

//  for (int i=0; i<=255; i++) { 
//    mcp4251.set(i); //format: <instance>write(pot, value)....pot is either 0 or 1, value is 0-255
//    delay(10);
//    //Serial.write(90); Serial.write(00); Serial.write(i); // data entry     
//    Serial.print("Val = "); Serial.print(i);
//    Serial.print(" Get = "); Serial.println(mcp4251.get());
//    delay(1500);
//    
//    //delay(100); //delay to make it visible
//  }
//  delay(10000);
//int i,j;
//  for (i=0; i<=255; i++) { //and fade back down from 255 to 0
//
//    if (i == 0) {
//      Serial.println("=== === === === === === === === === ");
//      j = 0;
//    }
//  
//    mcp4251.set(i );
//    delay(10);
//    //Serial.print(" Analog =");Serial.print(analogRead(2));
//    Serial.print(" T = "); Serial.println(i);
//    //Serial.print(" Val = "); Serial.print(r*i,4+r/3.0);Serial.print(" rounded =  ");Serial.println(round(r*i +r/3.0));    
//    delay(1000); //delay to make it visible
//    if ( Serial.read() >= 0 ) { toneTable[j++] = i; Serial.println("=============> TABLE");}
//  }
//  // generate Table
//
//  for ( i=0; i<j ; i++) {
//    Serial.print("0x");Serial.print(toneTable[i]);Serial.print(",");    
//  }
// delay(10000);
 
//  int i,j;
//  for (i=0; i<=127; i++) { 
//
//    Serial.write(0xB0); Serial.write(0x63); Serial.write(0x05); // Pitch
//    Serial.write(0xB0); Serial.write(0x62); Serial.write(0x20);
//    Serial.write(0xB0); Serial.write(0x06); Serial.write(i); // data entry
//    Serial.write(0x90);Serial.write(0x1C);Serial.write(127);
//    delay(1000);
//
// }
  

  

}

