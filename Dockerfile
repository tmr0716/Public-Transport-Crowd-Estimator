FROM gcc:14

WORKDIR /app

COPY backend/ ./backend/

RUN g++ -std=c++17 -O2 \
    backend/main.cpp \
    backend/crowd_estimator.cpp \
    -o crowd_estimator

CMD ["./crowd_estimator"]
