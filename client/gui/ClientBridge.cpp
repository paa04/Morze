#include "ClientBridge.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <chrono>
#include <exception>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>

#include "GlobalEnums.h"
#include "MessageModel.h"
#include "UUIDConverter.h"

ChatListModel::ChatListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int ChatListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(items_.size());
}

QVariant ChatListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(items_.size())) {
        return {};
    }

    const auto &item = items_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case ChatIdRole:
        return item.chatId;
    case TitleRole:
        return item.title;
    case SubtitleRole:
        return item.subtitle;
    case LastTextRole:
        return item.lastText;
    case TimeRole:
        return item.time;
    case UnreadRole:
        return item.unread;
    case AvatarLetterRole:
        return item.avatarLetter;
    case AvatarHueRole:
        return item.avatarHue;
    default:
        return {};
    }
}

QHash<int, QByteArray> ChatListModel::roleNames() const {
    return {
        {ChatIdRole, "chatId"},
        {TitleRole, "title"},
        {SubtitleRole, "subtitle"},
        {LastTextRole, "lastText"},
        {TimeRole, "time"},
        {UnreadRole, "unread"},
        {AvatarLetterRole, "avatarLetter"},
        {AvatarHueRole, "avatarHue"}};
}

void ChatListModel::setItems(std::vector<Item> items) {
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

const ChatListModel::Item *ChatListModel::itemAt(int row) const {
    if (row < 0 || row >= static_cast<int>(items_.size())) {
        return nullptr;
    }
    return &items_[static_cast<std::size_t>(row)];
}

MessageListModel::MessageListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int MessageListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(items_.size());
}

QVariant MessageListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(items_.size())) {
        return {};
    }

    const auto &item = items_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case IsOutgoingRole:
        return item.isOutgoing;
    case NameRole:
        return item.name;
    case TextRole:
        return item.text;
    case TimeRole:
        return item.time;
    default:
        return {};
    }
}

QHash<int, QByteArray> MessageListModel::roleNames() const {
    return {
        {IsOutgoingRole, "isOutgoing"},
        {NameRole, "name"},
        {TextRole, "text"},
        {TimeRole, "time"}};
}

void MessageListModel::setItems(std::vector<Item> items) {
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
}

ClientBridge::ClientBridge(QObject *parent)
    : QObject(parent)
{
    try {
        database_ = std::make_shared<Database>(resolveDatabasePath());
        if (!database_->init()) {
            throw std::runtime_error("Не удалось инициализировать базу данных");
        }

        auto storage = database_->storage();
        chatRepository_ = std::make_shared<ChatRepository>(ioc_, storage);
        memberRepository_ = std::make_shared<ChatMemberRepository>(ioc_, storage);
        messageRepository_ = std::make_shared<MessageRepository>(ioc_, storage);
        profileRepository_ = std::make_shared<ConnectionProfileRepository>(ioc_, storage);

        chatService_ = std::make_shared<ChatService>(chatRepository_);
        memberService_ = std::make_shared<ChatMemberService>(memberRepository_);
        messageService_ = std::make_shared<MessageService>(messageRepository_);
        profileService_ = std::make_shared<ConnectionProfileService>(profileRepository_);

        workGuard_.emplace(ioc_.get_executor());
        ioThread_ = std::thread([this]() { ioc_.run(); });
    } catch (const std::exception &e) {
        emit errorOccurred(QString::fromUtf8(e.what()));
    }
}

ClientBridge::~ClientBridge() {
    workGuard_.reset();
    ioc_.stop();
    if (ioThread_.joinable()) {
        ioThread_.join();
    }
}

QAbstractItemModel *ClientBridge::chatModel() {
    return &chatModel_;
}

QAbstractItemModel *ClientBridge::messageModel() {
    return &messageModel_;
}

QString ClientBridge::selectedChatId() const {
    return selectedChatId_;
}

QString ClientBridge::selectedTitle() const {
    const auto *item = chatModel_.itemAt(selectedChatIndex_);
    return item ? item->title : QString();
}

QString ClientBridge::selectedSubtitle() const {
    const auto *item = chatModel_.itemAt(selectedChatIndex_);
    return item ? item->subtitle : QString();
}

