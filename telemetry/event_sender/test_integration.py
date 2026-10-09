"""Qt CLI -> selected transport -> local mTLS. Adapter runs are NOT curl validation."""
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import threading
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'receiver'))
from test_tls_receiver import issue
from tls_receiver import TLSReceiver, context_for
from event_tls_receiver import EventHandler
from event_protocol import open_events


class SenderIntegration(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.cert=self.root/'cert';self.cert.mkdir();issue(self.cert)
        self.queue=self.root/'queue';self.db=self.root/'received.sqlite'
        open_events(self.db).close()
        self.server=TLSReceiver(('127.0.0.1',0),{'127.0.0.1'},self.db,
            context_for(self.cert/'server.pem',self.cert/'server-key.pem',self.cert/'ca.pem'),
            self.cert/'client.pem',handler=EventHandler)
        self.thread=threading.Thread(target=self.server.serve_forever);self.thread.start()
        self.url=f'https://127.0.0.1:{self.server.server_port}/events-test'
        self.exe=str(Path(__file__).with_name('event_sender_test.exe'))

    def tearDown(self):
        self.server.shutdown();self.thread.join();self.server.server_close();self.temp.cleanup()

    def run_sender(self,action,client='client',drop=False,url=None):
        args=[self.exe,'--test-directory',str(self.queue),'--'+action]
        if action=='send':
            args+=['--endpoint',url or self.url,'--cacert',str(self.cert/'ca.pem'),
                   '--cert',str(self.cert/(client+'.pem')),'--key',str(self.cert/(client+'-key.pem'))]
        env=os.environ.copy()
        if drop:env['EVENT_TEST_DROP_ACK']='1'
        else:env.pop('EVENT_TEST_DROP_ACK',None)
        return subprocess.run(args,capture_output=True,text=True,timeout=20,env=env)

    def test_send_roundtrip_and_drop_ack(self):
        self.assertEqual(self.run_sender('enqueue').returncode,0)
        self.assertEqual(self.run_sender('send',drop=True).returncode,78)
        self.assertIn('pending=1',self.run_sender('status').stdout)
        retry=self.run_sender('send');self.assertEqual(retry.returncode,0,retry.stdout+retry.stderr)
        self.assertIn('ACK committed',retry.stdout);self.assertIn('pending=0',retry.stdout)
        with sqlite3.connect(self.db) as db:
            rows=db.execute('SELECT payload FROM synthetic_events').fetchall()
        db.close()
        self.assertEqual(len(rows),1);self.assertIs(json.loads(rows[0][0])['synthetic'],True)

    def test_bad_cert_and_conflict_retain(self):
        self.assertEqual(self.run_sender('enqueue').returncode,0)
        rejected=self.run_sender('send',client='other')
        self.assertEqual(rejected.returncode,3);self.assertIn('http=403',rejected.stdout)
        self.assertEqual(self.run_sender('send',drop=True).returncode,78)
        with sqlite3.connect(self.db) as db:
            payload=json.loads(db.execute('SELECT payload FROM synthetic_events').fetchone()[0])
            payload['kind']='shutdown'
            db.execute('UPDATE synthetic_events SET payload=?',(json.dumps(payload,sort_keys=True,separators=(',',':')),))
        db.close()
        conflict=self.run_sender('send');self.assertEqual(conflict.returncode,3);self.assertIn('http=409',conflict.stdout)
        self.assertIn('pending=1',self.run_sender('status').stdout)

    def test_http_rejected_and_real_db_refused(self):
        bad=self.run_sender('send',url='http://192.168.101.100:18767/events-test')
        self.assertEqual(bad.returncode,1);self.assertFalse(self.queue.exists())
        self.queue.mkdir()
        with sqlite3.connect(self.queue/'synthetic-events.sqlite') as db:
            db.execute('CREATE TABLE real_data(person TEXT)')
        db.close()
        rejected=self.run_sender('enqueue');self.assertEqual(rejected.returncode,1)
        self.assertIn('refusing import',rejected.stderr)


if __name__=='__main__':unittest.main()
