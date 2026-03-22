# C++ signaling server (Boost.Beast MVP)

Сигналинг-сервер для MVP из `server/tz.md` на современных готовых компонентах:

- `Boost.Asio` для асинхронной event-loop модели
- `Boost.Beast` для WebSocket сервера
- `Boost.JSON` для разбора и сериализации сообщений

## Поддерживаемый протокол (совместим с `tz.md`)

Клиентские сообщения:

- `join` (`roomId`, `username`, опционально `roomType`)
- `offer` (`roomId`, `toPeerId`, `sdp`)
- `answer` (`roomId`, `toPeerId`, `sdp`)
- `ice-candidate` (`roomId`, `toPeerId`, `candidate`)
- `leave` (`roomId`, `peerId`)

Серверные сообщения:

- `joined` (`roomId`, `roomType`, `peerId`, `participants`)
- `peer-joined`
- `peer-left`
- `offer`
- `answer`
- `ice-candidate`
- `error`

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
# или (port + worker threads)
./build/signaling_server 9001 4
# или
PORT=9001 THREADS=4 ./build/signaling_server
```

## Тесты

```bash
ctest --test-dir build --output-on-failure
```

## Что важно

- Сервер не хранит историю сообщений (только in-memory состояние комнат/peer sessions).
- `peerId` генерируется сервером при `join`.
- При выходе последнего участника комната удаляется.
- `offer`/`answer`/`ice-candidate` пересылаются только в `direct` комнатах.
- Сервер валидирует, что `roomId` и `peerId` в `leave` соответствуют активной session.
