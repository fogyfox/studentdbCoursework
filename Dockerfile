FROM debian:bookworm-slim

RUN sed -i 's/deb.debian.org/mirror.yandex.ru/g' /etc/apt/sources.list.d/debian.sources
RUN apt-get update && apt-get install -y \
    g++ \
    make \
    cmake \
    libasio-dev\
    libssl-dev \
    libpqxx-dev \
    libpq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY src ./src
COPY web ./web
COPY external ./external

WORKDIR /app/src
RUN make

EXPOSE 18080

CMD ["./server"]
