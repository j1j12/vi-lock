import http.client
import json
import ssl
import tempfile
import threading
import unittest
from pathlib import Path
from test_tls_receiver import issue
from test_event_protocol import fixture
from event_protocol import open_events
from event_tls_receiver import EventHandler
from tls_receiver import TLSReceiver, context_for


class EventTLSTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();cls.root=Path(cls.temp.name);issue(cls.root)
        cls.path=cls.root/'events.sqlite';open_events(cls.path).close()
        cls.server=TLSReceiver(('127.0.0.1',0),{'127.0.0.1'},cls.path,
            context_for(cls.root/'server.pem',cls.root/'server-key.pem',cls.root/'ca.pem'),
            cls.root/'client.pem',handler=EventHandler)
        cls.thread=threading.Thread(target=cls.server.serve_forever);cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown();cls.thread.join();cls.server.server_close();cls.temp.cleanup()

    def request(self,body=None,path='/events-test',client='client',content_type='application/json'):
        ctx=ssl.create_default_context(cafile=str(self.root/'ca.pem'))
        if client:ctx.load_cert_chain(str(self.root/(client+'.pem')),str(self.root/(client+'-key.pem')))
        con=http.client.HTTPSConnection('127.0.0.1',self.server.server_port,context=ctx,timeout=4)
        try:
            con.request('GET' if body is None else 'POST',path,body,{'Content-Type':content_type})
            response=con.getresponse();return response.status,json.loads(response.read())
        finally:con.close()

    def test_commit_duplicate_conflict(self):
        first=self.request(fixture())
        self.assertEqual(first,(200,{'event_id':'evt-'+'1'*32,'result':'stored'}))
        self.assertEqual(self.request(fixture())[1]['result'],'duplicate')
        self.assertEqual(self.request(fixture(kind='access_denied'))[0],409)
        self.assertEqual(self.request(fixture())[1]['result'],'duplicate')

    def test_reject_fields_routes_size(self):
        self.assertEqual(self.request(None,'/health')[1]['service'],'access-event-test')
        for body in (fixture(person='never-store'),fixture(synthetic=False),b'{"event_id":"x","event_id":"y"}'):
            self.assertEqual(self.request(body)[0],400)
        self.assertEqual(self.request(fixture(),'/events')[0],404)
        self.assertEqual(self.request(b'x'*1025)[0],413)
        self.assertEqual(self.request(fixture(),content_type='text/plain')[0],415)

    def test_no_client_or_unenrolled(self):
        with self.assertRaises((ssl.SSLError,ConnectionError)):
            self.request(fixture(),client=None)
        self.assertEqual(self.request(fixture(),client='other')[0],403)

    def test_bad_http_framing(self):
        ctx=ssl.create_default_context(cafile=str(self.root/'ca.pem'))
        ctx.load_cert_chain(str(self.root/'client.pem'),str(self.root/'client-key.pem'))
        for headers in ((('Content-Length','2'),('Content-Length','2')),
                        (('Content-Length','2'),('Transfer-Encoding','chunked'))):
            con=http.client.HTTPSConnection('127.0.0.1',self.server.server_port,context=ctx,timeout=4)
            try:
                con.putrequest('POST','/events-test');con.putheader('Content-Type','application/json')
                for key,value in headers:con.putheader(key,value)
                con.endheaders(b'{}')
                response=con.getresponse();self.assertEqual(response.status,400);response.read()
            finally:con.close()


if __name__=='__main__':unittest.main()
