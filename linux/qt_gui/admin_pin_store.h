#ifndef ADMIN_PIN_STORE_H
#define ADMIN_PIN_STORE_H
#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
#include <functional>

// Used only by AdminPinWorker. No PIN is stored as plaintext or logged.
class AdminPinStore {
public:
    explicit AdminPinStore(const QString &directory, std::function<qint64()> testClock = std::function<qint64()>())
        : directory_(directory), testClock_(testClock) { monotonic_.start(); }
    static bool validNewPin(const QString &pin);
    QString inspect(); // ready/setup_required; throws on missing/damaged config
    void status(int &attempts, qint64 &lockedMs);
    bool verify(const QString &pin);
    void initialize(const QString &pin, const std::function<bool()> &cancelled = []() { return false; });
    bool change(const QString &oldPin, const QString &newPin, const std::function<bool()> &cancelled = []() { return false; });
private:
    struct Record { QByteArray salt, digest; int failures = 0; };
    Record read();
    void write(const Record &record);
    void checkDirectory();
    void checkFile(const QString &path);
    bool verifyRecord(const QString &pin, Record &record);
    Record makeRecord(const QString &pin);
    QString directory_;
    qint64 now() const { return testClock_ ? testClock_() : monotonic_.elapsed(); }
    QElapsedTimer monotonic_;
    std::function<qint64()> testClock_; // Tests only; production has no clock override.
    qint64 cooldownUntil_ = -1;
};
#endif
