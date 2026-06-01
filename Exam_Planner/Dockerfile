FROM ubuntu:22.04

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    libmicrohttpd-dev \
    git \
    cmake \
    && rm -rf /var/lib/apt/lists/*

# Install cJSON from source
RUN git clone https://github.com/DaveGamble/cJSON.git /tmp/cjson \
    && cd /tmp/cjson \
    && mkdir build && cd build \
    && cmake .. && make && make install \
    && ldconfig \
    && rm -rf /tmp/cjson

# Set working directory
WORKDIR /app

# Copy backend source code
COPY backend/ ./backend/

# Build the project
WORKDIR /app/backend
RUN make clean && make

# Expose port
EXPOSE 8080

# Start the server
CMD ["./build/exam_planner", "--serve", "8080"]
