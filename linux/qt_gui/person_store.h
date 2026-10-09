#ifndef PERSON_STORE_H
#define PERSON_STORE_H
#include <QStringList>
#include <vector>
#include <functional>

struct PersonRecord {
    QString id;
    bool enabled;
    std::vector<float> feature;
};

// GUI and recognition share no mutable database objects: only the vision thread
// uses this store. Disabled records have a suffix ignored by legacy recognition.
class PersonStore {
public:
    explicit PersonStore(const QString &directory) : directory_(directory) {}
    static bool validId(const QString &id);
    std::vector<PersonRecord> load() const;
    void save(const QString &id, const std::vector<float> &feature, bool replace,
              const std::function<bool()> &cancelled = []() { return false; });
    void setEnabled(const QString &id, bool enabled);
    void remove(const QString &id);
private:
    QString path(const QString &id, bool enabled) const;
    QString existing(const QString &id) const;
    QString directory_;
};
#endif
