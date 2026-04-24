FROM ubuntu:24.04

# Устанавливаем системные зависимости
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    libboost-system-dev \
    libboost-json-dev \
    libsqlite3-dev \
    netcat-openbsd \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Копируем исходный код сервера
COPY . .

# Собираем сервер в Release-режиме
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --target signaling_server -j$(nproc)

# Порт по умолчанию (может быть переопределён через переменную PORT)
EXPOSE 9001

# Healthcheck для Railway
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD nc -z localhost 9001 || exit 1

# Запуск сервера
CMD ["./build/signaling_server"]
