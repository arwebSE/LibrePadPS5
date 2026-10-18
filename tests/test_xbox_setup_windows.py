"""Windows-only helper integration test; Microsoft download, disposable local HTTP server.

Run from the repository root. No console is contacted. Requires Python and
Windows PowerShell; the optional repository helper itself needs no Python.
"""
import hashlib, http.server, json, pathlib, subprocess, threading

uploads = []
class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args): pass
    def reply(self, value):
        body = json.dumps(value).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def do_GET(self):
        assert self.path == '/api/xbox'
        self.reply({'enabled':True, 'firmware':'bundled'})
    def do_POST(self):
        assert self.path == '/api/xbox/firmware'
        assert self.headers['X-LibrePad-Control'] == '1'
        assert self.headers['Content-Type'] == 'application/octet-stream'
        body = self.rfile.read(int(self.headers['Content-Length']))
        assert len(body) == 70620
        assert hashlib.sha256(body).hexdigest() == '48084d9fa53b9bb04358f3bb127b7495dc8f7bb0b3ca1437bd24ef2b6eabdf66'
        uploads.append(body)
        self.reply({'message':'Verified firmware installed on disposable HTTP mock'})
server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
command = ['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File','tools/xbox/setup.ps1','-PS5Address',f'http://127.0.0.1:{server.server_port}']
result = subprocess.run(command, capture_output=True, text=True, timeout=90)
print(result.stdout)
print(result.stderr)
assert result.returncode == 0 and len(uploads) == 1
bad = pathlib.Path('build/invalid-test-firmware.bin')
bad.write_bytes(bytes(70620))
try:
    result = subprocess.run(command + ['-FirmwarePath',str(bad)],capture_output=True,text=True,timeout=20)
    assert result.returncode != 0 and 'hash does not match' in result.stderr and len(uploads) == 1
finally:
    bad.unlink()
    server.shutdown()
print('Windows Microsoft download, native CAB extraction, SHA-256 verification, binary installation and wrong-image rejection passed')
