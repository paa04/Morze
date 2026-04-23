#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
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

using namespace std::chrono_literals;

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

int main() {
    try {
        DependencyContainer container("config.json");

        auto& ioc = container.ioContext();
        auto chatService = container.chatService();
        auto memberService = container.memberService();
        auto messageService = container.messageService();
        auto commController = container.communicationController();
        auto signaling = container.signalingService();

        std::thread ioThread([&ioc]() { ioc.run(); });

        std::string serverUrl = "ws://localhost:9001";
        auto profiles = run_and_wait(ioc, container.profileService()->getAllProfiles(true));
        if (!profiles.empty()) {
            serverUrl = profiles[0].getServerUrl();
        }
        std::cout << "Connecting to signaling server: " << serverUrl << "\n";
        signaling->connectToServer(QString::fromStdString(serverUrl));

        bool running = true;
        while (running) {
            std::cout << "\n=== Morze Console Client ===\n";
            std::cout << "1. List all chats\n";
            std::cout << "2. Create new chat\n";
            std::cout << "3. Join chat\n";
            std::cout << "4. Send message to chat\n";
            std::cout << "5. List members of chat\n";
            std::cout << "6. Add member to chat\n";
            std::cout << "7. Exit\n";
            std::cout << "Choose option: ";

            int option;
            std::cin >> option;
            std::cin.ignore();

            if (option == 1) {
                auto chats = run_and_wait(ioc, chatService->getAllChats());
                std::vector<ChatDTO> dtos;
                for (const auto& c : chats) dtos.push_back(ChatDTOConverter::toDto(c));
                printChats(dtos);
            }
            else if (option == 2) {
                std::string title, roomId, typeStr;
                std::cout << "Enter chat title: ";
                std::getline(std::cin, title);
                std::cout << "Enter roomId (unique server ID): ";
                std::getline(std::cin, roomId);
                std::cout << "Enter type (direct/group): ";
                std::getline(std::cin, typeStr);

                ChatDTO newChatDTO;
                newChatDTO.setId(boost::uuids::to_string(boost::uuids::random_generator()()));
                newChatDTO.setRoomId(roomId);
                newChatDTO.setType(typeStr);
                newChatDTO.setTitle(title);
                newChatDTO.setCreatedAt(TimePointConverter::toIsoString(std::chrono::system_clock::now()));
                newChatDTO.setLastMessageNumber("0");

                ChatModel newChat = ChatDTOConverter::fromDto(newChatDTO);
                run_and_wait(ioc, chatService->addChat(newChat));
                std::cout << "Chat created.\n";
            }
            else if (option == 3) {
                std::string roomId, username, chatName;
                std::cout << "Enter roomId: ";
                std::getline(std::cin, roomId);
                std::cout << "Enter chat name: ";
                std::getline(std::cin, chatName);
                std::cout << "Enter your username: ";
                std::getline(std::cin, username);

                std::string roomType = "direct";
                auto chats = run_and_wait(ioc, chatService->getAllChats());
                for (const auto& c : chats) {
                    if (UUIDConverter::toString(c.getRoomId()) == roomId) {
                        roomType = ChatTypeConverter::toString(c.getType());
                        break;
                    }
                }
                if (roomType.empty()) {
                    std::cout << "Chat not found locally, assuming type direct.\n";
                    roomType = "direct";
                }

                ChatDTO newChatDTO;
                newChatDTO.setId(boost::uuids::to_string(boost::uuids::random_generator()()));
                newChatDTO.setRoomId(roomId);
                newChatDTO.setType(roomType);
                newChatDTO.setTitle(chatName);
                newChatDTO.setCreatedAt(TimePointConverter::toIsoString(std::chrono::system_clock::now()));
                newChatDTO.setLastMessageNumber("0");

                commController->joinChat(newChatDTO, username);
                std::cout << "Join request sent.\n";
            }
            else if (option == 4) {
                auto chats = run_and_wait(ioc, chatService->getAllChats());
                std::vector<ChatDTO> dtos;
                for (const auto& c : chats) dtos.push_back(ChatDTOConverter::toDto(c));
                printChats(dtos);
                if (chats.empty()) continue;

                std::cout << "Select chat number: ";
                size_t idx;
                std::cin >> idx;
                std::cin.ignore();
                if (idx < 1 || idx > chats.size()) {
                    std::cout << "Invalid selection.\n";
                    continue;
                }

                const auto& chatDTO = dtos[idx - 1];
                std::string content;
                std::cout << "Enter message: ";
                std::getline(std::cin, content);

                MessageDTO msgDTO;
                msgDTO.setId(boost::uuids::to_string(boost::uuids::random_generator()()));
                msgDTO.setChatId(chatDTO.getId());
                msgDTO.setDirection("outgoing");
                msgDTO.setContent(content);
                msgDTO.setCreatedAt(TimePointConverter::toIsoString(std::chrono::system_clock::now()));
                msgDTO.setDeliveryState("pending");
                // sender_id будет установлен сервисом или контроллером
                msgDTO.setSenderId("");

                commController->sendMessage(msgDTO);
                std::cout << "Message sent.\n";
            }
            else if (option == 5) {
                auto chats = run_and_wait(ioc, chatService->getAllChats());
                std::vector<ChatDTO> dtos;
                for (const auto& c : chats) dtos.push_back(ChatDTOConverter::toDto(c));
                printChats(dtos);
                if (chats.empty()) continue;

                std::cout << "Select chat number: ";
                size_t idx;
                std::cin >> idx;
                std::cin.ignore();
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
                auto chats = run_and_wait(ioc, chatService->getAllChats());
                std::vector<ChatDTO> dtos;
                for (const auto& c : chats) dtos.push_back(ChatDTOConverter::toDto(c));
                printChats(dtos);
                if (chats.empty()) continue;

                std::cout << "Select chat number: ";
                size_t chatIdx;
                std::cin >> chatIdx;
                std::cin.ignore();
                if (chatIdx < 1 || chatIdx > chats.size()) {
                    std::cout << "Invalid selection.\n";
                    continue;
                }
                const auto& chatDTO = dtos[chatIdx - 1];
                auto chatModel = ChatDTOConverter::fromDto(chatDTO);

                auto allMembers = run_and_wait(ioc, memberService->getAllMembers());
                if (allMembers.empty()) {
                    std::cout << "No members in system. Create one first.\n";
                    continue;
                }
                std::cout << "Available members:\n";
                for (size_t i = 0; i < allMembers.size(); ++i) {
                    std::cout << i + 1 << ". " << allMembers[i].getUsername()
                              << " (id: " << UUIDConverter::toString(allMembers[i].getId()) << ")\n";
                }
                std::cout << "Select member number: ";
                size_t memberIdx;
                std::cin >> memberIdx;
                std::cin.ignore();
                if (memberIdx < 1 || memberIdx > allMembers.size()) {
                    std::cout << "Invalid selection.\n";
                    continue;
                }
                const auto& member = allMembers[memberIdx - 1];

                run_and_wait(ioc, chatService->addMemberToChat(chatModel.getId(), member.getId()));
                std::cout << "Member added to chat.\n";
            }
            else if (option == 7) {
                running = false;
            }
            else {
                std::cout << "Unknown option.\n";
            }
        }

        signaling->disconnectFromServer();
        ioc.stop();
        ioThread.join();
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}