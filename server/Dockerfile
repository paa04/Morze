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

# Копируем исходный код сервера (no-cache: forces rebuild)
COPY . .

# Собираем сервер в Release-режиме (single RUN to avoid stale cmake cache)
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    && cmake --build build --target signaling_server -j$(nproc)

# Порт по умолчанию (может быть переопределён через переменную PORT)
EXPOSE 9001

# Healthcheck для Railway
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD nc -z localhost ${PORT:-9001} || exit 1

# Запуск сервера
CMD ["./build/signaling_server"]
