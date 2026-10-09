#include "admin_pin_store.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include "pin_kdf.h"
#include <QRandomGenerator>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {
const int Iterations = 600000;
const qint64 CooldownMs = 60000;
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
QByteArray derive(const QString &pin, const QByteArray &salt) {
    QByteArray secret = pin.toLatin1();
    const auto digest = pinPbkdf2Sha256(secret, salt, Iterations);
    secret.fill('\0');
    require(digest.size() == 32, "PIN key derivation failed");
    return digest;
}
bool equalDigest(const QByteArray &a, const QByteArray &b) {
    if (a.size() != 32 || b.size() != 32) return false;
    unsigned int diff = 0;
    for (int i = 0; i < 32; ++i) diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return diff == 0;
}
}

bool AdminPinStore::validNewPin(const QString &pin) {
    if (!QRegularExpression("\\A[0-9]{8,12}\\z").match(pin).hasMatch()) return false;
    bool repeated = true;
    for (const QChar c : pin) if (c != pin[0]) repeated = false;
    return !repeated && !QString("01234567890123456789").contains(pin) &&
           !QString("98765432109876543210").contains(pin);
}
void AdminPinStore::checkDirectory() {
    QFileInfo info(directory_);
    require(info.isDir() && !info.isSymLink(), "PIN directory missing or unsafe; root setup required");
#ifdef Q_OS_UNIX
    require(info.ownerId() == geteuid(), "PIN directory owner mismatch");
    require(!(info.permissions() & (QFile::WriteGroup | QFile::WriteOther)), "PIN directory is writable by others");
#endif
}
void AdminPinStore::checkFile(const QString &path) {
    QFileInfo info(path);
    require(info.isFile() && !info.isSymLink(), "PIN file missing or unsafe");
#ifdef Q_OS_UNIX
    require(info.ownerId() == geteuid(), "PIN file owner mismatch");
    require(!(info.permissions() & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)),
            "PIN file must be private (chmod 600)");
#endif
}
AdminPinStore::Record AdminPinStore::read() {
    checkDirectory();
    const QString path = QDir(directory_).filePath("admin-pin.json");
    checkFile(path);
    QFile file(path);
    require(file.open(QIODevice::ReadOnly) && file.size() > 0 && file.size() <= 4096, "PIN file cannot be read");
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    require(doc.isObject(), "PIN config damaged; root recovery required");
    const auto obj = doc.object();
    require(obj["version"].toDouble(-1) == 1 && obj["iterations"].toDouble(-1) == Iterations &&
            obj["algorithm"].toString() == "PBKDF2-HMAC-SHA256", "PIN config schema mismatch");
    const QString salt = obj["salt"].toString(), digest = obj["digest"].toString();
    const QRegularExpression hex("\\A[0-9a-f]{64}\\z");
    require(hex.match(salt).hasMatch() && hex.match(digest).hasMatch(), "PIN config invalid salt/digest");
    Record record;
    record.salt = QByteArray::fromHex(salt.toLatin1());
    record.digest = QByteArray::fromHex(digest.toLatin1());
    record.failures = obj["failures"].toInt(-1);
    require(record.failures >= 0 && record.failures <= 5 && obj["failures"].toDouble(-1) == record.failures,
            "PIN config invalid failure count");
    return record;
}
void AdminPinStore::write(const Record &record) {
    checkDirectory();
    const QString path = QDir(directory_).filePath("admin-pin.json");
    require(!QFileInfo(path).isSymLink(), "Refusing symbolic link PIN config");
    QJsonObject obj;
    obj["version"] = 1;
    obj["algorithm"] = "PBKDF2-HMAC-SHA256";
    obj["iterations"] = Iterations;
    obj["salt"] = QString::fromLatin1(record.salt.toHex());
    obj["digest"] = QString::fromLatin1(record.digest.toHex());
    obj["failures"] = record.failures;
    QSaveFile file(path);
    const auto bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    require(file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) &&
            file.write(bytes) == bytes.size() && file.commit(), "PIN state could not be saved; access denied");
}
QString AdminPinStore::inspect() {
    checkDirectory();
    const QString config = QDir(directory_).filePath("admin-pin.json");
    if (QFileInfo::exists(config) || QFileInfo(config).isSymLink()) {
        const auto record = read();
        if (record.failures >= 5 && cooldownUntil_ < 0) cooldownUntil_ = now() + CooldownMs;
        return "ready";
    }
    const QString marker = QDir(directory_).filePath("admin-pin.setup");
    checkFile(marker);
    require(QFileInfo(marker).size() == 0, "Invalid first-setup marker");
    return "setup_required";
}
void AdminPinStore::status(int &attempts, qint64 &lockedMs) {
    attempts = 5; lockedMs = 0;
    if (inspect() == "setup_required") return;
    const auto record = read();
    if (record.failures >= 5) {
        lockedMs = qMax(qint64(0), cooldownUntil_ - now());
        attempts = lockedMs ? 0 : 5;
    } else attempts = 5 - record.failures;
}
AdminPinStore::Record AdminPinStore::makeRecord(const QString &pin) {
    require(validNewPin(pin), "Use 8-12 digits; repeated or sequential PINs are not allowed");
    Record record;
    for (int i = 0; i < 8; ++i) {
        const quint32 value = QRandomGenerator::system()->generate();
        for (int j = 0; j < 4; ++j) record.salt.append(char((value >> (j * 8)) & 0xff));
    }
    record.digest = derive(pin, record.salt);
    return record;
}
void AdminPinStore::initialize(const QString &pin, const std::function<bool()> &cancelled) {
    require(inspect() == "setup_required", "PIN already initialized");
    const auto record = makeRecord(pin);
    require(!cancelled(), "PIN setup cancelled before commit");
    // Consume root's one-time permission before publishing credentials. Failure
    // thereafter is closed, never an automatic unauthenticated setup fallback.
    require(QFile::remove(QDir(directory_).filePath("admin-pin.setup")), "Cannot consume setup permission");
    write(record);
}
bool AdminPinStore::verifyRecord(const QString &pin, Record &record) {
    if (record.failures >= 5) {
        if (cooldownUntil_ < 0) cooldownUntil_ = now() + CooldownMs;
        if (now() < cooldownUntil_)
            throw std::runtime_error(QString("PIN locked; retry after %1 seconds").arg((cooldownUntil_ - now() + 999) / 1000).toStdString());
        record.failures = 0;
        cooldownUntil_ = -1;
    }
    // Count before the expensive calculation: process restart cannot discard a
    // guess in progress. A successful guess clears only after atomic-file commit.
    ++record.failures;
    write(record);
    const bool shape = QRegularExpression("\\A[0-9]{8,12}\\z").match(pin).hasMatch();
    const bool matched = shape && equalDigest(derive(pin, record.salt), record.digest);
    if (matched) { record.failures = 0; write(record); cooldownUntil_ = -1; return true; }
    if (record.failures == 5) cooldownUntil_ = now() + CooldownMs;
    return false;
}
bool AdminPinStore::verify(const QString &pin) { auto record = read(); return verifyRecord(pin, record); }
bool AdminPinStore::change(const QString &oldPin, const QString &newPin, const std::function<bool()> &cancelled) {
    require(validNewPin(newPin), "Use 8-12 non-trivial digits");
    auto record = read();
    if (!verifyRecord(oldPin, record)) return false;
    const auto replacement = makeRecord(newPin);
    require(!cancelled(), "PIN change cancelled before commit");
    write(replacement);
    return true;
}
