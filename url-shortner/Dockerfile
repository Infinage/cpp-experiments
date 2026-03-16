# Stage #1
FROM --platform=$TARGETPLATFORM alpine:latest as builder
RUN apk add --no-cache g++ cmake make
WORKDIR /app 
COPY . .
RUN cmake -B build -DBUILD_SHARED_LIBS=OFF && cmake --build build -j4

# Stage #2
FROM --platform=$TARGETPLATFORM alpine:latest 
WORKDIR /app
COPY --from=builder /app/build/url-shortner ./url-shortner
COPY --from=builder /app/static ./static
EXPOSE 8080
CMD ["./url-shortner"]
