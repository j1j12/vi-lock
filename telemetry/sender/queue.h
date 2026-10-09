#pragma once
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QRegularExpression>
#include <QUuid>
#include <QSet>
#include <stdexcept>
class ProbeQueue {
public:
    static const int capacity=128;
    QJsonArray entries;
    QString path;
    explicit ProbeQueue(const QString &directory):path(QDir(directory).filePath("pending.json")) {
        if(QFileInfo(path).isSymLink()) throw std::runtime_error("queue symlink rejected");
        QFile file(path);
        if(!file.exists()) return;
        if(file.size()>65536 || !file.open(QIODevice::ReadOnly)) throw std::runtime_error("queue unreadable/oversized");
        auto doc=QJsonDocument::fromJson(file.readAll());auto obj=doc.object();
        if(!doc.isObject() || obj.size()!=2 || obj["version"].toInt()!=1 || !obj["pending"].isArray()) throw std::runtime_error("queue schema invalid; preserve file");
        entries=obj["pending"].toArray();
        if(entries.size()>capacity) throw std::runtime_error("queue over capacity");
        QSet<QString> ids;
        for(const auto &entry:entries) {
            auto e=entry.toObject();QString id=e["event_id"].toString();double failures=e["failures"].toDouble(-1);
            if(!entry.isObject() || e.size()!=3 || e["type"].toString()!="connectivity_probe" ||
                !QRegularExpression("^[A-Za-z0-9_-]{1,80}$").match(id).hasMatch() || ids.contains(id) ||
                failures<0 || failures>100000 || failures!=int(failures)) throw std::runtime_error("queue entry invalid");
            ids.insert(id);
        }
    }
    void save() {
        QSaveFile file(path);
        auto bytes=QJsonDocument(QJsonObject{{"version",1},{"pending",entries}}).toJson(QJsonDocument::Compact);
        if(!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) ||
            file.write(bytes)!=bytes.size() || !file.commit()) throw std::runtime_error("queue commit failed");
    }
    QString enqueue() {
        if(entries.size()>=capacity) throw std::runtime_error("queue full; existing records retained");
        QString id="probe-"+QUuid::createUuid().toString(QUuid::Id128);
        entries.append(QJsonObject{{"event_id",id},{"type","connectivity_probe"},{"failures",0}});save();return id;
    }
    QByteArray payload() const {
        auto obj=entries.first().toObject();obj.remove("failures");return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }
    bool acknowledge(int status,const QByteArray &body) {
        auto obj=QJsonDocument::fromJson(body).object();
        if(entries.isEmpty() || status!=200 || obj["event_id"].toString()!=entries.first().toObject()["event_id"].toString() ||
            (obj["result"].toString()!="stored" && obj["result"].toString()!="duplicate")) return false;
        entries.removeFirst();save();return true;
    }
    void failed() {
        auto obj=entries.first().toObject();obj["failures"]=qMin(100000,obj["failures"].toInt()+1);entries.replace(0,obj);save();
    }
    int delaySeconds() const {
        int n=entries.first().toObject()["failures"].toInt();return n==0?0:qMin(60,1<<qMin(n,6));
    }
};
