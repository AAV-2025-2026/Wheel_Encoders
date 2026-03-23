import socket

PORT = 10111

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

sock.bind(("0.0.0.0", PORT))

print(f"UDP receiver listening on port {PORT}...")

try:
    while True:
        # Receive data and sender address
        data, addr = sock.recvfrom(1024)
        if len(data) >= 4:
            value = int.from_bytes(data[:4], byteorder="big", signed=True)
            print(f"Received from {addr}: {value}")
        else:
            print(f"Received invalid size {len(data)} from {addr}")
except KeyboardInterrupt:
    print("\nShutting down...")
finally:
    sock.close()
