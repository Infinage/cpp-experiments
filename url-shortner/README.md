# URL Shortener

This is a lightweight, in-memory URL shortener built entirely in C++ with no external dependencies. It uses sockets in async mode for network connectivity and a simple UI built with barebone HTML, CSS, and JavaScript.

### TODO:
1. Configure Github actions to push to docker hub: infinage/url-shortner:latest
2. Configure AWS ECS / Fargate

---

## Build and Test Locally

To build and test the application locally:

```bash
make && ./build/main
```

---

## Test and Deploy with Docker

To build and run the application using Docker:

```bash
docker build -t url-container .
docker run -p 80:80 url-container
```

---

## Features

- **Protocol Handling**: URLs without a protocol will automatically have "http" appended.
- **In-Memory**: All data is stored in-memory, so any changes will be lost on application restart.

---

## API Endpoints

1. **GET `/ping`**  
   Returns the count of URLs currently stored.

2. **GET `/`**  
   Renders the home HTML page.

3. **GET `/<short_url>`**  
   Redirects to the long URL if it exists, or returns a 404 error.

4. **POST `/`**  
   - Request body: `{"url": "<long_url>"}`
   - Shortens the provided URL and returns the shortened URL.
   - If the URL already exists, it returns a `200` status. Otherwise, a `201` status is returned.

5. **DELETE `/<short_url>`**  
   Deletes the corresponding shortened URL if it exists, or returns a 404 error.

---

## URL Shortening Mechanism

- **Counter**: A `std::size_t` counter is incremented with each successful POST.
- **Key Generation**: A pseudo-random key is generated and XOR'd using a stream cipher to produce a uint8 vector, which is then base62 encoded.
- **Size Consistency**: All returned URLs are of the same length.
- **Potential Limits**: It is theoretically possible to run out of space for URL shortening.
