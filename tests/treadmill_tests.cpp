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
  t.start(0,150); // Synthetic threshold for testing only.
  sample(t,100,70);
  assert(t.phase==Treadmill::WORKOUT && t.baseline==70);
  uint32_t now=15100+180000;
  t.tick(now);
  assert(Treadmill::walking(t.elapsed));
  sample(t,now+100,160);
  assert(t.phase==Treadmill::ALERT && t.high==160);
  uint32_t frozen=t.elapsed;
  t.tick(now+20000);assert(t.elapsed==frozen);
  t.go(now+20001,150);
  sample(t,now+20100,155);
  assert(t.phase==Treadmill::ALERT);
  t.go(now+36000,150);
  sample(t,now+36100,140);
  assert(t.phase==Treadmill::RESUME && t.elapsed==frozen);
  t.go(now+52000,150);
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

  t.start(0,0);
  t.tick(60000);assert(t.phase==Treadmill::WORKOUT && !t.baseline);
  t.tick(240000);
  for(uint32_t i=0;i<=15000;i+=1000)t.beat(i<8000?100:120,240000+i);
  assert(t.checked && t.bpm==110);
  t.tick(300000);t.tick(480000);t.tick(540000);t.tick(720000);
  assert(t.phase==Treadmill::FINAL_CHECK);
  t.tick(780000);assert(t.phase==Treadmill::DONE && t.missed);
}
