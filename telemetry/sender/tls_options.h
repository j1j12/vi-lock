#pragma once
#include <QFileInfo>
#include <QFile>
#include <QStringList>
#include <stdexcept>
inline QString tlsFile(const QString &path, bool key) {
    QFileInfo info(path);
    if(path.isEmpty() || !info.isAbsolute() || info.isSymLink() || !info.isFile() || !info.isReadable())
        throw std::runtime_error("TLS files must be readable absolute regular paths, not symlinks");
#ifdef Q_OS_UNIX
    if(key && (info.permissions() & (QFile::ReadGroup|QFile::WriteGroup|QFile::ExeGroup|
                                    QFile::ReadOther|QFile::WriteOther|QFile::ExeOther)))
        throw std::runtime_error("client key must not be accessible to group/other; use chmod 600");
#else
    Q_UNUSED(key); // Windows deployment uses ACLs; POSIX bits cannot validate them.
#endif
    return info.absoluteFilePath();
}
inline QStringList tlsOptions(const QString &ca, const QString &cert, const QString &key) {
    return {"--cacert",tlsFile(ca,false),"--cert-type","PEM","--cert",tlsFile(cert,false),
            "--key-type","PEM","--key",tlsFile(key,true),"--tlsv1.2","--proto","=https"};
}
