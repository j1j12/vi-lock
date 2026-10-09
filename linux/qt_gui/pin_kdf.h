#ifndef PIN_KDF_H
#define PIN_KDF_H
#include <QByteArray>
#include <QMessageAuthenticationCode>

// RFC8018 section 5.2, fixed dkLen=32 with HMAC-SHA256 (one block).
// QtCore supplies the HMAC/SHA primitives; no QtNetwork/QPasswordDigestor.
// Output is byte-for-byte compatible with the v15 credential format.
inline QByteArray pinPbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations) {
    if (iterations < 1) return QByteArray();
    QByteArray input = salt;
    input.append("\0\0\0\1", 4); // INT_32_BE(1), independent of CPU endianness.
    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256, password);
    hmac.addData(input);
    QByteArray u = hmac.result();
    if (u.size() != 32) return QByteArray();
    QByteArray output = u;
    char *bytes = output.data(); // Detach from u before accumulating XOR.
    for (int round = 1; round < iterations; ++round) {
        hmac.reset(); // Retains the key; avoid constructing 600000 HMAC objects.
        hmac.addData(u);
        u = hmac.result();
        if (u.size() != 32) return QByteArray();
        for (int i = 0; i < 32; ++i)
            bytes[i] = char(static_cast<unsigned char>(bytes[i]) ^ static_cast<unsigned char>(u[i]));
    }
    u.fill('\0');
    return output;
}
#endif
