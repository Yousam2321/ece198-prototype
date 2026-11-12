#pragma once
extern bool fakeBeat;
inline bool checkForBeat(long){bool value=fakeBeat;fakeBeat=false;return value;}
