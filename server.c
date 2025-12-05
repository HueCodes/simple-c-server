/*
 * Simple HTTP Server with Static File Serving
 * Author: HueCodes
 *
 * A multithreaded HTTP server supporting:
 * - Static file serving from ./public directory
 * - Dynamic route handlers
 * - Query string parsing
 * - MIME type detection
 * - Graceful shutdown
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctype.h>

/* Configuration constants */
#define DEFAULT_PORT 8080
#define BUFFER_SIZE 8192
#define MAX_PATH_SIZE 512
#define MAX_METHOD_SIZE 16
#define MAX_QUERY_PARAMS 32
#define MAX_PARAM_SIZE 256
#define LISTEN_BACKLOG 128
#define DOCUMENT_ROOT "./public"
#define DEFAULT_INDEX "index.html"

/* HTTP status codes */
#define HTTP_OK 200
#define HTTP_BAD_REQUEST 400
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_INTERNAL_ERROR 500

/* Global state */
static volatile sig_atomic_t keep_running = 1;
static int server_fd = -1;

/* Query parameter structure */
typedef struct {
    char key[MAX_PARAM_SIZE];
    char value[MAX_PARAM_SIZE];
} query_param_t;

typedef struct {
    query_param_t params[MAX_QUERY_PARAMS];
    size_t count;
} query_params_t;

/* Request context passed to handlers */
typedef struct {
    const char *method;
    const char *path;
    const query_params_t *query;
} request_t;

/* Response buffer */
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} response_t;

/* Route handler function pointer */
typedef void (*route_handler_t)(const request_t *req, response_t *res);

/* Route definition */
typedef struct {
    const char *path;
    route_handler_t handler;
} route_t;

/* Thread argument structure */
typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} client_info_t;

/* MIME type mapping */
typedef struct {
    const char *extension;
    const char *mime_type;
} mime_type_t;

static const mime_type_t mime_types[] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".txt", "text/plain"},
    {".pdf", "application/pdf"},
    {NULL, NULL}
};

/* Forward declarations */
static void handle_home(const request_t *req, response_t *res);
static void handle_about(const request_t *req, response_t *res);
static void handle_health(const request_t *req, response_t *res);

/* Route table */
static const route_t routes[] = {
    {"/", handle_home},
    {"/about", handle_about},
    {"/health", handle_health},
    {NULL, NULL}
};

/* Signal handler for graceful shutdown */
static void signal_handler(int signum) {
    (void)signum;
    keep_running = 0;
    if (server_fd != -1) {
        close(server_fd);
    }
}

/* Initialize response buffer */
static int response_init(response_t *res, size_t initial_capacity) {
    res->data = malloc(initial_capacity);
    if (!res->data) {
        return -1;
    }
    res->size = 0;
    res->capacity = initial_capacity;
    res->data[0] = '\0';
    return 0;
}

/* Append data to response buffer */
static int response_append(response_t *res, const char *data, size_t len) {
    if (res->size + len + 1 > res->capacity) {
        size_t new_capacity = res->capacity * 2;
        while (new_capacity < res->size + len + 1) {
            new_capacity *= 2;
        }
        char *new_data = realloc(res->data, new_capacity);
        if (!new_data) {
            return -1;
        }
        res->data = new_data;
        res->capacity = new_capacity;
    }
    memcpy(res->data + res->size, data, len);
    res->size += len;
    res->data[res->size] = '\0';
    return 0;
}

/* Append formatted string to response */
static int response_printf(response_t *res, const char *format, ...) {
    char buffer[BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len < 0 || (size_t)len >= sizeof(buffer)) {
        return -1;
    }
    return response_append(res, buffer, len);
}

/* Free response buffer */
static void response_free(response_t *res) {
    free(res->data);
    res->data = NULL;
    res->size = 0;
    res->capacity = 0;
}

/* Send HTTP response with status */
static void send_http_response(response_t *res, int status, const char *content_type,
                               const char *body, size_t body_len) {
    const char *status_text;
    switch (status) {
        case HTTP_OK: status_text = "OK"; break;
        case HTTP_BAD_REQUEST: status_text = "Bad Request"; break;
        case HTTP_NOT_FOUND: status_text = "Not Found"; break;
        case HTTP_METHOD_NOT_ALLOWED: status_text = "Method Not Allowed"; break;
        case HTTP_INTERNAL_ERROR: status_text = "Internal Server Error"; break;
        default: status_text = "Unknown"; break;
    }

    response_printf(res, "HTTP/1.1 %d %s\r\n", status, status_text);
    response_printf(res, "Content-Type: %s\r\n", content_type);
    response_printf(res, "Content-Length: %zu\r\n", body_len);
    response_printf(res, "Connection: close\r\n");
    response_printf(res, "\r\n");

    if (body && body_len > 0) {
        response_append(res, body, body_len);
    }
}

