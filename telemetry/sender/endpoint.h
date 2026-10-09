#pragma once
#include <QString>
#include <QUrl>
#include <QRegularExpression>
#include <stdexcept>
// Synthetic LAN diagnostics only. No redirects, credentials, DNS or real data.
inline QString probeEndpoint(const QString &text) {
    QUrl url(text,QUrl::StrictMode);
    QString host=url.host();auto parts=host.split('.');
    bool ipv4=parts.size()==4;
    for(const auto &part:parts) {
        bool ok=false;int value=part.toInt(&ok);
        if(!ok || value<0 || value>255 || QString::number(value)!=part) ipv4=false;
    }
    bool local=ipv4 && (parts[0]=="10" || (parts[0]=="192" && parts[1]=="168") ||
        (parts[0]=="172" && parts[1].toInt()>=16 && parts[1].toInt()<=31));
    if(!url.isValid() || url.scheme()!="https" || !local || url.port()!=18766 ||
        url.path()!="/probe" || url.hasQuery() || url.hasFragment() || !url.userInfo().isEmpty() || text.contains('@'))
        throw std::runtime_error("--endpoint must be https://<private IPv4>:18766/probe (mTLS synthetic LAN only)");
    return url.toString(QUrl::FullyEncoded);
}
