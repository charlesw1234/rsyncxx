import sys
import socket
from wsgiref.simple_server import (make_server as _make_server,
                                   WSGIRequestHandler as _WSGIRequestHandler)
from wsgiref.handlers import SimpleHandler

server_version = "WSGIServer/0.1.2"
sys_version = "Python/" + sys.version.split()[0]
software_version = server_version + ' ' + sys_version

class ServerHandler(SimpleHandler):
    http_version = '1.1'
    server_software = software_version
    def close(self):
        try:
            self.request_handler.log_request(
                self.status.split(' ',1)[0], self.bytes_sent
            )
        finally:
            SimpleHandler.close(self)

class WSGIRequestHandler(_WSGIRequestHandler):
    protocol_version = 'HTTP/1.1'
    def address_string(self):
        return self.client_address[0]

    def handle_one_request(self):
        try:
            self.raw_requestline = self.rfile.readline(65537)
            if len(self.raw_requestline) > 65536:
                self.requestline = ''
                self.request_version = ''
                self.command = ''
                self.send_error(414)
                return
            if not self.raw_requestline:
                self.close_connection = 1
                return
            if not self.parse_request():
                # An error code has been sent, just exit
                return
            handler = ServerHandler(
                self.rfile, self.wfile, self.get_stderr(), self.get_environ()
            )
            handler.request_handler = self      # backpointer for logging
            handler.run(self.server.get_app())
            self.wfile.flush() #actually send the response if not already done.
        except socket.timeout, e:
            #a read or a write timed out.  Discard this connection
            self.log_error("Request timed out: %r", e)
            self.close_connection = 1
        except socket.error, e:
            #a read or a write timed out.  Discard this connection
            self.log_error("Request error: %r", e)
            self.close_connection = 1
            return

    def handle(self):
        """Handle multiple requests if necessary."""
        self.close_connection = 1
        self.handle_one_request()
        while not self.close_connection:
            self.handle_one_request()

    def get_environ(self):
        env = _WSGIRequestHandler.get_environ(self)
        param = env.get('HTTP_CONNECTION', None)
        if param == 'close':
            self.close_connection = 1
        elif param == 'keep-alive':
            self.close_connection = 0
        return env

def make_server(host, port, app):
    return _make_server(host, port, app,
                        handler_class=WSGIRequestHandler)
