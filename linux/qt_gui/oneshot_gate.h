#ifndef ONESHOT_GATE_H
#define ONESHOT_GATE_H
#include <cstdint>
// GUI-thread only. A ticket is consumed by the first post-arm recognition.
class OneShotGate {
public:
    bool arm(std::int64_t now, bool ready) {
        if (!ready || ticket_) return false;
        ticket_ = ++serial_;
        claimed_ = false;
        deadline_ = now + 30000;
        return true;
    }
    void cancel() { ticket_ = 0; claimed_ = false; }
    bool active() const { return ticket_ != 0; }
    bool owns(std::uint64_t ticket) const { return ticket && ticket == ticket_ && claimed_; }
    bool expire(std::int64_t now) {
        if (active() && now >= deadline_) { cancel(); return true; }
        return false;
    }
    std::uint64_t claim(std::int64_t now) {
        expire(now);
        if (!active() || claimed_) return 0;
        claimed_ = true;
        return ticket_;
    }
    bool finish(std::uint64_t ticket, bool matched, std::int64_t now) {
        expire(now);
        if (!ticket || ticket != ticket_ || !claimed_) return false;
        cancel();
        return matched;
    }
private:
    std::uint64_t serial_ = 0, ticket_ = 0;
    std::int64_t deadline_ = 0;
    bool claimed_ = false;
};
#endif
