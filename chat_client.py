import socket
import threading

def receive_messages(client_socket):
    while True:
        try:
            message = client_socket.recv(1024).decode('utf-8')
            print("\n" + message + "\nYou: ", end="")
        except:
            print("\nDisconnected from server")
            client_socket.close()
            break

def start_client():
    host = input("Enter server IP (default: localhost): ") or 'localhost'
    port = 5555
    username = input("Enter your username: ")
    
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.connect((host, port))
    
    # Send username first
    client.send(username.encode('utf-8'))
    
    print(f"\nConnected to {host}:{port} as {username}")
    print("Type messages below (type 'exit' to quit)\nYou: ", end="")
    
    receive_thread = threading.Thread(target=receive_messages, args=(client,))
    receive_thread.start()
    
    while True:
        message = input()
        if message.lower() == 'exit':
            client.close()
            break
        client.send(message.encode('utf-8'))

if __name__ == "__main__":
    start_client()