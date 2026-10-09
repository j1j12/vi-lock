#include "../oneshot_gate.h"
#include "../../../m4/m4_fw/CM4/App/servo_cycle.h"
#include <cassert>
#include <cstdio>
int main() {
    OneShotGate gate;
    assert(!gate.claim(0));
    assert(!gate.arm(0, false));
    assert(gate.arm(0, true));
    assert(!gate.arm(0, true));
    auto first = gate.claim(1);
    assert(first && !gate.claim(2));
    assert(!gate.finish(0, true, 2));
    assert(gate.owns(first));
    assert(!gate.finish(first, false, 3));
    assert(!gate.active());
    assert(gate.arm(4, true));
    auto second = gate.claim(5);
    assert(second != first);
    assert(!gate.finish(first, true, 6));
    assert(gate.owns(second));
    assert(gate.finish(second, true, 7));
    assert(!gate.finish(second, true, 8));
    assert(gate.arm(10, true));
    auto canceled = gate.claim(11);
    gate.cancel();
    assert(!gate.finish(canceled, true, 12));
    assert(gate.arm(100, true));
    auto expired = gate.claim(101);
    assert(!gate.finish(expired, true, 30100));
    assert(!gate.active());
    assert(gate.arm(40000, true));
    assert(!gate.claim(70000));
    volatile uint32_t remaining = 50, phase = 1;
    for (int i=1; i<=200; ++i) {
        auto action = ServoCycle_Tick(&remaining, &phase);
        assert(action == (i==50 ? 1u : i==150 ? 2u : i==200 ? 3u : 0u));
        if (i==50) assert(phase==2 && remaining==100);
        if (i==150) assert(phase==3 && remaining==50);
    }
    assert(!remaining && !phase && !ServoCycle_Tick(&remaining, &phase));
    remaining=50; phase=0;
    for (int i=1; i<=50; ++i)
        assert(ServoCycle_Tick(&remaining, &phase)==(i==50 ? 3u : 0u));
    std::puts("PASS: one-shot ticket/expiry/cancel/stale-result and 200-tick cycle/manual cutoff");
}
