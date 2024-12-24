# Redis-Server Clone

A lite version clone of Redis Server implemented in C++. This project supports common Redis commands and asynchronous processing on a single thread.

## Features

- **Implemented Commands**:
  - **String Operations**:  
    `PING`, `ECHO`, `SET`, `GET`, `EXISTS`, `DEL`, `INCR`, `DECR`, `TTL`
  - **List Operations**:  
    `LPUSH`, `RPUSH`, `LRANGE`, `LLEN`
  - **Database Persistence**:  
    `SAVE`, `BGSAVE`
  - **Pattern Matching**:  
    `KEYS`

- **Data Persistence**:  
  Automatically loads from `dump.rdb` at startup (if present).

- **Performance**:  
  Runs asynchronously on a single thread. Benchmark results:
  ```bash
  redis-benchmark -t set,get -q
  SET: 71073.21 requests per second, p50=0.335 msec
  GET: 69979.01 requests per second, p50=0.359 msec
  ```

## Commands

- Build and start the server in release mode:
```bash
make
./bin/server
```
- Build and run the tests:
```bash
make test
```
