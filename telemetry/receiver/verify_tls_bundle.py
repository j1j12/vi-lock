"""Local-only handshake check for the issued bundle; no LAN listener/data writes."""
import argparse
import json
import socket
import ssl
import tempfile
import threading
from pathlib import Path
from receiver import open_store
from tls_receiver import TLSReceiver, context_for


def verify(bundle):
    server, board = bundle/'server', bundle/'board'
    manifest = json.loads((bundle/'manifest.json').read_text())
    context = ssl.create_default_context(cafile=str(board/'ca.pem'))
    context.load_cert_chain(str(board/'client.pem'),str(board/'client-key.pem'))
    with tempfile.TemporaryDirectory() as directory:
        db = Path(directory)/'probes.sqlite3'
        open_store(db).close()
        listener = TLSReceiver(('127.0.0.1',0),{'127.0.0.1'},db,
            context_for(server/'server.pem',server/'server-key.pem',server/'ca.pem'),server/'client.pem')
        thread = threading.Thread(target=listener.serve_forever)
        thread.start()
        try:
            with socket.create_connection(listener.server_address,timeout=4) as raw:
                with context.wrap_socket(raw,server_hostname=manifest['server_ip']) as secure:
                    secure.sendall(b'GET /health HTTP/1.0\r\nHost: localhost\r\n\r\n')
                    data = b''
                    while True:
                        chunk = secure.recv(4096)
                        if not chunk:
                            break
                        data += chunk
                        if len(data)>8192:
                            raise RuntimeError('oversized response')
                    header,body = data.split(b'\r\n\r\n',1)
                    if b' 200 ' not in header.split(b'\r\n')[0] or json.loads(body).get('status')!='ok':
                        raise RuntimeError('health check failed')
        finally:
            listener.shutdown();thread.join();listener.server_close()
    print('PASS: issued bundle mTLS handshake and IP SAN verification (loopback only)')


if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('bundle',type=Path)
    verify(parser.parse_args().bundle)
