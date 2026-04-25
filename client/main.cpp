#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <set>
#include <map>
#include <poll.h>
#include <unistd.h>
#include <QCoreApplication>
#include <QNetworkProxy>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "DependencyContainer.h"
#include "ChatService.h"
#include "SignalingService.h"
#include "CommunicationController.h"
#include "ChatModel.h"
#include "UUIDConverter.h"
#include "ChatDTO.h"
#include "ChatMemberDTO.h"
#include "MessageDTO.h"
#include "ChatDTOConverter.h"
#include "ChatMemberDTOConverter.h"
#include "MessageDTOConverter.h"
#include "MessageService.h"
#include "ChatMemberService.h"

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Non-blocking stdin helpers
// ---------------------------------------------------------------------------

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string readLineNonBlocking() {
    std::cout.flush();
    std::string line;
    struct pollfd fds[1] = {{STDIN_FILENO, POLLIN, 0}};
    while (true) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        int ret = poll(fds, 1, 50);
        if (ret > 0 && (fds[0].revents & POLLIN)) {
            if (!std::getline(std::cin, line)) return "";
            return trim(line);
        }
    }
}

bool readIntNonBlocking(int& value) {
    std::string line = readLineNonBlocking();
    if (line.empty() && std::cin.eof()) return false;
    std::istringstream iss(line);
    return bool(iss >> value);
}

bool readSizeTNonBlocking(size_t& value) {
    std::string line = readLineNonBlocking();
    if (line.empty() && std::cin.eof()) return false;
    std::istringstream iss(line);
    return bool(iss >> value);
}

// ---------------------------------------------------------------------------
// Async helper
// ---------------------------------------------------------------------------

template<typename Awaitable>
auto run_and_wait(boost::asio::io_context& ioc, Awaitable&& awaitable) {
    using ResultType = decltype(std::declval<Awaitable>().await_resume());
    struct VoidResult {};
    using StoredType = std::conditional_t<std::is_void_v<ResultType>, VoidResult, ResultType>;
    std::optional<StoredType> result;
    std::exception_ptr exception;

    boost::asio::co_spawn(ioc, [&]() -> boost::asio::awaitable<void> {
        try {
            if constexpr (std::is_void_v<ResultType>) {
                co_await std::forward<Awaitable>(awaitable);
                result = VoidResult{};
            } else {
                result = co_await std::forward<Awaitable>(awaitable);
            }
        } catch (...) {
            exception = std::current_exception();
        }
    }, boost::asio::detached);

    ioc.run_one();
    while (!result && !exception) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        ioc.poll();
        std::this_thread::sleep_for(10ms);
    }

    if (exception) {
        std::rethrow_exception(exception);
    }
    if constexpr (!std::is_void_v<ResultType>) {
        return std::move(*result);
    }
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

void printChats(const std::vector<ChatDTO>& chats) {
    if (chats.empty()) {
        std::cout << "No chats available.\n";
        return;
    }
    for (size_t i = 0; i < chats.size(); ++i) {
        const auto& c = chats[i];
        std::cout << i + 1 << ". " << c.getTitle() << " (type: " << c.getType()
                  << ", room_id: " << c.getRoomId() << ")\n";
    }
}

void printMembers(const std::vector<ChatMemberDTO>& members) {
    if (members.empty()) {
        std::cout << "No members found.\n";
        return;
    }
    for (const auto& m : members) {
        std::cout << " - " << m.getUsername() << " (id: " << m.getId() << ")\n";
    }
}

// ---------------------------------------------------------------------------
// Helpers to find local chat by roomId
// ---------------------------------------------------------------------------

