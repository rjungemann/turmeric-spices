#!/usr/bin/env python3
"""Minimal RFC 6455 WebSocket echo server (stdlib only).

Used by run.sh to exercise the tur-ws-client spice end to end: completes the
HTTP Upgrade handshake, then echoes every text/binary frame back unmasked.
Responds to client Close with a Close echo. Single connection, then exits.

Usage: echo_server.py <port>
"""
import base64, hashlib, os, socket, ssl, struct, sys

GUID = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def accept_key(key: bytes) -> str:
    return base64.b64encode(hashlib.sha1(key + GUID).digest()).decode()


def recv_exact(conn, n):
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def read_frame(conn):
    h = recv_exact(conn, 2)
    if h is None:
        return None
    b0, b1 = h[0], h[1]
    opcode = b0 & 0x0F
    masked = b1 & 0x80
    length = b1 & 0x7F
    if length == 126:
        length = struct.unpack("!H", recv_exact(conn, 2))[0]
    elif length == 127:
        length = struct.unpack("!Q", recv_exact(conn, 8))[0]
    mask = recv_exact(conn, 4) if masked else b"\x00\x00\x00\x00"
    payload = recv_exact(conn, length) if length else b""
    if payload is None:
        return None
    if masked:
        payload = bytes(payload[i] ^ mask[i % 4] for i in range(length))
    return opcode, payload


def send_frame(conn, opcode, payload=b""):
    hdr = bytearray([0x80 | opcode])  # FIN + opcode, server frames unmasked
    n = len(payload)
    if n < 126:
        hdr.append(n)
    elif n <= 0xFFFF:
        hdr.append(126)
        hdr += struct.pack("!H", n)
    else:
        hdr.append(127)
        hdr += struct.pack("!Q", n)
    conn.sendall(bytes(hdr) + payload)


def main():
    port = int(sys.argv[1])
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(5)

    # Optional TLS for the wss:// fixture: wrap accepted sockets with a
    # self-signed cert when WS_TLS_CERT / WS_TLS_KEY are set.
    tls_ctx = None
    cert, keyfile = os.environ.get("WS_TLS_CERT"), os.environ.get("WS_TLS_KEY")
    if cert and keyfile:
        tls_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls_ctx.load_cert_chain(cert, keyfile)

    # Accept connections until one completes the WS handshake. This tolerates
    # run.sh's TCP readiness probe, which connects and closes without
    # upgrading -- we just skip it and wait for the real client.
    conn = key = None
    while True:
        raw, _ = srv.accept()
        if tls_ctx is not None:
            try:
                conn = tls_ctx.wrap_socket(raw, server_side=True)
            except (ssl.SSLError, OSError):
                raw.close()
                continue
        else:
            conn = raw
        req = b""
        while b"\r\n\r\n" not in req:
            chunk = conn.recv(1024)
            if not chunk:
                break
            req += chunk
        key = None
        for line in req.split(b"\r\n"):
            if line.lower().startswith(b"sec-websocket-key:"):
                key = line.split(b":", 1)[1].strip()
        if key:
            break
        conn.close()

    conn.sendall(
        b"HTTP/1.1 101 Switching Protocols\r\n"
        b"Upgrade: websocket\r\n"
        b"Connection: Upgrade\r\n"
        b"Sec-WebSocket-Accept: " + accept_key(key).encode() + b"\r\n\r\n"
    )

    # Echo loop.
    while True:
        frame = read_frame(conn)
        if frame is None:
            break
        opcode, payload = frame
        if opcode == 0x8:            # close -> echo and stop
            send_frame(conn, 0x8, payload)
            break
        if opcode in (0x1, 0x2):     # text / binary -> echo same opcode
            send_frame(conn, opcode, payload)
    conn.close()
    srv.close()


if __name__ == "__main__":
    main()
