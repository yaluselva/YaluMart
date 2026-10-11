FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    g++ cmake make libpq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt ./
COPY backend ./backend

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j2

CMD ["./build/ApiServer"]