from http.server import HTTPServer, ThreadingHTTPServer
from RangeHTTPServer import RangeRequestHandler
import ssl
import sys
import os
import subprocess

class Handler(RangeRequestHandler):
    pass

# def ensure_cert(cert_file="cert.pem", key_file="key.pem"):
#     if os.path.exists(cert_file) and os.path.exists(key_file):
#         return
#     print("Generating self-signed certificate...")
#     subprocess.run([
#         "openssl", "req", "-x509", "-newkey", "rsa:2048",
#         "-keyout", key_file, "-out", cert_file,
#         "-days", "365", "-nodes",
#         "-subj", "/CN=localhost"
#     ], check=True)

port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
# ensure_cert()

# ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
# ctx.load_cert_chain("cert.pem", "key.pem")

server = ThreadingHTTPServer(("", port), Handler)
# server.socket = ctx.wrap_socket(server.socket, server_side=True)

print(f"Serving HTTP on port {port}")
print(f"  http://localhost:{port}")
server.serve_forever()
