#include "person_store.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QRegularExpression>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
std::vector<float> normalized(std::vector<float> f) {
    require(f.size() == 512, "template must contain 512 floats");
    double norm = 0;
    for (float v : f) norm += double(v) * v;
    require(std::isfinite(norm) && norm > 1e-12, "invalid template values");
    for (float &v : f) v = float(v / std::sqrt(norm));
    return f;
}
}

bool PersonStore::validId(const QString &id) {
    return QRegularExpression("\\A[A-Za-z0-9_][A-Za-z0-9_-]{0,47}\\z").match(id).hasMatch();
}
QString PersonStore::path(const QString &id, bool enabled) const {
    require(validId(id), "ID: 1-48 ASCII letters, digits, underscore or hyphen");
    return QDir(directory_).filePath(id + (enabled ? ".bin" : ".bin.disabled"));
}
QString PersonStore::existing(const QString &id) const {
    const QString on = path(id, true), off = path(id, false);
    require(!QFileInfo(on).isSymLink() && !QFileInfo(off).isSymLink(), "symbolic links are not supported");
    require(QFileInfo::exists(on) != QFileInfo::exists(off), "record missing or duplicate enabled/disabled files");
    return QFileInfo::exists(on) ? on : off;
}
std::vector<PersonRecord> PersonStore::load() const {
    std::vector<PersonRecord> records;
    QDir dir(directory_);
    const auto files = dir.entryList(QStringList() << "*.bin" << "*.bin.disabled", QDir::Files, QDir::Name);
    require(files.size() <= 200, "person database exceeds 200 records");
    for (const QString &fileName : files) {
        bool enabled = !fileName.endsWith(".disabled");
        QString id = fileName.left(fileName.size() - (enabled ? 4 : 13));
        // Fail closed on unsupported/corrupt records instead of hiding them.
        QFile file(existing(id));
        require(file.open(QIODevice::ReadOnly) && file.size() == 2048, "invalid or unreadable template file");
        QByteArray bytes = file.readAll();
        require(bytes.size() == 2048, "short template read");
        std::vector<float> f(512);
        std::memcpy(f.data(), bytes.constData(), 2048);
        records.push_back({id, enabled, normalized(f)});
    }
    return records;
}
void PersonStore::save(const QString &id, const std::vector<float> &feature, bool replace, const std::function<bool()> &cancelled) {
    const auto f = normalized(feature);
    QString target = path(id, true);
    require(QDir().mkpath(directory_), "cannot create person directory");
    require(QFile::setPermissions(directory_, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner),
            "cannot restrict person directory permissions");
    if (replace) target = existing(id); // Updating must preserve disabled state.
    else {
        require(load().size() < 200, "person limit reached (200)");
        require(!QFileInfo::exists(target) && !QFileInfo::exists(path(id, false)) &&
                !QFileInfo(target).isSymLink() && !QFileInfo(path(id, false)).isSymLink(), "ID already exists");
    }
    QSaveFile file(target);
    require(file.open(QIODevice::WriteOnly), "cannot open template for atomic write");
    require(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner), "cannot restrict template permissions");
    require(file.write(reinterpret_cast<const char *>(f.data()), 2048) == 2048, "template write failed");
    require(!cancelled(), "enrollment cancelled before atomic commit");
    require(file.commit(), "template commit failed");
}
void PersonStore::setEnabled(const QString &id, bool enabled) {
    const QString source = existing(id), target = path(id, enabled);
    if (source != target) require(QFile::rename(source, target), "cannot change enabled state");
}
void PersonStore::remove(const QString &id) {
    require(QFile::remove(existing(id)), "cannot delete template");
}
