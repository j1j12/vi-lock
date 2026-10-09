import http.client
import json
import sqlite3
import tempfile
import threading
import unittest
from pathlib import Path
from receiver import Receiver, open_store


class ReceiverTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / 'db.sqlite3'
        open_store(self.path).close()
        self.server = Receiver(('127.0.0.1', 0), {'127.0.0.1'}, self.path)
        self.thread = threading.Thread(target=self.server.serve_forever)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.thread.join()
        self.server.server_close()
        self.temp.cleanup()

    def request(self, method, path, body=None):
        client = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=3)
        client.request(method, path, body, {'Content-Type': 'application/json'})
        response = client.getresponse()
        result = response.status, json.loads(response.read())
        client.close()
        return result

    def test_health_and_dedup(self):
        self.assertEqual(self.request('GET', '/health')[0], 200)
        payload = json.dumps({'event_id': 'test-1', 'type': 'connectivity_probe'})
        self.assertEqual(self.request('POST', '/probe', payload)[1]['result'], 'stored')
        self.assertEqual(self.request('POST', '/probe', payload)[1]['result'], 'duplicate')
        with sqlite3.connect(self.path) as db:
            self.assertEqual(db.execute('SELECT count(*) FROM probes').fetchone()[0], 1)
        db.close()

    def test_rejections(self):
        self.assertEqual(self.request('POST', '/probe', '{')[0], 400)
        self.assertEqual(self.request('POST', '/probe', 'x' * 1025)[0], 413)
        self.assertEqual(self.request('POST', '/probe', json.dumps({'event_id': 'x', 'type': 'recognition_match'}))[0], 400)
        self.assertEqual(self.request('POST', '/probe', json.dumps({'event_id': 'x', 'type': 'connectivity_probe', 'person': 'secret'}))[0], 400)
        self.assertEqual(self.request('GET', '/unlock')[0], 404)
        self.server.allowed = set()
        self.assertEqual(self.request('GET', '/health')[0], 403)


if __name__ == '__main__':
    unittest.main()
