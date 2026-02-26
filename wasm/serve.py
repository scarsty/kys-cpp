from http.server import HTTPServer, SimpleHTTPRequestHandler
import sys

class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

HTTPServer(("", int(sys.argv[1]) if len(sys.argv) > 1 else 8080), Handler).serve_forever()
