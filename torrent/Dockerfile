# For build and usage instructions check the README.md

# Stage #1: Build
FROM alpine:latest as builder

# Tools required to build the app
RUN apk add --no-cache g++ --repository=https://dl-cdn.alpinelinux.org/alpine/edge/main 
RUN apk add --no-cache cmake make git perl linux-headers binutils 

# Build and install static openssl
WORKDIR /tmp/openssl 
RUN git clone https://github.com/openssl/openssl . 
RUN ./Configure -static --openssldir=/etc/ssl && \ 
    make -j8 && make install_sw 

# Build CTorrent
WORKDIR /app
RUN git clone https://github.com/infinage/cpp-experiments . && \ 
    cd torrent && \
    cmake -B build -DSTATIC_BUILD=ON && \ 
    cmake --build build -j8 && \
    strip build/ctorrent 

# Stage #2: Runtime
FROM alpine:latest
COPY --from=builder /app/torrent/build/ctorrent /usr/local/bin/ctorrent
ENTRYPOINT ["ctorrent"]
WORKDIR /app
CMD []
