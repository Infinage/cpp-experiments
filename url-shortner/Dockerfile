# Use a lightweight base image
FROM alpine:latest

# Install necessary packages for building and running C++ applications
RUN apk add --no-cache g++ make libstdc++ gcc

# Set the working directory
WORKDIR /app

# Copy the application source code to the container
COPY . .

# Build the application
RUN make release

# Allow external applications to access 80 
EXPOSE 8080

# Set the default command to run the application
CMD ["./build/app"]
