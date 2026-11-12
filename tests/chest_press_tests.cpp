#include <cassert>
#include <cmath>
#include <iostream>
#include "../HoldStation/exercises/ChestPress.h"

void sample(ChestPressCounter& c,float p,int n=2){while(n--)c.update(p,true,true);}
int main(){
  ChestPressCounter c;
  sample(c,1);sample(c,0);assert(c.good==0 && c.phase==ChestPressCounter::READY);
  sample(c,.79f);sample(c,0);assert(c.good==0 && c.attempts==1);
  sample(c,.8f);sample(c,.2f);assert(c.good==1 && c.attempts==2);
  sample(c,.8f,20);assert(c.good==1);sample(c,0,20);assert(c.good==2 && c.attempts==3);
  // A single endpoint spike cannot qualify a rep or provide a completed return.
  sample(c,.4f);sample(c,1,1);sample(c,.5f);sample(c,0);
  assert(c.good==2 && c.attempts==4);
  sample(c,1);sample(c,0,1);assert(c.good==2);sample(c,.5f);sample(c,0);
  assert(c.good==3 && c.attempts==5);
  sample(c,.1f,50);assert(c.attempts==5); // Small adjustments are not attempts.
  sample(c,1);c.update(0,true,false);sample(c,0);assert(c.good==3);
  sample(c,1);c.update(0,false,true);sample(c,0);assert(c.good==3);
  sample(c,1);c.update(NAN,true,true);sample(c,0);assert(c.good==3);
  sample(c,-.3f);sample(c,1);sample(c,0);assert(c.good==3); // Need a fresh start first.
  sample(c,1);sample(c,-.1f);assert(c.good==4); // Return tolerance is symmetric.
  c.clear();assert(c.good==0 && c.attempts==0 && c.lastPeak==0);
  // Normalization is direction-independent: 80 -> 50 cm and 50 -> 80 cm.
  for(int direction=-1;direction<=1;direction+=2){
    c.clear();float start=50,end=start+direction*30;
    sample(c,(start-start)/(end-start));
    sample(c,((start+direction*24)-start)/(end-start));
    sample(c,(start-start)/(end-start));assert(c.good==1);
  }
  c.attempts=UINT16_MAX;c.good=UINT16_MAX;
  sample(c,1);sample(c,0);assert(c.attempts==UINT16_MAX && c.good==UINT16_MAX);
  std::cout<<"PASS: chest press thresholds, noise, dropouts, pauses, both directions and saturation\n";
}
