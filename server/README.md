# C++ signaling server (MVP skeleton)

Скелет сигналинг-сервера для MVP из `server/tz.md`.

## Что реализовано

- WebSocket-сервер (без внешних библиотек).
- In-memory состояние комнат и peer sessions.
- Поддержка сообщений:
  - `join` (`room`, `id`)
  - `signal` (`to`, `data`)
- Серверные события:
  - `peers`
  - `peer-joined`
  - `peer-left`
  - `signal`
  - `error`
- Удаление комнаты после выхода последнего участника.

## Сборка

```bash
cd server
cmake -S . -B build
cmake --build build -j
```

## Запуск

```bash
./build/signaling_server
# или
PORT=9001 ./build/signaling_server
# или
./build/signaling_server 9001
```

## Ограничения скелета

- Нет аутентификации и персистентного хранилища.
- Нет TLS.
- JSON-разбор минимальный и ориентирован на MVP-схему сообщений.
- Однопоточная event-loop модель.
