#include "../App/servo_timing.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    const uint32_t clocks[] = {64000000u, 104438968u, 208877936u, 240000000u};
    const uint32_t durations[] = {500u, 1500u, 2500u, 20000u};
    for (unsigned i=0;i<4;i++) {
        uint32_t d=Servo_Divider(clocks[i]);
        assert(d && d<=65536u);
        for(unsigned j=0;j<4;j++) {
            uint32_t n=Servo_Counts(clocks[i],d,durations[j]);
            int64_t err=(int64_t)n*d*1000000-(int64_t)clocks[i]*durations[j];
            if(err<0) err=-err;
            assert(err <= (int64_t)d*500000);
        }
    }
    assert(Servo_Divider(0)==0);
    assert(Servo_Counts(64000000,0,1500)==0);
    assert(Servo_Counts(64000000,64,20000)==20000);
    puts("PASS: 16 timing cases, zero inputs and 64MHz baseline");
}
