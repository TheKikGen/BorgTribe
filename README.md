# BorgTribe
A Korg Electribe ES1 mod to enable pitch recording


To describe the project (work in progress) rapidly).

It is not possible to record pitch changes sent to ES1 via MIDI control changes : the ES1 doesn't care about these CC when in recording mode.  But, the pitch potentiometer motion are recorded... so....

I'm using a digital potentiomer to generate the right pitch change motion (mimic the analog pot) with a "midified" Arduino board, so I can use a midi keyboard to record the pich changes mapped to note on events.

More later !