/* URL decode a string in-place */
static void url_decode(char *dst, const char *src, size_t dst_size) {
    size_t i = 0;
    while (*src && i < dst_size - 1) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            char *end;
            long val = strtol(hex, &end, 16);
            if (end == hex + 2) {
                dst[i++] = (char)val;
                src += 3;
                continue;
            }
        } else if (*src == '+') {
            dst[i++] = ' ';
            src++;
            continue;
        }
        dst[i++] = *src++;
    }
    dst[i] = '\0';
}

/* Parse query string into parameters */
static void parse_query_string(const char *query, query_params_t *params) {
    params->count = 0;
    if (!query || !*query) {
        return;
    }

    char buffer[BUFFER_SIZE];
    strncpy(buffer, query, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *saveptr;
    char *pair = strtok_r(buffer, "&", &saveptr);

    while (pair && params->count < MAX_QUERY_PARAMS) {
        char *equals = strchr(pair, '=');
        if (equals) {
            *equals = '\0';
            url_decode(params->params[params->count].key, pair, MAX_PARAM_SIZE);
            url_decode(params->params[params->count].value, equals + 1, MAX_PARAM_SIZE);
            params->count++;
        }
        pair = strtok_r(NULL, "&", &saveptr);
    }
}

/* Get query parameter value by key */
static const char *query_get(const query_params_t *params, const char *key) {
    for (size_t i = 0; i < params->count; i++) {
        if (strcmp(params->params[i].key, key) == 0) {
            return params->params[i].value;
        }
    }
    return NULL;
}

/* Parse HTTP request line */
static int parse_request_line(const char *request, char *method, char *path,
                              char *query_string) {
    const char *space1 = strchr(request, ' ');
    if (!space1) {
        return -1;
    }

    size_t method_len = space1 - request;
    if (method_len >= MAX_METHOD_SIZE) {
        return -1;
    }

    strncpy(method, request, method_len);
    method[method_len] = '\0';

    const char *space2 = strchr(space1 + 1, ' ');
    if (!space2) {
        return -1;
    }

    const char *uri_start = space1 + 1;
    size_t uri_len = space2 - uri_start;

    if (uri_len >= MAX_PATH_SIZE) {
        return -1;
    }

    char uri[MAX_PATH_SIZE];
    strncpy(uri, uri_start, uri_len);
    uri[uri_len] = '\0';

    char *question = strchr(uri, '?');
    if (question) {
        *question = '\0';
        strncpy(query_string, question + 1, MAX_PATH_SIZE - 1);
        query_string[MAX_PATH_SIZE - 1] = '\0';
    } else {
        query_string[0] = '\0';
    }

    strncpy(path, uri, MAX_PATH_SIZE - 1);
    path[MAX_PATH_SIZE - 1] = '\0';

    return 0;
}

/* Get MIME type from file extension */
static const char *get_mime_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) {
        return "application/octet-stream";
    }

    for (const mime_type_t *mt = mime_types; mt->extension; mt++) {
        if (strcasecmp(dot, mt->extension) == 0) {
            return mt->mime_type;
        }
    }

    return "application/octet-stream";
}

/* Normalize path and check for directory traversal */
static int is_safe_path(const char *path) {
    if (strstr(path, "..")) {
        return 0;
    }
    if (path[0] != '/') {
        return 0;
    }
    return 1;
}

