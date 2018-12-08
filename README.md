# BorgTribe
A Korg Electribe ES1 mod to enable pitch recording from a midi keyboard

<a href="https://3.bp.blogspot.com/-L-vnz9OfxlU/WqVO8Esta4I/AAAAAAAAANc/KfdmPknEjdE51xwFZMG_CuVDMzH1P7KtQCLcBGAs/s1600/Electribe-ES1-770x433.png" imageanchor="1" style="clear: left; float: left; margin-bottom: 1em; margin-right: 1em;"><img border="0" data-original-height="433" data-original-width="770" height="179" src="https://3.bp.blogspot.com/-L-vnz9OfxlU/WqVO8Esta4I/AAAAAAAAANc/KfdmPknEjdE51xwFZMG_CuVDMzH1P7KtQCLcBGAs/s320/Electribe-ES1-770x433.png" width="320" /></a>


I own an Electribe Korg for some time and I have to say that I really like this gear.  First because it is quick to learn, and second because it has a very special sound despite its relative low sampling rate comparing to current standard. I still use it in 2018, in the middle of much more elaborate gears (sometime too much elaborate !).

The Electribe ES1 is however quite limited in functionality, and on one point in particular: its inability to play a sample chromatically with an external midi keyboard.  Close to my other Volkaoss project, I quickly realized a MIDI notes to Electribe ES1 Pitch Control changes with an Arduino Uno board. Everything worked perfectly, and I was able to play a sample chromatically with an external Midi keyboard BUT....

But to my surprise the Electribe recording mode does not take into account a pitch transmitted to the MIDI IN.  Only PITCH potentiometer movements are recorded to change sample pitch in a pattern.  This is when the idea of controlling these movements with a midi keyboard connected to an Arduino came up...graft an Arduino to the ES1 like the Borg do in Startrek !

## Harware
To do this, I chose to develop on an Arduino Nano board, because of its small factor, and to use a Microchip MCP4151 digital potentiometer, with a resolution of 256 steps, which is enough to manage 127 positions on the PITCH potentiometer. 

<a href="https://2.bp.blogspot.com/-O7bXoyTK9XA/WqVa-vA7vYI/AAAAAAAAAOA/q2avoGy6UwYW0HHfWyhIHsk2GISHa2fwwCLcBGAs/s1600/borgtribe_schematic.jpg" imageanchor="1" style="clear: left; float: left; margin-bottom: 1em; margin-right: 1em;"><img border="0" data-original-height="1268" data-original-width="1518" height="534" src="https://2.bp.blogspot.com/-O7bXoyTK9XA/WqVa-vA7vYI/AAAAAAAAAOA/q2avoGy6UwYW0HHfWyhIHsk2GISHa2fwwCLcBGAs/s640/borgtribe_schematic.jpg" width="640" /></a>

The MCP4151 uses SPI, and that can be tricky on the Nano : notably the fact that MISO (pin 12) must be pulled up to MOSI (look at the 1K resistor).  That was working perfectly on the Uno proto board without that, so I suppose it is mandatory when using SPI on the Nano (whatever, it is required by SPI usually).

As you can see on the schematic also provided in the GIT repository, the analog PITCH pot wiper pad is connected to the Nano Analog2 pin and disconnected from the ES1.  So, the Nano is able to read pot values and to resend them to the MCP 4151 digital potentiometer.  That was the first step of this project : be transparent, as shown on that video : https://www.youtube.com/watch?v=-8Kga-2tmuo

## Software

I had to develop an Arduino firmware to simulate potentiometers movements when pressing a note on the MIDI keyboard. That was not so easy as the 256 steps of the MCP4151 seem a bit short to address the only 127 values but from an analog pot with an infinite resolution.  After fine tuning sessions, I finally got a very acceptable result, and I'm now able to record samples pitch on the Electribe from an external midi keyboard.

The Arduino Nano is fully embeded in the Electribe case, and works as the "man in the middle" behind the ES1 MIDI IN jack. It filters and eventually transforms every midi messages sent to the Electribe MIDI IN jack and resends such messages to the ES1 CPU .

I had to tap directly on the ES1 motherboard but hopefully that was easy as there is a lot of space between pads you can tap in. (Hires pictures can be found on the GitHub project site). 

## Using BorgTribe

At the boot time, The Nano takes the hand and asks the global parameters of the ES1 via a system exclusive message.  Then it autoconfigures everything, notably MIDI notes part affectation by resending the sysex modified to the Electribe ES1.  So the only thing to really configure is the midi channel. After power on, the Electribe has a standard behaviour ("Classic" mode), as the Nano resend everything coming  from midi IN and/or the analog Pitch potentiometer.

Notes affectations start from C2 (C2 = Part1, C2# = Part2, D2 = Part3, etc...)

The C0 midi keyboard key is the "command key".
When you hold the command key and press a function key, that will send a specific command to BorgTribe, confirmed by parts flashing n times.

#### Setting modes : C0 + D0

3 Modes are available :

1- Classic mode : the standard Electribe one

2- Midi Pitched notes : Convert notes received on MIDI IN to Pitch control changes.  In that mode, you can't record pitch in the Electribe, but useful when in playing mode, because it acts like a transpose function with the current selected part.

3- Potentiometer Pitched notes : Convert notes received on MIDI IN to potentiometers movement. In that mode you can record pitched notes in the current pattern, in the same way you do by moving manually the pitch potentiometer.

Mode alternate each time you send the command.  The parts will flash a number of time corresponding to the current mode.

#### Setting full velocity : C0 + E0

This toggle the velocity sensitivity and set the value by default to 127 (max).  This reproduce the Electribe pad behaviour.

#### Clear current pattern : C0 + F0

This command clears the current pattern, as there is no clear pattern on the Electribe ES1. It is necessary to confirm the command by sending it a second time when the parts are flashing 5 time.

#### Auto tune Key  : C0 + F0#

This command attempts an autotuning by comparing default internal note/tune tables with midi CC pitch values. It is necessary to confirm the command by sending it a second time when the parts are flashing.
**Still experimental**


#### Reset BorgTribe : C0 + B0

This command proceed a soft reset of the Arduino Nano. It is necessary to confirm the command by sending it a second time when the parts are flashing.
