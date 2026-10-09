// Host test adapter ONLY. Not part of CMake release target.
#include <QCoreApplication>
#include <QProcess>
#include <cstdio>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QProcess python;
    QStringList args{QString::fromLocal8Bit(qgetenv("EVENT_TEST_TRANSPORT_SCRIPT"))};
    args+=app.arguments().mid(1);
    python.start(QString::fromLocal8Bit(qgetenv("EVENT_TEST_PYTHON")),args);
    if(!python.waitForStarted(2000)||!python.waitForFinished(10000))return 7;
    auto out=python.readAllStandardOutput();std::fwrite(out.constData(),1,size_t(out.size()),stdout);
    return python.exitCode();
}
