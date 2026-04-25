//
// Created by ivan on 22.04.2026.
//

#ifndef MORZE_DPENDENCYCONTAINER_H
#define MORZE_DPENDENCYCONTAINER_H

#include <memory>
#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <thread>

#include "ConfigManager.h"
#include "Database.h"
#include "ChatRepository.h"
#include "ChatMemberRepository.h"
#include "MessageRepository.h"
#include "ConnectionProfileRepository.h"
#include "UserRepository.h"
#include "ChatService.h"
#include "ChatMemberService.h"
#include "MessageService.h"
#include "ConnectionProfileService.h"
#include "UserService.h"
#include "SignalingService.h"
#include "WebRTCService.h"
#include "CommunicationController.h"

class DependencyContainer {
public:
    explicit DependencyContainer(const std::string& configPath, const std::string& dbPathOverride = "");
    ~DependencyContainer();

    // Запуск асинхронного контекста (опционально, если нужно отдельно)
    void runIoContext();
    void stopIoContext();

    // Геттеры для компонентов
    boost::asio::io_context& ioContext() { return ioc_; }
    std::shared_ptr<ChatService> chatService() const { return chatService_; }
    std::shared_ptr<ChatMemberService> memberService() const { return memberService_; }
    std::shared_ptr<MessageService> messageService() const { return messageService_; }
    std::shared_ptr<ConnectionProfileService> profileService() const { return profileService_; }
    std::shared_ptr<UserService> userService() const { return userService_; }
    std::shared_ptr<SignalingService> signalingService() const { return signaling_; }
    std::shared_ptr<WebRTCService> webRTCService() const { return webRTC_; }
    std::shared_ptr<CommunicationController> communicationController() const { return commController_; }

private:
    void initDatabase();
    void initRepositories();
    void initServices();
    void initNetworkServices();
    void initControllers();
    void applyStunServersFromProfile();


    std::string configPath_;
    std::unique_ptr<ConfigManager> config_;
    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> workGuard_{boost::asio::make_work_guard(ioc_)};
    std::shared_ptr<Database> database_;

    // Репозитории
    std::shared_ptr<ChatRepository> chatRepo_;
    std::shared_ptr<ChatMemberRepository> memberRepo_;
    std::shared_ptr<MessageRepository> messageRepo_;
    std::shared_ptr<ConnectionProfileRepository> profileRepo_;
    std::shared_ptr<UserRepository> userRepo_;

    // Сервисы
    std::shared_ptr<ChatService> chatService_;
    std::shared_ptr<ChatMemberService> memberService_;
    std::shared_ptr<MessageService> messageService_;
    std::shared_ptr<ConnectionProfileService> profileService_;
    std::shared_ptr<UserService> userService_;

    // Сетевые сервисы
    std::shared_ptr<SignalingService> signaling_;
    std::shared_ptr<WebRTCService> webRTC_;

    // Контроллеры
    std::shared_ptr<CommunicationController> commController_;

    std::thread ioThread_;
};


#endif //MORZE_DPENDENCYCONTAINER_H