/* Serve static file */
static void serve_static_file(const char *request_path, response_t *res) {
    char filepath[MAX_PATH_SIZE * 2];
    struct stat st;

    if (!is_safe_path(request_path)) {
        send_http_response(res, HTTP_BAD_REQUEST, "text/html",
                          "<h1>400 Bad Request</h1>", 24);
        return;
    }

    snprintf(filepath, sizeof(filepath), "%s%s", DOCUMENT_ROOT, request_path);

    if (stat(filepath, &st) == 0 && S_ISDIR(st.st_mode)) {
        size_t len = strlen(filepath);
        if (len > 0 && filepath[len - 1] != '/') {
            strncat(filepath, "/", sizeof(filepath) - len - 1);
        }
        strncat(filepath, DEFAULT_INDEX, sizeof(filepath) - strlen(filepath) - 1);
    }

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        send_http_response(res, HTTP_NOT_FOUND, "text/html",
                          "<h1>404 Not Found</h1>", 23);
        return;
    }

    if (fstat(fd, &st) < 0) {
        close(fd);
        send_http_response(res, HTTP_INTERNAL_ERROR, "text/html",
                          "<h1>500 Internal Server Error</h1>", 35);
        return;
    }

    char *file_content = malloc(st.st_size);
    if (!file_content) {
        close(fd);
        send_http_response(res, HTTP_INTERNAL_ERROR, "text/html",
                          "<h1>500 Internal Server Error</h1>", 35);
        return;
    }

    ssize_t bytes_read = read(fd, file_content, st.st_size);
    close(fd);

    if (bytes_read != st.st_size) {
        free(file_content);
        send_http_response(res, HTTP_INTERNAL_ERROR, "text/html",
                          "<h1>500 Internal Server Error</h1>", 35);
        return;
    }

    const char *mime_type = get_mime_type(filepath);
    send_http_response(res, HTTP_OK, mime_type, file_content, st.st_size);

    free(file_content);
}

/* Route handlers */
static void handle_home(const request_t *req, response_t *res) {
    (void)req;
    const char *body = "<h1>Welcome!</h1><p>Simple C HTTP Server</p>";
    send_http_response(res, HTTP_OK, "text/html", body, strlen(body));
}

static void handle_about(const request_t *req, response_t *res) {
    (void)req;
    const char *body = "<h1>About</h1><p>Multithreaded C Server with Static Files</p>";
    send_http_response(res, HTTP_OK, "text/html", body, strlen(body));
}

static void handle_health(const request_t *req, response_t *res) {
    (void)req;
    const char *body = "{\"status\":\"healthy\",\"threads\":\"enabled\"}";
    send_http_response(res, HTTP_OK, "application/json", body, strlen(body));
}

/* Find and execute route handler */
static int handle_dynamic_route(const request_t *req, response_t *res) {
    for (const route_t *route = routes; route->path; route++) {
        if (strcmp(req->path, route->path) == 0) {
            route->handler(req, res);
            return 1;
        }
    }
    return 0;
}

/* Main request handler */
static void handle_request(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
        return;
    }

    buffer[bytes_read] = '\0';

    char method[MAX_METHOD_SIZE];
    char path[MAX_PATH_SIZE];
    char query_string[MAX_PATH_SIZE];

    response_t res;
    if (response_init(&res, BUFFER_SIZE) < 0) {
        return;
    }

    if (parse_request_line(buffer, method, path, query_string) < 0) {
        send_http_response(&res, HTTP_BAD_REQUEST, "text/html",
                          "<h1>400 Bad Request</h1>", 24);
        write(client_fd, res.data, res.size);
        response_free(&res);
        return;
    }

    if (strcmp(method, "GET") != 0) {
        send_http_response(&res, HTTP_METHOD_NOT_ALLOWED, "text/html",
                          "<h1>405 Method Not Allowed</h1>", 32);
        write(client_fd, res.data, res.size);
        response_free(&res);
        return;
    }

    query_params_t query_params;
    parse_query_string(query_string, &query_params);

    request_t req = {
        .method = method,
        .path = path,
        .query = &query_params
    };

    if (!handle_dynamic_route(&req, &res)) {
        serve_static_file(path, &res);
    }

    write(client_fd, res.data, res.size);
    response_free(&res);
}

/* Thread function to handle client connection */
static void *client_thread(void *arg) {
    client_info_t *client = (client_info_t *)arg;

    handle_request(client->client_fd);

    close(client->client_fd);
    free(client);

    return NULL;
}

/* Start server and accept connections */
static int run_server(int port) {
    struct sockaddr_in address;
    int opt = 1;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d\n", port);
    printf("Document root: %s\n", DOCUMENT_ROOT);
    printf("Press Ctrl+C to shutdown\n");

    while (keep_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (!keep_running) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        client_info_t *client = malloc(sizeof(client_info_t));
        if (!client) {
            close(client_fd);
            continue;
        }

        client->client_fd = client_fd;
        client->client_addr = client_addr;

        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&thread, &attr, client_thread, client) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(client);
        }

        pthread_attr_destroy(&attr);
    }

    close(server_fd);
    printf("\nServer shutdown complete\n");

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            return EXIT_FAILURE;
        }
    }

    return run_server(port);
}
