# Stage #1
FROM alpine:latest as builder
RUN apk add --no-cache g++ make libstdc++
WORKDIR /app
COPY . .
RUN make release

# Stage #2
FROM alpine:latest
WORKDIR /app
COPY --from=builder /app/build/app ./app
COPY --from=builder /app/static ./static
EXPOSE 8080
CMD ["./app"]
