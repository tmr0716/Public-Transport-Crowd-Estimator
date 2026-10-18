
FROM gcc:14

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl \
    && curl -fsSL \
    https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h \
    -o /usr/local/include/httplib.h \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN g++ -std=c++17 -O2 -pthread \
    -I/usr/local/include \
    backend/main.cpp \
    backend/crowd_estimator.cpp \
    -o /app/crowd_estimator

EXPOSE 10000

CMD ["/app/crowd_estimator"]
