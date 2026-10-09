"""Ephemeral test credentials ONLY, removed with temporary directory."""
import datetime
import http.client
import ipaddress
import json
import ssl
import sqlite3
import tempfile
import threading
import unittest
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID
from receiver import open_store
from tls_receiver import TLSReceiver, context_for


def issue(directory):
    now = datetime.datetime.now(datetime.timezone.utc)
    rootkey = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'Ephemeral test CA')])
    root = (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(rootkey.public_key())
            .serial_number(x509.random_serial_number()).not_valid_before(now-datetime.timedelta(days=1))
            .not_valid_after(now+datetime.timedelta(days=1))
            .add_extension(x509.BasicConstraints(ca=True, path_length=0), True)
            .add_extension(x509.KeyUsage(False, False, False, False, False, True, True, False, False), True)
            .add_extension(x509.SubjectKeyIdentifier.from_public_key(rootkey.public_key()), False)
            .sign(rootkey, hashes.SHA256()))
    (directory/'ca.pem').write_bytes(root.public_bytes(serialization.Encoding.PEM))
    for label in ('server', 'client', 'other', 'expired'):
        key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, label)])
        builder = (x509.CertificateBuilder().subject_name(subject).issuer_name(name).public_key(key.public_key())
                   .serial_number(x509.random_serial_number()).not_valid_before(now-datetime.timedelta(days=2))
                   .not_valid_after(now+datetime.timedelta(days=-1 if label=='expired' else 1))
                   .add_extension(x509.BasicConstraints(ca=False, path_length=None), True)
                   .add_extension(x509.KeyUsage(True, False, label=='server', False, False, False, False, False, False), True)
                   .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH if label=='server' else ExtendedKeyUsageOID.CLIENT_AUTH]), False)
                   .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(rootkey.public_key()), False))
        if label=='server':
            builder=builder.add_extension(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.ip_address('127.0.0.1'))]), False)
        cert=builder.sign(rootkey, hashes.SHA256())
        (directory/(label+'.pem')).write_bytes(cert.public_bytes(serialization.Encoding.PEM))
        (directory/(label+'-key.pem')).write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))


class TLSTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory()
        cls.dir=Path(cls.temp.name)
        issue(cls.dir)
        cls.db=cls.dir/'probes.sqlite3'
        open_store(cls.db).close()
        cls.server=TLSReceiver(('127.0.0.1',0),{'127.0.0.1'},cls.db,
            context_for(cls.dir/'server.pem',cls.dir/'server-key.pem',cls.dir/'ca.pem'),cls.dir/'client.pem')
        cls.thread=threading.Thread(target=cls.server.serve_forever)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown();cls.thread.join();cls.server.server_close();cls.temp.cleanup()

    def context(self, client='client', trust=True):
        ctx=ssl.create_default_context(cafile=str(self.dir/'ca.pem') if trust else None)
        ctx.minimum_version=ssl.TLSVersion.TLSv1_2
        if client:
            ctx.load_cert_chain(str(self.dir/(client+'.pem')),str(self.dir/(client+'-key.pem')))
        return ctx

    def request(self, ctx, host='127.0.0.1', payload=None):
        con=http.client.HTTPSConnection(host,self.server.server_port,context=ctx,timeout=4)
        try:
            con.request('POST' if payload else 'GET','/probe' if payload else '/health',payload,{'Content-Type':'application/json'})
            response=con.getresponse()
            return response.status,json.loads(response.read())
        finally:
            con.close()

    def test_valid_tls_and_dedup(self):
        self.assertEqual(self.request(self.context())[0],200)
        payload=json.dumps({'event_id':'tls-test-001','type':'connectivity_probe'})
        self.assertEqual(self.request(self.context(),payload=payload)[1]['result'],'stored')
        self.assertEqual(self.request(self.context(),payload=payload)[1]['result'],'duplicate')

    def test_no_client(self):
        with self.assertRaises((ssl.SSLError,ConnectionError)):
            self.request(self.context(client=None))

    def test_untrusted_server(self):
        with self.assertRaises(ssl.SSLCertVerificationError):
            self.request(self.context(trust=False))

    def test_wrong_server_name(self):
        with self.assertRaises(ssl.SSLCertVerificationError):
            self.request(self.context(),host='localhost')

    def test_expired_client(self):
        with self.assertRaises((ssl.SSLError,ConnectionError)):
            self.request(self.context(client='expired'))

    def test_unenrolled_valid_client(self):
        self.assertEqual(self.request(self.context(client='other'))[0],403)


if __name__=='__main__':
    unittest.main()
