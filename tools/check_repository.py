"""Read-only source-tree preflight; not a comprehensive secret scanner."""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
bad = []
count = 0
private_key = re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH |ENCRYPTED )?PRIVATE KEY-----")
for path in ROOT.rglob('*'):
    rel = path.relative_to(ROOT)
    if '.git' in rel.parts or '__pycache__' in rel.parts:
        continue
    if path.is_symlink():
        bad.append(f'symlink: {rel}')
        continue
    if not path.is_file():
        continue
    count += 1
    if path.suffix.lower() in {'.pem','.key','.p12','.pfx','.crt','.sqlite','.db','.jsonl',
                              '.exe','.elf','.ko','.o','.a','.bin','.dtb','.zip','.log'}:
        bad.append(f'forbidden artifact: {rel}')
    if path.stat().st_size > 10 * 1024 * 1024:
        bad.append(f'oversized file: {rel}')
    content = path.read_bytes()
    if private_key.search(content):
        bad.append(f'private key marker: {rel}')
    if content.startswith(b'\x7fELF') or content.startswith(b'MZ'):
        bad.append(f'executable binary: {rel}')
    # Check curated docs only; vendor docs may refer to upstream files.
    if path.suffix == '.md' and (len(rel.parts) == 1 or rel.parts[0] == 'docs'):
        for target in re.findall(r'\]\(([^)]+)\)', content.decode('utf-8-sig')):
            if '://' in target or target.startswith('#'):
                continue
            if not (path.parent / target.split('#')[0]).exists():
                bad.append(f'broken doc link: {rel} -> {target}')
if bad:
    print('\n'.join(bad))
    sys.exit(1)
print(f'PASS: {count} files; basic artifact/key-marker/curated-link checks. Manual review still required.')
