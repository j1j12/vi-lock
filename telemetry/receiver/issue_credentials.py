"""One-device commissioning CA. Invoke via provision_tls.ps1 on Windows.

The CA private key is intentionally never persisted. Renew by provisioning a
new bundle and replacing trust on BOTH ends, never by disabling verification.
"""
import argparse
import datetime as dt
import ipaddress
import json
import os
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID


def write_new(path, content):
    with path.open('xb') as stream:
        stream.write(content)


def issue(directory, server_ip):
    ip = ipaddress.IPv4Address(server_ip)
    if not any(ip in ipaddress.ip_network(net) for net in ('10.0.0.0/8','172.16.0.0/12','192.168.0.0/16')):
        raise ValueError('Require a private LAN IPv4 address')
    directory = Path(directory)
    if directory.is_symlink() or not directory.is_dir() or any(directory.iterdir()):
        raise ValueError('Require an empty protected directory; no overwrite')
    now = dt.datetime.now(dt.timezone.utc)
    start, end = now-dt.timedelta(minutes=5), now+dt.timedelta(days=90)
    root_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    root_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'Access probe commissioning CA')])
    root = (x509.CertificateBuilder().subject_name(root_name).issuer_name(root_name)
            .public_key(root_key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(start).not_valid_after(end)
            .add_extension(x509.BasicConstraints(ca=True,path_length=0),True)
            .add_extension(x509.KeyUsage(False,False,False,False,False,True,True,False,False),True)
            .add_extension(x509.SubjectKeyIdentifier.from_public_key(root_key.public_key()),False)
            .sign(root_key,hashes.SHA256()))
    server_dir, board_dir = directory/'server', directory/'board'
    server_dir.mkdir(mode=0o700); board_dir.mkdir(mode=0o700)
    pem = serialization.Encoding.PEM
    for dest in (server_dir,board_dir):
        write_new(dest/'ca.pem',root.public_bytes(pem))
    for label, dest, usage in (('server',server_dir,ExtendedKeyUsageOID.SERVER_AUTH),
                               ('client',board_dir,ExtendedKeyUsageOID.CLIENT_AUTH)):
        key = rsa.generate_private_key(public_exponent=65537,key_size=2048)
        name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'Access probe '+label)])
        builder = (x509.CertificateBuilder().subject_name(name).issuer_name(root_name)
                   .public_key(key.public_key()).serial_number(x509.random_serial_number())
                   .not_valid_before(start).not_valid_after(end)
                   .add_extension(x509.BasicConstraints(ca=False,path_length=None),True)
                   .add_extension(x509.KeyUsage(True,False,label=='server',False,False,False,False,False,False),True)
                   .add_extension(x509.ExtendedKeyUsage([usage]),False)
                   .add_extension(x509.SubjectKeyIdentifier.from_public_key(key.public_key()),False)
                   .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(root_key.public_key()),False))
        if label == 'server':
            builder = builder.add_extension(x509.SubjectAlternativeName([x509.IPAddress(ip)]),False)
        certificate = builder.sign(root_key,hashes.SHA256())
        write_new(dest/(label+'.pem'),certificate.public_bytes(pem))
        write_new(dest/(label+'-key.pem'),key.private_bytes(pem,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
        if label == 'client':
            write_new(server_dir/'client.pem',certificate.public_bytes(pem))
    manifest = {'server_ip':str(ip),'not_before_utc':start.isoformat(),'expires_utc':end.isoformat(),
                'ca_sha256':root.fingerprint(hashes.SHA256()).hex(),
                'ca_private_key':'not persisted; renew both ends as a new bundle',
                'mode':'synthetic-only'}
    write_new(directory/'manifest.json',(json.dumps(manifest,indent=2)+'\n').encode())
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory',required=True,type=Path)
    parser.add_argument('--server-ip',required=True)
    args = parser.parse_args()
    # This helper does not promise Windows ACL protection; the PS wrapper does.
    if os.name != 'nt':
        os.umask(0o077)
    result = issue(args.directory,args.server_ip)
    print('ISSUED synthetic-only credentials; expires UTC '+result['expires_utc'])
