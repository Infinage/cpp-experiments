# Usage: `docker run -p8080:8080 --rm infinage/lqueens`

# ---- Builder Stage ----
FROM alpine:latest AS builder

# Install build dependencies
RUN apk add --no-cache g++ cmake make linux-headers git unzip asio-dev

# Download and install Crow
WORKDIR /home/deps
RUN wget -O crow.zip https://github.com/CrowCpp/Crow/archive/refs/tags/v1.2.1.2.zip && \
    unzip crow.zip && cd Crow-1.2.1.2 && \
    cmake -Bbuild -S. -DCROW_BUILD_EXAMPLES=OFF -DCROW_BUILD_TESTS=OFF && \
    cmake --build build -j4 && cmake --install build

# Download and install opencv
WORKDIR /home/deps
RUN wget -O opencv.zip https://github.com/opencv/opencv/archive/refs/tags/4.11.0.zip && \
    unzip opencv.zip && cd opencv-4.11.0 && \
    cmake -Bbuild -S. \
      -DBUILD_SHARED_LIBS=OFF \
      -DBUILD_LIST=core,imgcodecs,imgproc \
      -DBUILD_ZLIB=ON -DBUILD_JPEG=ON -DBUILD_PNG=ON \
      -DBUILD_TIFF=ON -DBUILD_OPENJPEG=ON -DBUILD_WEBP=ON \
      -DBUILD_IPP_IW=OFF -DBUILD_EXAMPLES=OFF \
      -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF && \
    cmake --build build -j4 && cmake --install build

# Copy source code
WORKDIR /home/app
COPY queens.cpp .

# Compile
RUN g++ queens.cpp -o queens -std=c++23 -Os -s -flto \
    -I/usr/local/include/opencv4 -L/usr/local/lib \
    -lopencv_imgcodecs -lopencv_imgproc -lopencv_core \
    -Wno-deprecated-enum-enum-conversion -static \
    /usr/local/lib/opencv4/3rdparty/*.a 

# ---- Final Image ----
FROM alpine:latest

# Copy executable and its needed libraries
WORKDIR /home/app
COPY --from=builder /home/app/queens .

# Expose internal port
EXPOSE 8080

# Run the app
CMD ["./queens"]
