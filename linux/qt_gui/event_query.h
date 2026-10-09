#ifndef EVENT_QUERY_H
#define EVENT_QUERY_H
#include <QStringList>
#include <QDate>
#include <QMap>
struct EventFilter {
    bool dates = false;
    QDate first, last;
    QString person, type; // empty = all; exact matching otherwise
};
struct EventQueryResult {
    QStringList lines;
    QMap<QString,int> counts;
    int invalid = 0;
    bool valid = true;
};
EventQueryResult queryEvents(const QStringList &lines, const EventFilter &filter);
#endif
