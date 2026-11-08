FROM ubuntu:22.04

# Install g++, make, and wget
RUN apt-get update && apt-get install -y g++ make wget

WORKDIR /app

# Copy project files into container
COPY . .

# Download single-header httplib.h before compilation
RUN wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -O httplib.h

# Compile main.cpp
RUN g++ -O2 main.cpp -o server -pthread

EXPOSE 8080

CMD ["./server"]
