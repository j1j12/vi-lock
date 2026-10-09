"""Separate mTLS synthetic receiver; existing HTTP listener is unchanged."""
import argparse
import hashlib
import hmac
import ssl
from http.server import HTTPServer
from pathlib import Path
from receiver import Handler, open_store


def context_for(cert, key, ca):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(str(cert), str(key))
    context.load_verify_locations(cafile=str(ca))
    context.verify_mode = ssl.CERT_REQUIRED
    return context


class TLSHandler(Handler):
    def allowed(self):
        if not super().allowed():
            return False
        certificate = self.connection.getpeercert(binary_form=True)
        digest = hashlib.sha256(certificate or b'').digest()
        if not hmac.compare_digest(digest, self.server.client_digest):
            self.reply(403, {'error': 'device certificate not enrolled'})
            return False
        return True


class TLSReceiver(HTTPServer):
    allow_reuse_address = False

    def __init__(self, address, allowed, path, context, client_cert, handler=TLSHandler):
        self.allowed = set(allowed)
        self.path = path
        self.context = context
        der = ssl.PEM_cert_to_DER_cert(Path(client_cert).read_text(encoding='ascii'))
        self.client_digest = hashlib.sha256(der).digest()
        super().__init__(address, handler)

    def get_request(self):
        sock, address = super().get_request()
        sock.settimeout(4)
        if address[0] not in self.allowed:
            sock.close()
            raise ConnectionAbortedError('source denied')
        try:
            secure = self.context.wrap_socket(sock, server_side=True)
            return secure, address
        except Exception:
            sock.close()
            raise


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--bind', required=True)
    parser.add_argument('--allow', action='append', required=True)
    parser.add_argument('--cert-dir', type=Path, required=True)
    parser.add_argument('--port', type=int, default=18766)
    parser.add_argument('--data', type=Path, default=Path(__file__).parent / 'tls-data')
    args = parser.parse_args()
    context = context_for(args.cert_dir / 'server.pem', args.cert_dir / 'server-key.pem', args.cert_dir / 'ca.pem')
    args.data.mkdir(parents=True, exist_ok=True)
    path = args.data / 'probes.sqlite3'
    open_store(path).close()
    server = TLSReceiver((args.bind, args.port), args.allow, path, context, args.cert_dir / 'client.pem')
    print(f'LISTEN https://{args.bind}:{args.port} (mTLS, synthetic-only)', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
