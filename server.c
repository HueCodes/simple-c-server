#include <stdio.h>      // For printf/perror
#include <stdlib.h>     // For exit
#include <string.h>     // For memset
#include <unistd.h>     // For close
#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For sockaddr_in
#include <arpa/inet.h>  // For inet_addr (optional, but useful)

#define PORT 8080       // Port to listen on (use >1023 to avoid needing sudo)
#define BUFFER_SIZE 1024 // Size for request buffer

int main() {
    int server_fd, client_fd; // File descriptors for server and client sockets
    struct sockaddr_in address; // Address structure
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0}; // Buffer for reading requests
    
    // Step 1: Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully.\n");
    
    // Step 2: Set up address (localhost:8080)
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces (0.0.0.0)
    address.sin_port = htons(PORT); // Convert port to network byte order
    
    // Step 3: Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Socket bound to port %d.\n", PORT);
    
    // Step 4: Listen for connections (backlog of 3)
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Listening on port %d...\n", PORT);
    
    // Infinite loop to accept connections
    while (1) {
        // Step 5: Accept a client connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue; // Keep listening
        }
        printf("Client connected.\n");
        
        // Step 6: Read the HTTP request (simple, no full parsing)
        read(client_fd, buffer, BUFFER_SIZE);
        printf("Received request:\n%s\n", buffer); // For debugging
        
        // Step 7: Send a simple HTTP response
        char *response = 
            "HTTP/1.1 200 OK\n"
            "Content-Type: text/html\n"
            "Content-Length: 13\n\n"
            "Hello, World!";
        write(client_fd, response, strlen(response));
        
        // Step 8: Close the client socket
        close(client_fd);
        printf("Connection closed.\n");
    }
    
    // Cleanup (unreachable in this loop, but good practice)
    close(server_fd);
    return 0;
}