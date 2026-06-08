from http.server import BaseHTTPRequestHandler, HTTPServer

VERSION = "2.0.4"

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):

        if self.path == "/version":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(VERSION.encode())

        elif self.path == "/firmware.bin":
            with open("firmware.bin", "rb") as f:
                data = f.read()

            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        else:
            self.send_response(404)
            self.end_headers()

HTTPServer(("0.0.0.0", 80), Handler).serve_forever()