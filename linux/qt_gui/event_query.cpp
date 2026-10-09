#include "event_query.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
EventQueryResult queryEvents(const QStringList &lines,const EventFilter &filter) {
    EventQueryResult result;
    if(filter.dates && (!filter.first.isValid() || !filter.last.isValid() || filter.first>filter.last)) {
        result.valid=false; return result;
    }
    for(const auto &line:lines) {
        const auto doc=QJsonDocument::fromJson(line.toUtf8());
        const auto obj=doc.object();
        if(!doc.isObject() || !obj["type"].isString() || !obj["person"].isString() ||
           !obj["time"].isString() || !obj["detail"].isString()) { ++result.invalid; continue; }
        if(!filter.person.isEmpty() && obj["person"].toString()!=filter.person) continue;
        if(!filter.type.isEmpty() && obj["type"].toString()!=filter.type) continue;
        if(filter.dates) {
            const auto time=QDateTime::fromString(obj["time"].toString(),Qt::ISODateWithMs);
            if(!time.isValid()) { ++result.invalid; continue; }
            const auto date=time.toOffsetFromUtc(8*3600).date();
            if(date<filter.first || date>filter.last) continue;
        }
        result.lines.append(line);
        ++result.counts[obj["type"].toString()];
    }
    return result;
}
