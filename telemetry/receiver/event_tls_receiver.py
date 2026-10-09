"""Separate synthetic event protocol receiver; never launches GUI or M4."""
import argparse
import hashlib
import sqlite3
from pathlib import Path
from event_protocol import open_events, store_event, Conflict, Capacity
from tls_receiver import TLSReceiver, TLSHandler, context_for


class EventHandler(TLSHandler):
    def do_GET(self):
        if not self.allowed():
            return
        if self.path != '/health':
            self.reply(404,{'error':'not found'})
            return
        self.reply(200,{'service':'access-event-test','status':'ok','mode':'synthetic-only','schema_version':1})

    def do_POST(self):
        if not self.allowed():
            return
        if self.path != '/events-test':
            self.reply(404,{'error':'not found'})
            return
        lengths = self.headers.get_all('Content-Length',[])
        if self.headers.get_all('Transfer-Encoding') or len(lengths)!=1 or not lengths[0].isascii() or not lengths[0].isdigit():
            self.reply(400,{'error':'single explicit Content-Length required'})
            return
        if len(lengths[0])>4 or not 0<int(lengths[0])<=1024:
            self.reply(413,{'error':'body limit'})
            return
        if self.headers.get_content_type()!='application/json':
            self.reply(415,{'error':'application/json required'})
            return
        try:
            body = self.rfile.read(int(lengths[0]))
            if len(body)!=int(lengths[0]):
                raise ValueError('incomplete body')
            # Identity is from the authenticated connection, never a JSON field.
            device = hashlib.sha256(self.connection.getpeercert(binary_form=True)).hexdigest()
            db = open_events(self.server.path)
            try:
                ack = store_event(db,device,body)
            finally:
                db.close()
            self.reply(200,ack)
        except Conflict:
            self.reply(409,{'error':'event ID conflict; original retained'})
        except (Capacity,sqlite3.Error):
            self.reply(503,{'error':'storage unavailable; retain event'})
        except (ValueError,UnicodeError,RecursionError):
            self.reply(400,{'error':'invalid synthetic event'})
        except (TimeoutError,ConnectionError):
            self.close_connection=True


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--bind',required=True)
    parser.add_argument('--allow',required=True,action='append')
    parser.add_argument('--cert-dir',required=True,type=Path)
    parser.add_argument('--port',type=int,default=18767)
    parser.add_argument('--data',type=Path,default=Path(__file__).parent/'event-test-data')
    args=parser.parse_args()
    args.data.mkdir(parents=True,exist_ok=True)
    path=args.data/'events.sqlite3'
    open_events(path).close()
    cert=args.cert_dir
    server=TLSReceiver((args.bind,args.port),args.allow,path,
        context_for(cert/'server.pem',cert/'server-key.pem',cert/'ca.pem'),cert/'client.pem',handler=EventHandler)
    print(f'LISTEN https://{args.bind}:{args.port} (synthetic events ONLY; /events-test)',flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__=='__main__':
    main()
