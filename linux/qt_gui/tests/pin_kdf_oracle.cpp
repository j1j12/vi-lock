// Host-only cross-check against the previous QtNetwork implementation.
#include "../pin_kdf.h"
#include <QPasswordDigestor>
#include <cstdio>
#include <QElapsedTimer>
static QByteArray previous(const QByteArray &password, const QByteArray &salt, int n) {
    QByteArray input=salt; input.append("\0\0\0\1",4);
    QByteArray u=QMessageAuthenticationCode::hash(input,password,QCryptographicHash::Sha256), out=u;
    char *p=out.data();
    for(int r=1;r<n;++r) {
        u=QMessageAuthenticationCode::hash(u,password,QCryptographicHash::Sha256);
        for(int i=0;i<32;++i) p[i]=char(static_cast<unsigned char>(p[i])^static_cast<unsigned char>(u[i]));
    }
    return out;
}
int main() {
    const QByteArray passwords[] = {QByteArray("83052947"), QByteArray("a\0b", 3), QByteArray(80, 'x')};
    const QByteArray salt("salt\0with-binary", 16);
    const int rounds[] = {1, 2, 80000, 600000};
    for (const auto &password : passwords) for (int n : rounds) {
        if (pinPbkdf2Sha256(password, salt, n) != QPasswordDigestor::deriveKeyPbkdf2(
                QCryptographicHash::Sha256, password, salt, n, 32)) {
            std::puts("FAIL: old/new KDF mismatch"); return 1;
        }
    }
    std::puts("PASS: QtNetwork oracle matches QtCore KDF (1/2/80000/600000 rounds; binary and long keys)");
    QElapsedTimer timer; timer.start();
    const auto oldDigest=previous(passwords[0],salt,600000);
    const auto oldMs=timer.elapsed(); timer.restart();
    const auto newDigest=pinPbkdf2Sha256(passwords[0],salt,600000);
    std::printf("Host benchmark 600000 rounds: previous=%lld ms, reused HMAC=%lld ms\n", (long long)oldMs,(long long)timer.elapsed());
    if(oldDigest!=newDigest) return 1;
    return 0;
}
