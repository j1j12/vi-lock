#ifndef ENROLLMENT_SESSION_H
#define ENROLLMENT_SESSION_H
#include <atomic>
#include <cstdint>
// A closed/replaced enrollment can never become current again.
class EnrollmentSession {
public:
    std::uint64_t begin() {
        if(serial_.load()==UINT32_MAX) { active_.store(0); return 0; }
        const auto id=++serial_; active_.store(id); return id;
    }
    bool owns(std::uint64_t id) const { return id && active_.load()==id; }
    void cancel(std::uint64_t id) {
        if(id>UINT32_MAX) return;
        auto expected=static_cast<std::uint32_t>(id); active_.compare_exchange_strong(expected,0);
    }
private:
    std::atomic<std::uint32_t> serial_{0},active_{0};
};
#endif
