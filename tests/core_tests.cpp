#include <cassert>
#include <iostream>
#include "../WearableMonitor/PrototypeCore.h"
using namespace ece198;
int main() {
  // Countdown, running, pause and resume must never credit stopped time.
  Session s; s.reset(100); s.go(100); s.tick(3099);
  assert(s.state==COUNTDOWN && s.active==0);
  s.tick(3100); assert(s.state==RUNNING && s.active==0);
  s.tick(4100); assert(s.active==1000);
  s.go(4200); assert(s.state==PAUSED && s.active==1100);
  s.tick(12000); assert(s.active==1100);
  s.go(12100);s.tick(13100);assert(s.active==2100);
  s.finish(13200);s.tick(40000);assert(s.active==2200);
  // Same arithmetic must survive the Arduino millis() wraparound.
  s.reset(0xFFFFFF00U);s.state=RUNNING;s.tick(0x00000100U);assert(s.active==512);
  // An incomplete half-cycle cannot count, nor can a pause-spanning motion.
  CycleCounter c;
  assert(!c.update(0.0f,0));assert(!c.update(1.0f,300));
  assert(c.update(0.0f,1000));assert(!c.update(0.1f,1100));
  c.reset();assert(!c.update(1.0f,2000));assert(!c.update(0.0f,2400));
  c.reset();c.update(0.0f,3000);c.update(1.0f,3200);
  assert(!c.update(0.0f,3400)); // Too fast (<600ms).
  c.reset();c.update(0.0f,100);c.update(1.0f,400);
  assert(!c.update(0.0f,11000)); // Timed-out cycle.
  // Range credit is gated by valid consecutive samples and a settling time.
  RangeTimer r;
  for(uint32_t t=0;t<=750;t+=75)r.update(t,true,true,true);
  assert(r.total==525);
  uint32_t before=r.total;
  r.update(825,true,false,false);r.update(900,true,true,true);
  r.update(975,true,true,true);assert(r.total==before);
  r.update(1050,false,true,true);r.update(10000,true,true,true);
  assert(r.total==before); // Neither a pause nor a long data gap adds time.
  r.update(10075,true,true,false);assert(r.total==before);
  // Bouncing inputs must create one event on release, and long presses differ.
  Button b;
  assert(b.update(true,0)==NO_PRESS);assert(b.update(false,10)==NO_PRESS);
  assert(b.update(true,20)==NO_PRESS);assert(b.update(true,51)==NO_PRESS);
  assert(b.update(false,200)==NO_PRESS);assert(b.update(false,231)==SHORT_PRESS);
  assert(b.update(false,300)==NO_PRESS);
  b.update(true,400);b.update(true,431);b.update(false,1800);
  assert(b.update(false,1831)==LONG_PRESS);
  std::cout << "PASS: countdown/pause/resume, rollover, rep cycles, range dropouts, button debounce\n";
}
