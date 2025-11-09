#include <stdio.h>      // For printf/perror
#include <stdlib.h>     // For exit
#include <string.h>     // For memset
#include <unistd.h>     // For close
#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For sockaddr_in
#include <arpa/inet.h>  // For inet_addr (optional, but useful)
#include <signal.h>     // For signal handling

#define PORT 8080       // Port to listen on (use >1023 to avoid needing sudo)
#define BUFFER_SIZE 1024 // Size for request buffer
#define MAX_PATH_SIZE 256 // Maximum path size

// Global variable for server socket (for signal handler)
volatile int server_fd_global = -1;
volatile int keep_running = 1;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    printf("\nReceived signal %d, shutting down gracefully...\n", signal);
    keep_running = 0;
    if (server_fd_global != -1) {
        close(server_fd_global);
    }
}

// Simple HTTP request parser - extracts method and path
int parse_http_request(const char *request, char *method, char *path) {
    // Find the first space (end of method)
    const char *space1 = strchr(request, ' ');
    if (!space1) return -1;
    
    // Extract method
    int method_len = space1 - request;
    if (method_len >= 16) return -1; // Method too long
    strncpy(method, request, method_len);
    method[method_len] = '\0';
    
    // Find the second space (end of path)
    const char *space2 = strchr(space1 + 1, ' ');
    if (!space2) return -1;
    
    // Extract path
    int path_len = space2 - (space1 + 1);
    if (path_len >= MAX_PATH_SIZE) return -1; // Path too long
    strncpy(path, space1 + 1, path_len);
    path[path_len] = '\0';
    
    return 0; // Success
}

// Simple route handler - returns response content for different paths
const char* handle_route(const char *method, const char *path) {
    // Only handle GET requests for simplicity
    if (strcmp(method, "GET") != 0) {
        return NULL; // Will trigger 405 Method Not Allowed
    }
    
    // Simple linear route matching (no recursion)
    if (strcmp(path, "/") == 0) {
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html\r\n"
               "Content-Length: 52\r\n\r\n"
               "<h1>Welcome!</h1><p>Simple C HTTP Server</p>";
    }
    else if (strcmp(path, "/about") == 0) {
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html\r\n"
               "Content-Length: 44\r\n\r\n"
               "<h1>About</h1><p>Simple C Server v1.0</p>";
    }
    else if (strcmp(path, "/health") == 0) {
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: 25\r\n\r\n"
               "{\"status\": \"healthy\"}";
    }
    
    return NULL; // 404 Not Found
}

// Generate error responses
const char* get_404_response(void) {
    return "HTTP/1.1 404 Not Found\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: 47\r\n\r\n"
           "<h1>404 Not Found</h1><p>Page not found</p>";
}

const char* get_405_response(void) {
    return "HTTP/1.1 405 Method Not Allowed\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: 63\r\n\r\n"
           "<h1>405 Method Not Allowed</h1><p>Method not supported</p>";
}

const char* get_400_response(void) {
    return "HTTP/1.1 400 Bad Request\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: 49\r\n\r\n"
           "<h1>400 Bad Request</h1><p>Invalid request</p>";
}

int main(void) {
    int server_fd, client_fd; // File descriptors for server and client sockets
    struct sockaddr_in address; // Address structure
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0}; // Buffer for reading requests
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);  // Ctrl+C
    signal(SIGTERM, signal_handler); // Termination signal
    
    // Step 1: Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully.\n");
    
    // Store server socket globally for signal handler
    server_fd_global = server_fd;
    
    // Set socket options to reuse address (prevents "Address already in use" error)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
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
    printf("Press Ctrl+C to shutdown gracefully\n");
    
    // Main server loop - continue while keep_running is true
    while (keep_running) {
        // Step 5: Accept a client connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            if (!keep_running) {
                break; // Server is shutting down
            }
            perror("Accept failed");
            continue; // Keep listening
        }
        printf("Client connected.\n");
        
        // Step 6: Read the HTTP request
        int bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read < 0) {
            perror("Failed to read request");
            close(client_fd);
            continue;
        }
        buffer[bytes_read] = '\0'; // Null-terminate
        
        printf("Received request:\n%s\n", buffer); // For debugging
        
        // Step 7: Parse request and generate response
        char method[16] = {0};
        char path[MAX_PATH_SIZE] = {0};
        const char *response;
        
        if (parse_http_request(buffer, method, path) < 0) {
            // Bad request
            response = get_400_response();
            printf("Bad request received\n");
        } else {
            printf("Method: %s, Path: %s\n", method, path);
            response = handle_route(method, path);
            
            if (response == NULL) {
                // Check if it's a method issue or path issue
                if (strcmp(method, "GET") != 0) {
                    response = get_405_response();
                    printf("Method not allowed: %s\n", method);
                } else {
                    response = get_404_response();
                    printf("Path not found: %s\n", path);
                }
            }
        }
        
        write(client_fd, response, strlen(response));
        
        // Step 8: Close the client socket
        close(client_fd);
        printf("Connection closed.\n");
    }
    
    // Cleanup
    printf("Server shutting down...\n");
    if (server_fd != -1) {
        close(server_fd);
    }
    printf("Server stopped.\n");
    return 0;
}
