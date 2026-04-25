#include "DependencyContainer.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <thread>

DependencyContainer::DependencyContainer(const std::string& configPath, const std::string& dbPathOverride)
    : configPath_(configPath)
    , ioc_(std::thread::hardware_concurrency())
{
    config_ = std::make_unique<ConfigManager>(ConfigManager::loadFromFile(configPath));
    if (!dbPathOverride.empty()) {
        config_->setDatabasePath(dbPathOverride);
    }
    initDatabase();
    initRepositories();
    initServices();
    initNetworkServices();
    initControllers();

    applyStunServersFromProfile();
    // Запускаем io_context в отдельном потоке
    ioThread_ = std::thread([this]() { ioc_.run(); });
}

void DependencyContainer::applyStunServersFromProfile()
{
    boost::asio::co_spawn(ioc_, [this]() -> boost::asio::awaitable<void> {
        try {
            auto profiles = co_await profileService_->getAllProfiles(true);
            std::vector<std::string> stunServers;

            if (!profiles.empty()) {
                const auto& latest = profiles.front();
                if (!latest.getStunUrl().empty())
                    stunServers.push_back(latest.getStunUrl());
            }

            // Если из профиля не получили — берём из конфига
            if (stunServers.empty()) {
                stunServers = config_->signaling().stun_urls;
            }

            webRTC_->setStunServers(stunServers);
            std::cout << "STUN servers configured (" << stunServers.size() << ")\n";
        } catch (const std::exception& e) {
            std::cerr << "Failed to load STUN servers: " << e.what() << '\n';
            webRTC_->setStunServers({"stun:stun.l.google.com:19302"});
        }
    }, boost::asio::detached);
}

DependencyContainer::~DependencyContainer()
{
    stopIoContext();
    if (ioThread_.joinable())
        ioThread_.join();
}

void DependencyContainer::runIoContext()
{
    // уже запущен в конструкторе
}

void DependencyContainer::stopIoContext()
{
    workGuard_.reset();
    ioc_.stop();
}

void DependencyContainer::initDatabase()
{
    database_ = std::make_shared<Database>(config_->database().path);
    if (!database_->init()) {
        throw std::runtime_error("Failed to initialize database");
    }
}

void DependencyContainer::initRepositories()
{
    auto storage = database_->storage();
    chatRepo_ = std::make_shared<ChatRepository>(ioc_, storage);
    memberRepo_ = std::make_shared<ChatMemberRepository>(ioc_, storage);
    messageRepo_ = std::make_shared<MessageRepository>(ioc_, storage);
    profileRepo_ = std::make_shared<ConnectionProfileRepository>(ioc_, storage);
    userRepo_ = std::make_shared<UserRepository>(ioc_, storage);
}

void DependencyContainer::initServices()
{
    chatService_ = std::make_shared<ChatService>(chatRepo_);
    memberService_ = std::make_shared<ChatMemberService>(memberRepo_);
    messageService_ = std::make_shared<MessageService>(messageRepo_);
    profileService_ = std::make_shared<ConnectionProfileService>(profileRepo_);
    userService_ = std::make_shared<UserService>(userRepo_);
}

void DependencyContainer::initNetworkServices()
{
    signaling_ = std::make_shared<SignalingService>();
    webRTC_ = std::make_shared<WebRTCService>();
}

void DependencyContainer::initControllers()
{
    commController_ = std::make_shared<CommunicationController>(signaling_.get(), webRTC_.get());

    // Подключаем сигналы/слоты между сервисами и контроллером
    // (если используются Qt-сигналы, нужно QObject::connect)
    // Для чистоты можно вынести в отдельный метод connectSignals()
}