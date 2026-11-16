"""Exercise the real Windows/macOS HTTP transport against an isolated local server."""
import http.server
from pathlib import Path
import subprocess
import sys
import threading


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *args):
        pass

    def do_GET(self):
        bodies = {'/exact': b'abcd', '/over': b'abcde', '/partial': b'abc',
                  '/empty': b'', '/denied': b'denied'}
        code = 204 if self.path == '/empty' else 403 if self.path == '/denied' else 200
        self.send_response(code)
        self.send_header('Connection', 'close')
        if self.path == '/chunked':
            self.send_header('Transfer-Encoding', 'chunked')
            body = b'4\r\nabcd\r\n1\r\ne\r\n0\r\n\r\n'
        else:
            body = bodies[self.path]
            self.send_header('Content-Length', '20' if self.path == '/partial' else str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass
        self.close_connection = True


if __name__ == '__main__':
    executable = str(Path(sys.argv[1]).resolve())
    with http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            subprocess.run([executable, f'http://127.0.0.1:{server.server_port}'], check=True, timeout=30)
        finally:
            server.shutdown()
            thread.join()
