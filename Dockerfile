FROM gcc:12 AS builder

WORKDIR /app

RUN apt-get update && apt-get install -y wget make && rm -rf /var/lib/apt/lists/*

RUN wget -q https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -O backend/httplib.h

COPY . .

RUN g++ -std=c++17 -O3 backend/main.cpp backend/crowd_estimator.cpp -o transport_server -lpthread

FROM debian:bookworm-slim

WORKDIR /app

RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/transport_server /app/transport_server
COPY --from=builder /app/index.html /app/index.html
COPY --from=builder /app/results.html /app/results.html
COPY --from=builder /app/algorithms.html /app/algorithms.html
COPY --from=builder /app/style.css /app/style.css

EXPOSE 10000

ENV PORT=10000
CMD ["/app/transport_server"]
