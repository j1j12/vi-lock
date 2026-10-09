import ipaddress
import json
import tempfile
import unittest
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.x509.oid import ExtendedKeyUsageOID
from issue_credentials import issue
from tls_receiver import context_for


class IssueTest(unittest.TestCase):
    def test_bundle_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            manifest = issue(root,'192.168.101.100')
            self.assertEqual(set(p.name for p in (root/'board').iterdir()),
                             {'ca.pem','client.pem','client-key.pem'})
            self.assertEqual(set(p.name for p in (root/'server').iterdir()),
                             {'ca.pem','client.pem','server.pem','server-key.pem'})
            self.assertFalse(list(root.rglob('ca-key*')))
            ca = x509.load_pem_x509_certificate((root/'server/ca.pem').read_bytes())
            for label,folder,usage in [('server','server',ExtendedKeyUsageOID.SERVER_AUTH),
                                       ('client','board',ExtendedKeyUsageOID.CLIENT_AUTH)]:
                cert = x509.load_pem_x509_certificate((root/folder/(label+'.pem')).read_bytes())
                ca.public_key().verify(cert.signature,cert.tbs_certificate_bytes,padding.PKCS1v15(),cert.signature_hash_algorithm)
                self.assertIn(usage,cert.extensions.get_extension_for_class(x509.ExtendedKeyUsage).value)
                self.assertEqual(cert.not_valid_after_utc,ca.not_valid_after_utc)
                if label == 'server':
                    self.assertEqual(cert.extensions.get_extension_for_class(x509.SubjectAlternativeName).value.get_values_for_type(x509.IPAddress),[ipaddress.ip_address(manifest['server_ip'])])
            context_for(root/'server/server.pem',root/'server/server-key.pem',root/'server/ca.pem')
            before = {str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*') if p.is_file()}
            with self.assertRaises(ValueError):
                issue(root,'192.168.101.100')
            self.assertEqual(before,{str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*') if p.is_file()})

    def test_public_address_rejected_without_files(self):
        with tempfile.TemporaryDirectory() as name:
            with self.assertRaises(ValueError):
                issue(Path(name),'8.8.8.8')
            self.assertEqual(list(Path(name).iterdir()),[])


if __name__ == '__main__':
    unittest.main()
