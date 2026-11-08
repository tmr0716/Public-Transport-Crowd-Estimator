# Use a lightweight Linux image with GCC installed
FROM ubuntu:22.04

# Install build essential (g++ and make)
RUN apt-get update && apt-get install -y g++ make

# Set working directory inside the container
WORKDIR /app

# Copy all project files into the container
COPY . .

# Compile main.cpp into a binary named 'server'
# Using -pthread and -lws2_32 for platform compatibility
RUN g++ -O2 main.cpp -o server -pthread

# Expose the port your C++ server listens on
EXPOSE 8080

# Run the compiled C++ executable
CMD ["./server"]
