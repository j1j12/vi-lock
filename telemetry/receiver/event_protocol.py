"""Strict synthetic event contract. No caller-provided device identity."""
import datetime
import json
import re
import sqlite3

KINDS = frozenset(('startup','shutdown','access_allowed','access_denied',
                   'cycle_requested','cycle_finished'))


class Conflict(Exception):
    pass


class Capacity(Exception):
    pass


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON field')
        result[key] = value
    return result


def decode(body):
    if not 0 < len(body) <= 1024:
        raise ValueError('Body size')
    obj = json.loads(body.decode('utf-8'), object_pairs_hook=unique_object)
    if not isinstance(obj,dict) or set(obj) != {'schema_version','event_id','occurred_at','kind','synthetic'}:
        raise ValueError('Exact synthetic schema required')
    if type(obj['schema_version']) is not int or obj['schema_version'] != 1 or obj['synthetic'] is not True:
        raise ValueError('Synthetic schema 1 only')
    if not isinstance(obj['event_id'],str) or not re.fullmatch(r'evt-[0-9a-f]{32}',obj['event_id']):
        raise ValueError('Invalid event ID')
    if not isinstance(obj['kind'],str) or obj['kind'] not in KINDS:
        raise ValueError('Invalid event kind')
    stamp = obj['occurred_at']
    if not isinstance(stamp,str) or not re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z',stamp):
        raise ValueError('UTC with milliseconds required')
    datetime.datetime.strptime(stamp,'%Y-%m-%dT%H:%M:%S.%fZ')
    return obj


def open_events(path):
    db = sqlite3.connect(str(path),timeout=3)
    try:
        db.execute('PRAGMA synchronous=FULL')
        db.execute('''CREATE TABLE IF NOT EXISTS synthetic_events (
            device TEXT NOT NULL, event_id TEXT NOT NULL, payload TEXT NOT NULL,
            received_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
            PRIMARY KEY(device,event_id))''')
        db.commit()
    except Exception:
        db.close()
        raise
    return db


def store_event(db, device, body, capacity=10000):
    obj = decode(body)
    if not re.fullmatch(r'[0-9a-f]{64}',device):
        raise ValueError('Device must be authenticated certificate fingerprint')
    canonical = json.dumps(obj,sort_keys=True,separators=(',',':'))
    # Serialize lookup and insertion across connections. ACK only after COMMIT.
    db.execute('BEGIN IMMEDIATE')
    try:
        old = db.execute('SELECT payload FROM synthetic_events WHERE device=? AND event_id=?',
                         (device,obj['event_id'])).fetchone()
        if old:
            if old[0] != canonical:
                raise Conflict('Same identity and ID, different payload')
            result = 'duplicate'
        else:
            if db.execute('SELECT count(*) FROM synthetic_events').fetchone()[0] >= capacity:
                raise Capacity('Capacity reached; no history evicted')
            db.execute('INSERT INTO synthetic_events(device,event_id,payload) VALUES (?,?,?)',
                       (device,obj['event_id'],canonical))
            result = 'stored'
        db.commit()
    except Exception:
        db.rollback()
        raise
    return {'event_id':obj['event_id'],'result':result}
