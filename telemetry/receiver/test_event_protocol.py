import json
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from event_protocol import decode, open_events, store_event, Conflict, Capacity


def fixture(**changes):
    obj={'schema_version':1,'event_id':'evt-'+'1'*32,'occurred_at':'2026-10-05T00:00:00.000Z',
         'kind':'access_allowed','synthetic':True}
    obj.update(changes)
    return json.dumps(obj).encode()


class ProtocolTest(unittest.TestCase):
    def test_validation(self):
        decode(fixture())
        for body in (fixture(person='private'),fixture(synthetic=False),fixture(schema_version=True),
                     fixture(kind='unlock'),fixture(occurred_at='2026-02-30T00:00:00.000Z'),
                     fixture(event_id='probe-1'),b'{"schema_version":1,"schema_version":1}',b'[]',
                     fixture(device_id='spoofed'),fixture(kind=[])):
            with self.assertRaises(ValueError): decode(body)
        obj=json.loads(fixture());obj.pop('synthetic')
        with self.assertRaises(ValueError):decode(json.dumps(obj).encode())

    def test_commit_retry_conflict_capacity_identity(self):
        with tempfile.TemporaryDirectory() as name:
            path=Path(name)/'db.sqlite';device='a'*64
            db=open_events(path)
            self.assertEqual(store_event(db,device,fixture())['result'],'stored');db.close()
            db=open_events(path) # same request after lost ACK / receiver reopen
            self.assertEqual(store_event(db,device,fixture(),capacity=1)['result'],'duplicate')
            with self.assertRaises(Conflict):store_event(db,device,fixture(kind='access_denied'))
            with self.assertRaises(Capacity):store_event(db,device,fixture(event_id='evt-'+'2'*32),capacity=1)
            self.assertEqual(db.execute('SELECT count(*) FROM synthetic_events').fetchone()[0],1)
            self.assertEqual(store_event(db,'b'*64,fixture())['result'],'stored')
            self.assertEqual(db.execute('SELECT count(*) FROM synthetic_events').fetchone()[0],2)
            db.close()

    def test_concurrent_connections(self):
        with tempfile.TemporaryDirectory() as name:
            path=Path(name)/'db.sqlite';open_events(path).close()
            def send(_):
                db=open_events(path)
                try:return store_event(db,'a'*64,fixture())['result']
                finally:db.close()
            with ThreadPoolExecutor(max_workers=4) as pool:
                results=list(pool.map(send,range(8)))
            self.assertEqual(results.count('stored'),1)
            self.assertEqual(results.count('duplicate'),7)


if __name__=='__main__':unittest.main()
