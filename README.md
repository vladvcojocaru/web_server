# Custom HTTP Server
This project is a fully custom-built HTTP server implemented from scratch in C.

## Features:

### 1. Basic HTTP Support
- Handles HTTP/1.1 GET requests.
- Serves static files like `.html`, `.css`, `.js`
- Responds with appropriate HTTP status codes (`200 OK`, `404 Not Found`, etc.).

### 2. Multithreading
- Each client connection is processed in its own thread.

### 3. MIME Type support
- Dynamically sets the Content-Type header based on the file type, examples:
    - text/html for `.html` files
    - text/css for `.css` files
    - applications/javascript for `.js` files
### 4. Multithreading
- Response compression support (only gzip) to reduce payload sizes.
## Planned Features
- Caching for optimized performance.
- Partial content delivery for large files (video/audio streaming).
- HTTPS support using SSL/TLS for secure communication.
