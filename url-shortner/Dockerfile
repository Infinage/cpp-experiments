# Stage #1
FROM alpine:latest as builder
RUN apk add --no-cache g++ cmake make
WORKDIR /app 
COPY . .
RUN cmake -B build && cmake --build build -j4

# Stage #2
FROM alpine:latest
WORKDIR /app
COPY --from=builder /app/build/app ./app
COPY --from=builder /app/static ./static
EXPOSE 8080
CMD ["./app"]
