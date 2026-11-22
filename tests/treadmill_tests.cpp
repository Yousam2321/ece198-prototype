#include "../PartA/PartATreadmill.h"
#include <cassert>

static void sample(Treadmill& t,uint32_t start,unsigned bpm){
  for(uint32_t i=0;i<=15000;i+=1000){
    t.tick(start+i);
    t.beat(bpm,start+i);
  }
}
int main(){
  Treadmill t;
  t.start(0);
  sample(t,100,70);
  assert(t.phase==Treadmill::WORKOUT && t.baseline==70 && t.alertLimit==140);
  uint32_t now=15100+180000;
  t.tick(now);
  assert(Treadmill::walking(t.elapsed));
  sample(t,now+100,160);
  assert(t.phase==Treadmill::ALERT && t.high==160);
  uint32_t frozen=t.elapsed;
  t.tick(now+20000);assert(t.elapsed==frozen);
  t.go(now+20001);
  sample(t,now+20100,129); // Below the alert limit, above 80% of 160.
  assert(t.phase==Treadmill::ALERT);
  t.go(now+36000);
  sample(t,now+36100,128);
  assert(t.phase==Treadmill::RESUME && t.elapsed==frozen);
  t.go(now+52000);
  uint32_t walkEnd=now+52000+(240000-frozen);
  t.tick(walkEnd);
  assert(t.elapsed==240000 && !t.missed);
  t.tick(walkEnd+180000);
  assert(Treadmill::walking(t.elapsed));
  t.beat(135,walkEnd+180000);
  t.tick(walkEnd+240000);
  assert(t.elapsed==480000 && t.missed && !t.bpm);
  t.tick(walkEnd+420000);
  assert(t.phase==Treadmill::FINAL_CHECK);
  sample(t,walkEnd+420100,125);
  assert(t.phase==Treadmill::DONE && t.bpm==125);

  t.start(0);
  t.tick(60000);assert(t.phase==Treadmill::WORKOUT && !t.baseline && !t.alertLimit);
  t.tick(240000);
  for(uint32_t i=0;i<=15000;i+=1000)t.beat(i<8000?100:120,240000+i);
  assert(t.checked && t.bpm==110);
  t.tick(300000);t.tick(480000);t.tick(540000);t.tick(720000);
  assert(t.phase==Treadmill::FINAL_CHECK);
  t.tick(780000);assert(t.phase==Treadmill::DONE && t.missed);

  // Without a resting reading, even a high walk reading has no pace goal.
  t.start(0);
  t.tick(60000);
  t.tick(240000);
  sample(t,240100,160);
  assert(t.phase==Treadmill::WORKOUT && !t.baseline && !t.alertLimit);
  assert(t.checked && t.bpm==160);

  // Bench mode supplies the readings on its own using full 15-second windows.
  t.go(0); // An active workout does not accept a second start.
  assert(t.phase==Treadmill::WORKOUT);
  t.start(0,true);
  for(uint32_t now=0;now<=210000;now+=1000){
    t.tick(now);
    t.simulatedPulse(now);
  }
  assert(t.phase==Treadmill::ALERT && t.high==160 && t.elapsed==15000+180000);
  t.go(211000);
  for(uint32_t now=211000;now<=226000;now+=1000){
    t.tick(now);
    t.simulatedPulse(now);
  }
  assert(t.phase==Treadmill::RESUME && t.bpm==120 && t.elapsed==195000);
}
