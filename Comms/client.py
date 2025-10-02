import socket

ULTRA96_IP = "172.26.190.200"  # update if needed
PORT = 5000

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((ULTRA96_IP, PORT))
    print("Connected to Ultra96")

    # Send one test packet
    test_packet = '{"yaw":12.3,"pitch":45.6,"roll":78.9,"accelx":0.12,"accely":0.34,"accelz":0.56}'
    sock.sendall(test_packet.encode())
    print("Sent:", test_packet)

    # Wait for reply
    data = sock.recv(1024)
    print("Received from Ultra96:", data.decode())

    sock.close()

if __name__ == "__main__":
    main()