QString ClientBridge::selectedAvatarLetter() const {
    const auto *item = chatModel_.itemAt(selectedChatIndex_);
    return item ? item->avatarLetter : QStringLiteral("?");
}

int ClientBridge::selectedAvatarHue() const {
    const auto *item = chatModel_.itemAt(selectedChatIndex_);
    return item ? item->avatarHue : 210;
}

QString ClientBridge::netBindAddress() const {
    return netBindAddress_;
}

QString ClientBridge::netStunServer() const {
    return netStunServer_;
}

QString ClientBridge::netRelayServer() const {
    return netRelayServer_;
}

void ClientBridge::refreshAll() {
    refreshProfiles();
    refreshChats();
}

void ClientBridge::selectChatByIndex(int index) {
    const auto *item = chatModel_.itemAt(index);
    if (!item) {
        selectedChatIndex_ = -1;
        selectedChatId_.clear();
        messageModel_.setItems({});
        emit selectedChatIdChanged();
        return;
    }
    selectedChatIndex_ = index;
    selectedChatId_ = item->chatId;
    emit selectedChatIdChanged();
    refreshMessages();
}

void ClientBridge::sendMessage(const QString &text) {
    if (!messageService_ || selectedChatId_.isEmpty()) {
        return;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    const std::string chatId = selectedChatId_.toStdString();
    const std::string content = trimmed.toStdString();

    boost::asio::co_spawn(
        ioc_,
        [this, chatId, content]() -> boost::asio::awaitable<void> {
            try {
                boost::uuids::string_generator parser;
                const boost::uuids::uuid chatUuid = parser(chatId);
                const auto members = co_await memberService_->getMembersByChatId(chatUuid);
                if (members.empty()) {
                    throw std::runtime_error("В выбранном чате нет участника для sender_id");
                }

                MessageModel message(
                    chatUuid,
                    members.front().getId(),
                    MessageDirection::Outgoing,
                    content,
                    std::chrono::system_clock::now(),
                    DeliveryState::Pending);
                co_await messageService_->addMessage(message);
                QMetaObject::invokeMethod(this, [this]() {
                    refreshMessages();
                    refreshChats();
                }, Qt::QueuedConnection);
            } catch (const std::exception &e) {
                QMetaObject::invokeMethod(this, [this, message = QString::fromUtf8(e.what())]() {
                    emit errorOccurred(message);
                }, Qt::QueuedConnection);
            }
        },
        boost::asio::detached);
}

void ClientBridge::updateNetworkSettings(const QString &bindAddress, const QString &stunServer, const QString &relayServer) {
    netBindAddress_ = bindAddress.trimmed();
    netStunServer_ = stunServer.trimmed();
    netRelayServer_ = relayServer.trimmed();
    emit networkSettingsChanged();
}

void ClientBridge::refreshProfiles() {
    if (!profileService_) {
        return;
    }

    boost::asio::co_spawn(
        ioc_,
        [this]() -> boost::asio::awaitable<void> {
            try {
                const auto profiles = co_await profileService_->getAllProfiles(true);
                if (!profiles.empty()) {
                    const auto &profile = profiles.front();
                    const QString serverUrl = QString::fromStdString(profile.getServerUrl());
                    const QString stunUrl = QString::fromStdString(profile.getStunUrl());
                    QMetaObject::invokeMethod(this, [this, serverUrl, stunUrl]() {
                        netBindAddress_ = serverUrl;
                        netStunServer_ = stunUrl;
                        emit networkSettingsChanged();
                    }, Qt::QueuedConnection);
                }
            } catch (const std::exception &) {
            }
        },
        boost::asio::detached);
}

void ClientBridge::refreshChats() {
    if (!chatService_ || !messageService_) {
        return;
    }

    boost::asio::co_spawn(
        ioc_,
        [this]() -> boost::asio::awaitable<void> {
            try {
                const auto chats = co_await chatService_->getAllChats();
                std::vector<ChatListModel::Item> items;
                items.reserve(chats.size());

                for (const auto &chat : chats) {
                    ChatListModel::Item item;
                    item.chatId = QString::fromStdString(chat.getIdAsString());
                    item.title = QString::fromStdString(chat.getTitle());
                    item.subtitle = QString::fromStdString(chat.getTypeAsString());
                    item.avatarHue = avatarHueForTitle(chat.getTitle());
                    item.avatarLetter = avatarLetterForTitle(chat.getTitle());

                    const auto messages = co_await messageService_->getMessagesByChatId(chat.getId());
                    if (!messages.empty()) {
                        const auto &latest = messages.front();
                        item.lastText = QString::fromStdString(latest.getContent());
                        item.time = formatMessageTime(latest.getCreatedAt());
                    } else {
                        item.lastText = QStringLiteral("Нет сообщений");
                        item.time = QString();
                    }
                    items.push_back(std::move(item));
                }

                QMetaObject::invokeMethod(this, [this, items = std::move(items)]() mutable {
                    chatModel_.setItems(std::move(items));
                    if (selectedChatIndex_ < 0 && chatModel_.rowCount() > 0) {
                        selectChatByIndex(0);
                    } else if (selectedChatIndex_ >= chatModel_.rowCount()) {
                        selectChatByIndex(chatModel_.rowCount() - 1);
                    } else if (selectedChatIndex_ >= 0) {
                        const auto *selected = chatModel_.itemAt(selectedChatIndex_);
                        if (selected) {
                            selectedChatId_ = selected->chatId;
                            emit selectedChatIdChanged();
                            refreshMessages();
                        }
                    } else {
                        messageModel_.setItems({});
                    }
                }, Qt::QueuedConnection);
            } catch (const std::exception &e) {
                QMetaObject::invokeMethod(this, [this, message = QString::fromUtf8(e.what())]() {
                    emit errorOccurred(message);
                }, Qt::QueuedConnection);
            }
        },
        boost::asio::detached);
}

void ClientBridge::refreshMessages() {
    if (!messageService_ || selectedChatId_.isEmpty()) {
        messageModel_.setItems({});
        return;
    }

    const std::string chatId = selectedChatId_.toStdString();
    boost::asio::co_spawn(
        ioc_,
        [this, chatId]() -> boost::asio::awaitable<void> {
            try {
                boost::uuids::string_generator parser;
                const boost::uuids::uuid chatUuid = parser(chatId);
                auto messages = co_await messageService_->getMessagesByChatId(chatUuid);
                std::reverse(messages.begin(), messages.end());

                std::vector<MessageListModel::Item> items;
                items.reserve(messages.size());
                for (const auto &msg : messages) {
                    MessageListModel::Item item;
                    item.isOutgoing = (msg.getDirection() == MessageDirection::Outgoing);
                    item.text = QString::fromStdString(msg.getContent());
                    item.time = formatMessageTime(msg.getCreatedAt());
                    item.name = item.isOutgoing ? QString() : QString::fromStdString(msg.getSenderIdAsString());
                    items.push_back(std::move(item));
                }

                QMetaObject::invokeMethod(this, [this, items = std::move(items)]() mutable {
                    messageModel_.setItems(std::move(items));
                }, Qt::QueuedConnection);
            } catch (const std::exception &e) {
                QMetaObject::invokeMethod(this, [this, message = QString::fromUtf8(e.what())]() {
                    emit errorOccurred(message);
                }, Qt::QueuedConnection);
            }
        },
        boost::asio::detached);
}

QString ClientBridge::formatMessageTime(const std::chrono::system_clock::time_point &tp) const {
    const qint64 secs = static_cast<qint64>(std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count());
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(secs);
    return dt.toString("HH:mm");
}

int ClientBridge::avatarHueForTitle(const std::string &title) const {
    int hash = 0;
    for (char c : title) {
        hash = (hash * 131 + static_cast<unsigned char>(c)) % 360;
    }
    return hash;
}

QString ClientBridge::avatarLetterForTitle(const std::string &title) const {
    if (title.empty()) {
        return QStringLiteral("?");
    }
    return QString::fromUtf8(title.substr(0, 1).c_str()).toUpper();
}

std::string ClientBridge::resolveDatabasePath() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/chat.db",
        appDir + "/../chat.db",
        appDir + "/../../chat.db",
        QDir::currentPath() + "/chat.db",
        QDir::currentPath() + "/../chat.db"
    };

    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return path.toStdString();
        }
    }
    return "chat.db";
}

