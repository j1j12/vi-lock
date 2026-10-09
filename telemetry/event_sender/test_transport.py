"""Curl-shaped test adapter using real Python TLS, not curl/GnuTLS validation."""
import http.client
import ssl
import sys
from urllib.parse import urlsplit


def main():
    args=sys.argv[1:]
    def value(name):return args[args.index(name)+1]
    assert args[0]=='-q' and '-k' not in args and '--insecure' not in args
    assert value('--proto')=='=https' and value('--noproxy')=='*'
    url=urlsplit(args[-1]);assert url.scheme=='https'
    ctx=ssl.create_default_context(cafile=value('--cacert'))
    ctx.minimum_version=ssl.TLSVersion.TLSv1_2
    ctx.load_cert_chain(value('--cert'),value('--key'))
    connection=http.client.HTTPSConnection(url.hostname,url.port,context=ctx,timeout=4)
    try:
        connection.request('POST',url.path,value('--data-binary'),{'Content-Type':'application/json'})
        response=connection.getresponse();body=response.read(2049)
        if len(body)>2048:return 63
        sys.stdout.buffer.write(body+b'\n'+str(response.status).encode())
        return 0
    finally:connection.close()


if __name__=='__main__':
    try:sys.exit(main())
    except (ssl.SSLError,OSError):sys.exit(60)
