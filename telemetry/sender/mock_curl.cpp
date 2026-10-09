// Host-only subprocess fixture; never ship as the board curl.
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();
    auto mode=qgetenv("PROBE_TEST_MODE");if(mode=="offline")return 7;
    int i=args.indexOf("--data-binary");if(i<0 || i+1>=args.size())return 1;
    auto id=QJsonDocument::fromJson(args[i+1].toUtf8()).object()["event_id"].toString();
    if(mode=="wrong")id="wrong-id";
    auto data=QJsonDocument(QJsonObject{{"event_id",id},{"result",mode=="duplicate"?"duplicate":"stored"}}).toJson(QJsonDocument::Compact);
    std::printf("%s\n200",data.constData());return 0;
}
