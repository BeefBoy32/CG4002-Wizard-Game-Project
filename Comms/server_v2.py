import socket, json

HOST = "127.0.0.1"      # bind all
PORT = 5000

WINDOW = 60
count = 0

def main():
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind((HOST, PORT))
    server_sock.listen(1)
    print(f"Ultra96 server listening on port {PORT}...")

    conn, addr = server_sock.accept()
    print(f"Connection from {addr}")
    with conn:
        buf = b""
        while True:
            data = conn.recv(4096)
            if not data:
                print("Client closed connection.")
                break
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                if not line.strip():
                    continue
                try:
                    global count
                    count += 1
                    if count == WINDOW:
                        result = {
                        "wand_id": 1,
                        "spell_id": 1,        # keep if you still use numeric somewhere
                        "spell_type": "C",    # <-- single-letter for ESP
                        "conf": 0.97,
                        "stable": True
                        }
                        conn.sendall((json.dumps(result) + "\n").encode())
                        count = 0
                except Exception as e:
                    print("Bad window JSON:", e)

    server_sock.close()

if __name__ == "__main__":
    main()