// Returns local chat ID for a given roomId, or empty string if not found
std::string findLocalChatId(boost::asio::io_context& ioc,
                            std::shared_ptr<ChatService> chatService,
                            const std::string& roomId) {
    auto chats = run_and_wait(ioc, chatService->getAllChats());
    for (const auto& c : chats) {
        if (UUIDConverter::toString(c.getRoomId()) == roomId)
            return UUIDConverter::toString(c.getId());
    }
    return "";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    // Parse --db <path> CLI argument
    std::string dbPathOverride;
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--db") {
            dbPathOverride = argv[i + 1];
            break;
        }
    }

    try {
        DependencyContainer container("config.json", dbPathOverride);

        auto& ioc = container.ioContext();
        auto chatService = container.chatService();
        auto memberService = container.memberService();
        auto messageService = container.messageService();
        auto commController = container.communicationController();
        auto signaling = container.signalingService();

        std::string serverUrl = "ws://localhost:9001";
        auto profiles = run_and_wait(ioc, container.profileService()->getAllProfiles(true));
        if (!profiles.empty()) {
            serverUrl = profiles[0].getServerUrl();
        }
        std::cout << "Connecting to signaling server: " << serverUrl << "\n";

        // State tracking
        std::set<std::string> joinedRooms;
        std::string currentUsername;
        // roomId → self member UUID (for FK-safe message persistence)
        std::map<std::string, std::string> selfMemberIds;

        // Wire up feedback signals BEFORE connecting
        bool wsConnected = false;
        QObject::connect(signaling.get(), &SignalingService::connected,
            [&wsConnected] { wsConnected = true; std::cout << "Connected to server.\n"; });
        QObject::connect(signaling.get(), &SignalingService::disconnected,
            [] { std::cout << "\n[!] Disconnected from server.\n"; });
        QObject::connect(signaling.get(), &SignalingService::errorOccurred,
            [](const QString& err) { std::cerr << "[!] Connection error: " << err.toStdString() << "\n"; });

        QObject::connect(commController.get(), &CommunicationController::chatJoined,
            [](const ChatDTO& chat, const std::string& myPeerId) {
                std::cout << "\n>>> Joined room " << chat.getRoomId()
                          << " as " << myPeerId << " (" << chat.getType() << ")\n";
            });
        QObject::connect(commController.get(), &CommunicationController::errorOccurred,
            [](const std::string& err) { std::cerr << "\n[!] Error: " << err << "\n"; });

        // Real-time incoming messages + persistence
        QObject::connect(commController.get(), &CommunicationController::messageReceived,
            [&](const MessageDTO& msg) {
                std::cout << "\n>>> [" << msg.getSenderId() << "]: "
                          << msg.getContent() << "\n";
                // Persist: map roomId → local chatId, use selfMemberId for sender FK
                try {
                    std::string localChatId = findLocalChatId(ioc, chatService, msg.getChatId());
                    if (localChatId.empty()) throw std::runtime_error("chat not in local DB");

                    // Find or create a member ID for the sender
                    std::string senderMemberId;
                    auto it = selfMemberIds.find(msg.getChatId());
                    if (it != selfMemberIds.end()) {
                        senderMemberId = it->second;
                    } else {
                        throw std::runtime_error("no member record for this room");
                    }

                    MessageDTO saveable;
                    saveable.setId(boost::uuids::to_string(boost::uuids::random_generator()()));
                    saveable.setChatId(localChatId);
                    saveable.setSenderId(senderMemberId);
                    saveable.setContent(msg.getContent());
                    saveable.setDirection("incoming");
                    saveable.setDeliveryState("delivered");
                    saveable.setCreatedAt(msg.getCreatedAt().empty()
                        ? TimePointConverter::toIsoString(std::chrono::system_clock::now())
                        : msg.getCreatedAt());
                    auto model = MessageDTOConverter::fromDto(saveable);
                    run_and_wait(ioc, messageService->addMessage(model));
                } catch (const std::exception& e) {
                    std::cerr << "[warn] Could not save incoming message: " << e.what() << "\n";
                }
            });
        QObject::connect(commController.get(), &CommunicationController::participantJoined,
            [](const std::string& roomId, const ChatMemberDTO& member) {
                std::cout << "\n>>> " << member.getUsername() << " joined " << roomId << "\n";
            });
        QObject::connect(commController.get(), &CommunicationController::participantLeft,
            [](const std::string& roomId, const std::string& peerId) {
                std::cout << "\n>>> " << peerId << " left " << roomId << "\n";
            });

        signaling->connectToServer(QString::fromStdString(serverUrl));

        // Wait for WebSocket connection (up to 5 seconds)
        auto connDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!wsConnected && std::chrono::steady_clock::now() < connDeadline)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (!wsConnected)
            std::cerr << "Warning: Could not connect to server within 5s.\n";

        bool running = true;
        while (running) {
            std::cout << "\n=== Morze Console Client ===\n";
            std::cout << "1. List all chats\n";
            std::cout << "2. Join chat (create + connect)\n";
            std::cout << "3. Send message\n";
            std::cout << "4. Read chat messages\n";
            std::cout << "5. List members of chat\n";
            std::cout << "6. Exit\n";
            std::cout << "Choose option: ";

            int option = 0;
            if (!readIntNonBlocking(option)) {
                if (std::cin.eof()) { running = false; }
                continue; // empty Enter or invalid input — just re-show menu
            }

            if (option == 1) {
                // --- List all chats ---
                auto chats = run_and_wait(ioc, chatService->getAllChats());
                std::vector<ChatDTO> dtos;
                for (const auto& c : chats) dtos.push_back(ChatDTOConverter::toDto(c));
                printChats(dtos);
            }
            else if (option == 2) {
                // --- Join chat (create local + join server) ---
                std::string roomId, chatName, roomType, username;
                std::cout << "Enter roomId: ";
                roomId = readLineNonBlocking();
                std::cout << "Enter chat name: ";
                chatName = readLineNonBlocking();
                std::cout << "Enter type (direct/group) [group recommended]: ";
                roomType = readLineNonBlocking();
                if (roomType != "direct" && roomType != "group") {
                    std::cout << "Invalid type. Must be 'direct' or 'group'.\n";
                    continue;
                }
                std::cout << "Enter your username: ";
                username = readLineNonBlocking();

                // Create chat in local DB
                ChatDTO chatDTO;
                chatDTO.setId(boost::uuids::to_string(boost::uuids::random_generator()()));
                chatDTO.setRoomId(roomId);
                chatDTO.setType(roomType);
                chatDTO.setTitle(chatName);
                chatDTO.setCreatedAt(TimePointConverter::toIsoString(std::chrono::system_clock::now()));
                chatDTO.setLastMessageNumber("0");

                try {
                    ChatModel chatModel = ChatDTOConverter::fromDto(chatDTO);
                    run_and_wait(ioc, chatService->addChat(chatModel));
                } catch (const std::exception& e) {
                    std::cerr << "Could not save chat: " << e.what() << "\n";
                }

                // Create a chat_member record for self (needed for message FK)
                std::string selfMemberId = boost::uuids::to_string(boost::uuids::random_generator()());
                try {
                    ChatMemberDTO selfMember;
                    selfMember.setId(selfMemberId);
                    selfMember.setUsername(username);
                    selfMember.setLastOnlineAt(TimePointConverter::toIsoString(std::chrono::system_clock::now()));
                    auto memberModel = ChatMemberDTOConverter::fromDto(selfMember);
                    run_and_wait(ioc, memberService->addMember(memberModel));
                } catch (const std::exception& e) {
                    std::cerr << "Could not save member: " << e.what() << "\n";
                }

                selfMemberIds[roomId] = selfMemberId;
                currentUsername = username;

                // Join server room
                commController->joinChat(chatDTO, username);
                joinedRooms.insert(roomId);

                // Wait for server response
                for (int i = 0; i < 30; ++i)
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

                std::cout << "Joined.\n";
            }
            else if (option == 3) {
                // --- Send message ---
                auto chats = run_and_wait(ioc, chatService->getAllChats());
                std::vector<ChatDTO> dtos;
                for (const auto& c : chats) dtos.push_back(ChatDTOConverter::toDto(c));
                printChats(dtos);
                if (chats.empty()) continue;

                std::cout << "Select chat number: ";
                size_t idx = 0;
                if (!readSizeTNonBlocking(idx)) continue;
                if (idx < 1 || idx > chats.size()) {
                    std::cout << "Invalid selection.\n";
                    continue;
                }

                const auto& chatDTO = dtos[idx - 1];

                // Auto-join if not already in this room
                if (joinedRooms.find(chatDTO.getRoomId()) == joinedRooms.end()) {
                    if (currentUsername.empty()) {
                        std::cout << "Enter your username: ";
                        currentUsername = readLineNonBlocking();
                    }

                    // Create member if not exists
                    if (selfMemberIds.find(chatDTO.getRoomId()) == selfMemberIds.end()) {
                        std::string mid = boost::uuids::to_string(boost::uuids::random_generator()());
                        try {
                            ChatMemberDTO selfMember;
                            selfMember.setId(mid);
                            selfMember.setUsername(currentUsername);
                            selfMember.setLastOnlineAt(TimePointConverter::toIsoString(std::chrono::system_clock::now()));
                            run_and_wait(ioc, memberService->addMember(ChatMemberDTOConverter::fromDto(selfMember)));
                        } catch (...) {}
                        selfMemberIds[chatDTO.getRoomId()] = mid;
                    }

                    commController->joinChat(chatDTO, currentUsername);
                    for (int i = 0; i < 30; ++i)
                        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
                    joinedRooms.insert(chatDTO.getRoomId());
                }

                std::cout << "Enter message: ";
                std::string content = readLineNonBlocking();

                // Send via controller (uses roomId)
                MessageDTO sendDTO;
                sendDTO.setId(boost::uuids::to_string(boost::uuids::random_generator()()));
                sendDTO.setChatId(chatDTO.getRoomId());
                sendDTO.setDirection("outgoing");
                sendDTO.setContent(content);
                sendDTO.setCreatedAt(TimePointConverter::toIsoString(std::chrono::system_clock::now()));
                sendDTO.setDeliveryState("pending");
                sendDTO.setSenderId("");

                commController->sendMessage(sendDTO);

                // Persist with local chat ID + self member ID (FK-safe)
                try {
                    MessageDTO dbDTO;
                    dbDTO.setId(sendDTO.getId());
                    dbDTO.setChatId(chatDTO.getId());       // local chat UUID
                    dbDTO.setSenderId(selfMemberIds[chatDTO.getRoomId()]); // self member UUID
                    dbDTO.setContent(content);
                    dbDTO.setDirection("outgoing");
                    dbDTO.setDeliveryState("delivered");
                    dbDTO.setCreatedAt(sendDTO.getCreatedAt());
                    auto model = MessageDTOConverter::fromDto(dbDTO);
                    run_and_wait(ioc, messageService->addMessage(model));
                } catch (const std::exception& e) {
                    std::cerr << "[warn] Could not save outgoing message: " << e.what() << "\n";
                }

                std::cout << "Message sent.\n";
            }
            else if (option == 4) {
                // --- Read chat messages ---
                auto chats = run_and_wait(ioc, chatService->getAllChats());
                std::vector<ChatDTO> dtos;
                for (const auto& c : chats) dtos.push_back(ChatDTOConverter::toDto(c));
                printChats(dtos);
                if (chats.empty()) continue;

                std::cout << "Select chat number: ";
                size_t idx = 0;
                if (!readSizeTNonBlocking(idx)) continue;
                if (idx < 1 || idx > chats.size()) {
                    std::cout << "Invalid selection.\n";
                    continue;
                }

                const auto& chatDTO = dtos[idx - 1];
                auto chatModel = ChatDTOConverter::fromDto(chatDTO);
                auto messages = run_and_wait(ioc, messageService->getMessagesByChatId(chatModel.getId()));
                if (messages.empty()) {
                    std::cout << "No messages in this chat.\n";
                } else {
                    std::cout << "\n--- Messages in \"" << chatDTO.getTitle() << "\" ---\n";
                    for (const auto& m : messages) {
                        auto dto = MessageDTOConverter::toDto(m);
                        std::string arrow = (dto.getDirection() == "outgoing") ? ">> " : "<< ";
                        std::cout << arrow << dto.getContent() << "\n";
                    }
                    std::cout << "--- End ---\n";
                }
            }
            else if (option == 5) {
                // --- List members ---
                auto chats = run_and_wait(ioc, chatService->getAllChats());
                std::vector<ChatDTO> dtos;
                for (const auto& c : chats) dtos.push_back(ChatDTOConverter::toDto(c));
                printChats(dtos);
                if (chats.empty()) continue;

                std::cout << "Select chat number: ";
                size_t idx = 0;
                if (!readSizeTNonBlocking(idx)) continue;
                if (idx < 1 || idx > chats.size()) {
                    std::cout << "Invalid selection.\n";
                    continue;
                }

                const auto& chatDTO = dtos[idx - 1];
                auto chatModel = ChatDTOConverter::fromDto(chatDTO);
                auto members = run_and_wait(ioc, memberService->getMembersByChatId(chatModel.getId()));
                std::vector<ChatMemberDTO> mdtos;
                for (const auto& m : members) mdtos.push_back(ChatMemberDTOConverter::toDto(m));
                printMembers(mdtos);
            }
            else if (option == 6) {
                running = false;
            }
            else {
                std::cout << "Unknown option.\n";
            }
        }

        signaling->disconnectFromServer();
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
