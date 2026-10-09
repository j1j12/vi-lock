"""LAN diagnostic receiver. Synthetic probes only; never accepts real access events."""
import argparse
import json
import re
import sqlite3
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

MAX_BODY = 1024


def open_store(path):
    db = sqlite3.connect(str(path), timeout=3)
    db.execute('PRAGMA synchronous=FULL')
    db.execute('''CREATE TABLE IF NOT EXISTS probes (
        event_id TEXT PRIMARY KEY, payload TEXT NOT NULL,
        received_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')))''')
    db.commit()
    return db


def store_probe(db, obj):
    if not isinstance(obj, dict) or set(obj) != {'event_id', 'type'}:
        raise ValueError('Only event_id and type are accepted')
    if obj['type'] != 'connectivity_probe':
        raise ValueError('Real events disabled in diagnostic phase')
    if not isinstance(obj['event_id'], str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,80}', obj['event_id']):
        raise ValueError('Invalid event_id')
    payload = json.dumps(obj, sort_keys=True, separators=(',', ':'))
    with db:
        old = db.execute('SELECT payload FROM probes WHERE event_id=?', (obj['event_id'],)).fetchone()
        if old:
            return 'duplicate'
        if db.execute('SELECT count(*) FROM probes').fetchone()[0] >= 10000:
            raise ValueError('Diagnostic capacity reached')
        db.execute('INSERT INTO probes(event_id,payload) VALUES (?,?)', (obj['event_id'], payload))
    return 'stored'  # commit completed before acknowledging


class Receiver(HTTPServer):
    allow_reuse_address = False

    def __init__(self, address, allowed, path):
        self.allowed = allowed
        self.path = path
        super().__init__(address, Handler)

    def get_request(self):
        sock, address = super().get_request()
        sock.settimeout(4)
        return sock, address


class Handler(BaseHTTPRequestHandler):
    server_version = 'AccessProbe/1'
    sys_version = ''

    def log_message(self, fmt, *args):
        pass  # no request contents or identifiers in logs

    def reply(self, status, obj):
        data = json.dumps(obj, separators=(',', ':')).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Connection', 'close')
        self.end_headers()
        self.wfile.write(data)
        self.close_connection = True

    def allowed(self):
        if self.client_address[0] not in self.server.allowed:
            self.reply(403, {'error': 'source denied'})
            return False
        return True

    def do_GET(self):
        if not self.allowed():
            return
        if self.path != '/health':
            self.reply(404, {'error': 'not found'})
            return
        self.reply(200, {'service': 'access-probe', 'status': 'ok', 'mode': 'synthetic-only'})

    def do_POST(self):
        if not self.allowed():
            return
        if self.path != '/probe':
            self.reply(404, {'error': 'not found'})
            return
        lengths = self.headers.get_all('Content-Length', [])
        if self.headers.get('Transfer-Encoding') or len(lengths) != 1 or not lengths[0].isdigit():
            self.reply(400, {'error': 'explicit single Content-Length required'})
            return
        if len(lengths[0]) > 4:
            self.reply(413, {'error': 'body limit'})
            return
        size = int(lengths[0])
        if not 0 < size <= MAX_BODY:
            self.reply(413, {'error': 'body limit'})
            return
        if self.headers.get_content_type() != 'application/json':
            self.reply(415, {'error': 'application/json required'})
            return
        try:
            body = self.rfile.read(size)
            if len(body) != size:
                raise ValueError('incomplete body')
            obj = json.loads(body)
            db = open_store(self.server.path)
            try:
                result = store_probe(db, obj)
            finally:
                db.close()
            self.reply(200, {'event_id': obj['event_id'], 'result': result})
        except (ValueError, UnicodeError):
            self.reply(400, {'error': 'invalid synthetic probe'})
        except sqlite3.Error:
            self.reply(503, {'error': 'storage unavailable; retry same event_id'})
        except TimeoutError:
            self.close_connection = True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--bind', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=18765)
    parser.add_argument('--allow', action='append', default=[])
    parser.add_argument('--data', type=Path, default=Path(__file__).parent / 'data')
    args = parser.parse_args()
    args.data.mkdir(parents=True, exist_ok=True)
    path = args.data / 'probes.sqlite3'
    open_store(path).close()
    server = Receiver((args.bind, args.port), set(args.allow) | {'127.0.0.1', args.bind}, path)
    print(f'LISTEN http://{args.bind}:{args.port} (synthetic-only; no real events)', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
