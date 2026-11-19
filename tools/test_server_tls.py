"""Exercise the production WSS address/CA path against a local TLS WebSocket server."""
from pathlib import Path
import base64
import hashlib
import os
import shutil
import socketserver
import ssl
import subprocess
import sys
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[1]
OPENSSL = shutil.which('openssl')
if not OPENSSL and os.name == 'nt':
    candidate = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Git/usr/bin/openssl.exe'
    if candidate.exists(): OPENSSL = str(candidate)
if not OPENSSL: raise SystemExit('OpenSSL is required for local TLS regression tests')

def run(command, cwd):
    result = subprocess.run(list(map(str, command)), cwd=cwd, capture_output=True, text=True,
                            encoding='utf-8', errors='replace', timeout=45)
    if result.returncode:
        raise RuntimeError(f'{command[0]} failed:\n{result.stdout}\n{result.stderr}')
    return result.stdout

def compile_probe():
    build = ROOT / 'build/tests'
    build.mkdir(parents=True, exist_ok=True)
    exe = build / ('server_tls_test.exe' if os.name == 'nt' else 'server_tls_test')
    if os.name == 'nt':
        from build_windows import BIN
        compiler, flags = BIN / 'x86_64-w64-mingw32-clang.exe', ['-static', '-lws2_32']
    else:
        compiler, flags = os.environ.get('CC', 'cc'), ['-lpthread', '-lm']
    run([compiler, '-std=gnu11', '-O1', ROOT / 'tests/server_tls_test.c',
         ROOT / 'app/src/network/server_address.c', ROOT / 'app/src/external/mongoose.c',
         *flags, '-o', exe], ROOT)
    return exe

class Echo(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(5)
        try:
            stream = self.request.makefile('rb')
            stream.readline()
            headers = {}
            while True:
                line = stream.readline(8192)
                if line in (b'\r\n', b''): break
                key, value = line.decode().split(':', 1)
                headers[key.lower()] = value.strip()
            key = headers['sec-websocket-key'] + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'
            accept = base64.b64encode(hashlib.sha1(key.encode()).digest())
            self.request.sendall(b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n'
                                 b'Connection: Upgrade\r\nSec-WebSocket-Accept: ' + accept + b'\r\n\r\n')
            while True:
                head = stream.read(2)
                if not head or head[0] & 15 == 8: break
                assert head[1] == 0x81
                mask, data = stream.read(4), stream.read(1)
                if len(mask) != 4 or not data: break
                self.request.sendall(b'\x82\x01' + bytes([data[0] ^ mask[0]]))
        except (OSError, ValueError, KeyError):
            pass  # Expected when the client rejects the TLS certificate.

class Server(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = True

def main():
    exe = compile_probe()
    with tempfile.TemporaryDirectory(prefix='tls-regression-', dir=ROOT / 'build') as temp:
        folder = Path(temp)
        ca_path = folder / 'app/res/certs/ca.pem'
        ca_path.parent.mkdir(parents=True)
        for ca in ('trusted', 'untrusted'):
            run([OPENSSL, 'req', '-x509', '-newkey', 'ec', '-pkeyopt', 'ec_paramgen_curve:P-256',
                 '-nodes', '-keyout', ca + '.key', '-out', ca + '.pem', '-days', '2',
                 '-subj', '/CN=' + ca, '-addext', 'basicConstraints=critical,CA:TRUE'], folder)
        for name in ('localhost', 'wrong.example'):
            run([OPENSSL, 'req', '-new', '-newkey', 'ec', '-pkeyopt', 'ec_paramgen_curve:P-256',
                 '-nodes', '-keyout', name + '.key', '-out', name + '.csr', '-subj', '/CN=' + name], folder)
            (folder / 'leaf.ext').write_text('basicConstraints=critical,CA:FALSE\n'
                  'keyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:' + name + '\n')
            run([OPENSSL, 'x509', '-req', '-in', name + '.csr', '-CA', 'trusted.pem',
                 '-CAkey', 'trusted.key', '-CAcreateserial', '-out', name + '.pem',
                 '-days', '1', '-extfile', 'leaf.ext'], folder)
        for label, cert, trust, expected in [
            ('trusted certificate', 'localhost', 'trusted', 'accept'),
            ('untrusted certificate', 'localhost', 'untrusted', 'reject'),
            ('wrong hostname', 'wrong.example', 'trusted', 'reject'),
            ('missing trust store', 'localhost', None, 'reject'),
        ]:
            if trust: shutil.copy2(folder / (trust + '.pem'), ca_path)
            elif ca_path.exists(): ca_path.unlink()
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(folder / (cert + '.pem'), folder / (cert + '.key'))
            with Server(('127.0.0.1', 0), Echo) as server:
                server.socket = context.wrap_socket(server.socket, server_side=True)
                worker = threading.Thread(target=server.serve_forever, daemon=True)
                worker.start()
                try:
                    print(label + ': ' + run([exe, f'wss://localhost:{server.server_address[1]}', expected], folder).strip(), flush=True)
                finally:
                    server.shutdown()
                    worker.join()
    print('TLS regression tests passed', flush=True)

if __name__ == '__main__': main()
