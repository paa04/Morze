#include "ClientBridge.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QHostInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QSettings>
#include <QClipboard>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <chrono>
#include <exception>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>

#include "GlobalEnums.h"
#include "MessageModel.h"
#include "UUIDConverter.h"

namespace {
/** Поле ввода: host:port; подключение WebSocket: ws://… */
QString normalizeSignalingForConnect(const QString &input) {
    const QString t = input.trimmed();
    if (t.isEmpty()) {
        return {};
    }
    if (t.startsWith("wss://", Qt::CaseInsensitive) || t.startsWith("ws://", Qt::CaseInsensitive)) {
        return t;
    }
    return QStringLiteral("ws://%1").arg(t);
}

QString displaySignalingInSettingsField(const QString &urlOrHostPort) {
    QString t = urlOrHostPort.trimmed();
    if (t.startsWith("ws://", Qt::CaseInsensitive)) {
        t = t.mid(5);
    } else if (t.startsWith("wss://", Qt::CaseInsensitive)) {
        t = t.mid(6);
    }
    return t;
}
} // namespace

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

ParticipantListModel::ParticipantListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int ParticipantListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(items_.size());
}

QVariant ParticipantListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(items_.size())) {
        return {};
    }

    const auto &item = items_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case NameRole:
        return item.name;
    case AvatarLetterRole:
        return item.avatarLetter;
    case AvatarHueRole:
        return item.avatarHue;
    default:
        return {};
    }
}

QHash<int, QByteArray> ParticipantListModel::roleNames() const {
    return {
        {NameRole, "name"},
        {AvatarLetterRole, "avatarLetter"},
        {AvatarHueRole, "avatarHue"}};
}

void ParticipantListModel::setItems(std::vector<Item> items) {
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
        signalingService_ = std::make_shared<SignalingService>();

        connect(signalingService_.get(), &SignalingService::joined, this,
                [this](const QString &roomId, const QString &roomType, const QString &peerId, const QJsonArray &participants) {
                    qInfo().noquote() << "[GUI] signaling joined roomId=" << roomId
                                      << " roomType=" << roomType << " peerId=" << peerId;
                    if (!roomId.isEmpty() && !peerId.isEmpty()) {
                        myPeerIdByRoom_[roomId] = peerId;
                        if (webRTCService_)
                            webRTCService_->setLocalPeerId(peerId);
                    }
                    roomTypeByRoom_[roomId] = roomType;
                    // Track existing participants (WebRTC initiated by peerJoined, not here —
                    // initiating here causes glare because the other side also initiates from peerJoined)
                    for (const QJsonValue &v : participants) {
                        QJsonObject obj = v.toObject();
                        QString pid = obj["peerId"].toString();
                        if (!pid.isEmpty() && pid != peerId) {
                            peerToRoom_[pid] = roomId;
                            peerUsernames_[roomId + ":" + pid] = obj["username"].toString();
                        }
                    }
                    handleSignalingJoined(roomId, roomType);
                });
        connect(signalingService_.get(), &SignalingService::connected, this, [this]() {
            qInfo().noquote() << "[GUI] signaling connected";
            if (!pendingJoinRoomId_.isEmpty() && !pendingJoinNickname_.isEmpty()) {
                qInfo().noquote() << "[GUI] replay pending join roomId=" << pendingJoinRoomId_
                                  << " roomType=" << pendingJoinRoomType_;
                signalingService_->join(pendingJoinRoomId_, pendingJoinNickname_, pendingJoinRoomType_);
            }
        });
        connect(signalingService_.get(), &SignalingService::errorReceived, this,
                [this](const QString &message) {
                    qWarning().noquote() << "[GUI] signaling protocol error:" << message;
                    pendingJoinRoomId_.clear();
                    pendingJoinNickname_.clear();
                    pendingJoinTitle_.clear();
                    pendingJoinMode_.clear();
                    pendingJoinRoomType_.clear();
                    emit errorOccurred(message);
                });
        connect(signalingService_.get(), &SignalingService::errorOccurred, this,
                [this](const QString &message) {
                    qWarning().noquote() << "[GUI] signaling transport error:" << message;
                    pendingJoinRoomId_.clear();
                    pendingJoinNickname_.clear();
                    pendingJoinTitle_.clear();
                    pendingJoinMode_.clear();
                    pendingJoinRoomType_.clear();
                    emit errorOccurred(message);
                });

        loadChatNicknames();
        loadStunServersFromConfig();
        loadDefaultNetworkState();

        // --- WebRTC for direct rooms ---
        {
            std::vector<std::string> stunForWebRtc;
            for (const QString &s : stunServerOptions_)
                stunForWebRtc.push_back(ensureStunPrefix(s).toStdString());
            if (stunForWebRtc.empty())
                stunForWebRtc.push_back("stun:stun.l.google.com:19302");
            webRTCService_ = std::make_shared<WebRTCService>(stunForWebRtc);
        }

        // Signaling → WebRTC
        connect(signalingService_.get(), &SignalingService::offerReceived,
                webRTCService_.get(), &WebRTCService::onOfferReceived);
        connect(signalingService_.get(), &SignalingService::answerReceived,
                webRTCService_.get(), &WebRTCService::onAnswerReceived);
        connect(signalingService_.get(), &SignalingService::iceCandidateReceived,
                webRTCService_.get(), &WebRTCService::onIceCandidateReceived);

        // WebRTC → Signaling
        connect(webRTCService_.get(), &WebRTCService::sendOffer, this,
                [this](const QString &roomId, const QString &peerId, const QJsonObject &sdp) {
                    signalingService_->offer(roomId, peerId, sdp);
                });
        connect(webRTCService_.get(), &WebRTCService::sendAnswer, this,
                [this](const QString &roomId, const QString &peerId, const QJsonObject &sdp) {
                    signalingService_->answer(roomId, peerId, sdp);
                });
        connect(webRTCService_.get(), &WebRTCService::sendIceCandidate, this,
                [this](const QString &roomId, const QString &peerId, const QJsonObject &candidate) {
                    signalingService_->iceCandidate(roomId, peerId, candidate);
                });

        // WebRTC incoming P2P message
        connect(webRTCService_.get(), &WebRTCService::messageReceived, this,
                [this](const QString &peerId, const QByteArray &data) {
                    auto it = peerToRoom_.find(peerId);
                    if (it != peerToRoom_.end())
                        handleIncomingMessage(*it, peerId, QString::fromUtf8(data));
                });

        // DataChannel opened → flush queued direct messages
        connect(webRTCService_.get(), &WebRTCService::connectionOpened, this,
                [this](const QString &peerId) {
                    qInfo().noquote() << "[GUI] WebRTC DataChannel open:" << peerId;
                    flushPendingDirectMessages(peerId);
                });

        // Peer joined → track + initiate WebRTC for direct rooms
        connect(signalingService_.get(), &SignalingService::peerJoined, this,
                [this](const QString &roomId, const QString &peerId, const QString &username) {
                    peerToRoom_[peerId] = roomId;
                    peerUsernames_[roomId + ":" + peerId] = username;
                    if (roomTypeByRoom_.value(roomId) == "direct" && webRTCService_)
                        webRTCService_->initiateConnection(roomId, peerId);
                });

        // Peer left → cleanup WebRTC
        connect(signalingService_.get(), &SignalingService::peerLeft, this,
                [this](const QString &roomId, const QString &peerId) {
                    peerToRoom_.remove(peerId);
                    if (webRTCService_)
                        webRTCService_->closeConnection(peerId);
                });

        // Group messages (relay through signaling server)
        connect(signalingService_.get(), &SignalingService::groupMessageReceived, this,
                [this](const QString &roomId, const QString &fromPeerId, qint64 seq, const QJsonObject &payload) {
                    // Skip own echoed messages
                    if (fromPeerId == myPeerIdByRoom_.value(roomId))
                        return;
                    handleIncomingMessage(roomId, fromPeerId, payload["text"].toString());
                    signalingService_->ack(roomId, seq);
                });

        // Buffered messages on reconnect
        connect(signalingService_.get(), &SignalingService::bufferedMessagesReceived, this,
                [this](const QString &roomId, const QJsonArray &messages) {
                    for (const QJsonValue &v : messages) {
                        QJsonObject obj = v.toObject();
                        QString fromPeerId = obj["fromPeerId"].toString();
                        if (fromPeerId == myPeerIdByRoom_.value(roomId))
                            continue;
                        handleIncomingMessage(roomId, fromPeerId,
                                              obj["payload"].toObject()["text"].toString());
                        signalingService_->ack(roomId, obj["messageSeq"].toInteger());
                    }
                });

        workGuard_.emplace(ioc_.get_executor());
        ioThread_ = std::thread([this]() { ioc_.run(); });

        // Auto-connect to signaling server on startup
        ensureSignalingConnected();
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

QAbstractItemModel *ClientBridge::participantModel() {
    return &participantModel_;
}

QString ClientBridge::selectedChatId() const {
    return selectedChatId_;
}

QString ClientBridge::selectedRoomId() const {
    return selectedRoomId_;
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

QString ClientBridge::chatSearchQuery() const {
    return chatSearchQuery_;
}

QStringList ClientBridge::stunServerOptions() const {
    return stunServerOptions_;
}

QString ClientBridge::bindCheckText() const {
    return bindCheckText_;
}

bool ClientBridge::bindCheckOk() const {
    return bindCheckOk_;
}

QString ClientBridge::stunCheckText() const {
    return stunCheckText_;
}

bool ClientBridge::stunCheckOk() const {
    return stunCheckOk_;
}

QString ClientBridge::relayCheckText() const {
    return relayCheckText_;
}

bool ClientBridge::relayCheckOk() const {
    return relayCheckOk_;
}

void ClientBridge::refreshAll() {
    refreshProfiles();
    refreshChats();
    refreshMessages();
    refreshParticipants();
}

void ClientBridge::refreshChatData() {
    refreshChats();
    refreshMessages();
    refreshParticipants();
}

void ClientBridge::setChatSearchQuery(const QString &query) {
    const QString normalized = query.trimmed();
    if (chatSearchQuery_ == normalized) {
        return;
    }
    chatSearchQuery_ = normalized;
    emit chatSearchQueryChanged();
    applyChatFilter();
}

void ClientBridge::selectChatByIndex(int index) {
    const auto *item = chatModel_.itemAt(index);
    if (!item) {
        const bool wasSelected = !selectedChatId_.isEmpty() || selectedChatIndex_ != -1;
        selectedChatIndex_ = -1;
        selectedChatId_.clear();
        selectedRoomId_.clear();
        messageModel_.setItems({});
        if (wasSelected) {
            emit selectedChatIdChanged();
        }
        return;
    }

    const QString newSelectedChatId = item->chatId;
    const QString newSelectedRoomId = item->roomId;
    const bool changed = (selectedChatIndex_ != index) || (selectedChatId_ != newSelectedChatId) ||
                         (selectedRoomId_ != newSelectedRoomId);
    selectedChatIndex_ = index;
    selectedChatId_ = newSelectedChatId;
    selectedRoomId_ = newSelectedRoomId;
    if (changed) {
        emit selectedChatIdChanged();
    }
    refreshMessages();
    refreshParticipants();
}

void ClientBridge::sendMessage(const QString &text) {
    if (!messageService_ || !chatService_ || !memberService_ || selectedChatId_.isEmpty()) {
        return;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    const std::string chatId = selectedChatId_.toStdString();
    const std::string content = trimmed.toStdString();
    const QString textCopy = trimmed;

    boost::asio::co_spawn(
        ioc_,
        [this, chatId, content, textCopy]() -> boost::asio::awaitable<void> {
            try {
                boost::uuids::string_generator parser;
                const boost::uuids::uuid chatUuid = parser(chatId);
                const auto chat = co_await chatService_->getChatById(chatUuid);
                const QString roomIdForServer = QString::fromStdString(chat.getRoomIdAsString());

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
                // Signaling: поток asio, QWebSocket — в потоке Qt, после записи в БД шлём на сигнальный сервер
                QMetaObject::invokeMethod(
                    this,
                    [this, roomIdForServer, textCopy]() {
                        const QString roomType = roomTypeByRoom_.value(roomIdForServer, "group");
                        if (roomType == "direct") {
                            // Send via WebRTC P2P
                            const QString myPeerId = myPeerIdByRoom_.value(roomIdForServer);
                            QByteArray data = textCopy.toUtf8();
                            for (auto it = peerToRoom_.cbegin(); it != peerToRoom_.cend(); ++it) {
                                if (it.value() == roomIdForServer && it.key() != myPeerId) {
                                    if (webRTCService_ && webRTCService_->isDataChannelOpen(it.key())) {
                                        webRTCService_->sendMessage(it.key(), data);
                                    } else {
                                        pendingDirectMessages_[it.key()].append(data);
                                        if (webRTCService_) {
                                            webRTCService_->initiateConnection(roomIdForServer, it.key());
                                        }
                                    }
                                }
                            }
                        } else {
                            // Send via signaling relay (group message)
                            if (signalingService_ && signalingService_->isConnected()) {
                                QJsonObject payload;
                                payload["text"] = textCopy;
                                payload["kind"] = "chat";
                                signalingService_->groupMessage(roomIdForServer, payload);
                            }
                        }
                        refreshMessages();
                        refreshParticipants();
                        refreshChats();
                    },
                    Qt::QueuedConnection);
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
    netStunServer_ = stripStunPrefix(stunServer);
    netRelayServer_ = relayServer.trimmed();
    {
        QSettings s("Morze", "MorzeGUI");
        s.setValue("ui/netBindAddress", netBindAddress_);
    }
    emit networkSettingsChanged();

    if (!profileService_) {
        return;
    }

    const std::string stunUrl = ensureStunPrefix(netStunServer_).toStdString();
    const QString connectWs = normalizeSignalingForConnect(netRelayServer_);

    boost::asio::co_spawn(
        ioc_,
        [this, stunUrl, connectWs]() -> boost::asio::awaitable<void> {
            try {
                auto profiles = co_await profileService_->getAllProfiles(true);
                if (!profiles.empty()) {
                    auto latest = profiles.front();
                    if (!connectWs.isEmpty()) {
                        latest.setServerUrl(connectWs.toStdString());
                    }
                    latest.setStunUrl(stunUrl);
                    latest.setUpdatedAt(std::chrono::system_clock::now());
                    co_await profileService_->updateProfile(latest);
                } else {
                    if (connectWs.isEmpty()) {
                        co_return;
                    }
                    ConnectionProfileModel profile(connectWs.toStdString(), stunUrl, std::chrono::system_clock::now());
                    co_await profileService_->addProfile(profile);
                }
            } catch (const std::exception &e) {
                QMetaObject::invokeMethod(this, [this, message = QString::fromUtf8(e.what())]() {
                    emit errorOccurred(message);
                }, Qt::QueuedConnection);
            }
        },
        boost::asio::detached);
}

bool ClientBridge::createNewChat(const QString &nickname, const QString &title, const QString &roomType) {
    if (!chatService_ || !memberService_ || !signalingService_) {
        emit errorOccurred("Сервисы чата недоступны");
        return false;
    }

    const QString normalizedNick = nickname.trimmed();
    const QString normalizedTitle = title.trimmed();
    if (normalizedNick.isEmpty() || normalizedTitle.isEmpty()) {
        return false;
    }
    const QString normalizedRoomType =
        roomType.trimmed().compare("direct", Qt::CaseInsensitive) == 0 ? "direct" : "group";

    if (!ensureSignalingConnected()) {
        qWarning().noquote() << "[GUI] create chat aborted: signaling not connected";
        return false;
    }

    pendingJoinMode_ = "create";
    pendingJoinNickname_ = normalizedNick;
    pendingJoinTitle_ = normalizedTitle;
    pendingJoinRoomType_ = normalizedRoomType;
    pendingJoinRoomId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (signalingService_->isConnected()) {
        qInfo().noquote() << "[GUI] create chat join roomId=" << pendingJoinRoomId_
                          << " roomType=" << pendingJoinRoomType_
                          << " nickname=" << pendingJoinNickname_;
        signalingService_->join(pendingJoinRoomId_, pendingJoinNickname_, pendingJoinRoomType_);
    }

    return true;
}

void ClientBridge::removeCurrentChat() {
    if (!chatService_ || selectedChatId_.isEmpty()) {
        return;
    }
    const QString idCopy = selectedChatId_.trimmed();
    if (idCopy.isEmpty()) {
        return;
    }
    const std::string idStr = idCopy.toStdString();
    boost::uuids::string_generator gen;
    boost::uuids::uuid chatId;
    try {
        chatId = gen(idStr);
    } catch (const std::exception &) {
        emit errorOccurred("Некорректный id чата");
        return;
    }

    boost::asio::co_spawn(
        ioc_,
        [this, idCopy, chatId]() -> boost::asio::awaitable<void> {
            try {
                const auto chat = co_await chatService_->getChatById(chatId);
                const QString roomIdStr = QString::fromStdString(chat.getRoomIdAsString());

                QMetaObject::invokeMethod(
                    this,
                    [this, idCopy, chatId, roomIdStr]() {
                        if (signalingService_ && signalingService_->isConnected()) {
                            const QString myPeer = myPeerIdByRoom_.value(roomIdStr);
                            if (!myPeer.isEmpty()) {
                                signalingService_->leave(roomIdStr, myPeer);
                            }
                        }
                        myPeerIdByRoom_.remove(roomIdStr);

                        boost::asio::co_spawn(
                            ioc_,
                            [this, idCopy, chatId]() -> boost::asio::awaitable<void> {
                                try {
                                    co_await chatService_->removeChat(chatId);
                                    QMetaObject::invokeMethod(
                                        this,
                                        [this, idCopy]() {
                                            const QString t = idCopy.trimmed();
                                            chatNicknames_.remove(t);
                                            persistChatNicknames();
                                            selectChatByIndex(-1);
                                            refreshChats();
                                        },
                                        Qt::QueuedConnection);
                                } catch (const std::exception &e) {
                                    QMetaObject::invokeMethod(
                                        this,
                                        [this, message = QString::fromUtf8(e.what())]() {
                                            emit errorOccurred(message);
                                        },
                                        Qt::QueuedConnection);
                                }
                            },
                            boost::asio::detached);
                    },
                    Qt::QueuedConnection);
            } catch (const std::exception &e) {
                QMetaObject::invokeMethod(
                    this,
                    [this, message = QString::fromUtf8(e.what())]() {
                        emit errorOccurred(message);
                    },
                    Qt::QueuedConnection);
            }
        },
        boost::asio::detached);
}

bool ClientBridge::joinChatById(const QString &chatOrRoomId, const QString &nickname, const QString &title) {
    if (!chatService_ || !memberService_ || !signalingService_) {
        emit errorOccurred("Сервисы чата недоступны");
        return false;
    }

    const QString normalizedId = chatOrRoomId.trimmed();
    const QString normalizedNick = nickname.trimmed();
    const QString normalizedTitle = title.trimmed();
    if (normalizedId.isEmpty() || normalizedNick.isEmpty() || normalizedTitle.isEmpty()) {
        return false;
    }

    if (!ensureSignalingConnected()) {
        qWarning().noquote() << "[GUI] join chat aborted: signaling not connected";
        return false;
    }

    pendingJoinMode_ = "join";
    pendingJoinRoomId_ = normalizedId;
    pendingJoinNickname_ = normalizedNick;
    pendingJoinTitle_ = normalizedTitle;
    pendingJoinRoomType_.clear(); // let server determine type from existing room
    if (signalingService_->isConnected()) {
        qInfo().noquote() << "[GUI] join chat roomId=" << pendingJoinRoomId_
                          << " nickname=" << pendingJoinNickname_;
        signalingService_->join(pendingJoinRoomId_, pendingJoinNickname_, pendingJoinRoomType_);
    }

    return true;
}

void ClientBridge::setChatNickname(const QString &chatId, const QString &nickname) {
    const QString normalizedChatId = chatId.trimmed();
    const QString normalizedNickname = nickname.trimmed();
    if (normalizedChatId.isEmpty() && selectedChatId_.isEmpty()) {
        return;
    }

    QStringList keys;
    if (!normalizedChatId.isEmpty()) {
        keys.push_back(normalizedChatId);
    }
    if (!selectedChatId_.isEmpty() && !keys.contains(selectedChatId_)) {
        keys.push_back(selectedChatId_);
    }

    for (const QString &key : keys) {
        if (normalizedNickname.isEmpty()) {
            chatNicknames_.remove(key);
        } else {
            chatNicknames_.insert(key, normalizedNickname);
        }
    }
    persistChatNicknames();
    refreshParticipants();
}

QString ClientBridge::chatNickname(const QString &chatId) const {
    return chatNicknames_.value(chatId.trimmed());
}

void ClientBridge::copyToClipboard(const QString &text) const {
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

void ClientBridge::refreshBindAddress() {
    QString stunValue = ensureStunPrefix(netStunServer_);
    if (!stunValue.contains("://") && stunValue.startsWith("stun:", Qt::CaseInsensitive)) {
        stunValue = "stun://" + stunValue.mid(5);
    }

    const QUrl stunUrl(stunValue);
    const QString host = stunUrl.host().trimmed();
    const quint16 port = stunUrl.port(3478) > 0 ? static_cast<quint16>(stunUrl.port(3478)) : static_cast<quint16>(3478);
    if (host.isEmpty()) {
        setBindCheckResult(false, "Ошибка: пустой STUN URL");
        return;
    }

    QString error;
    qint64 elapsedMs = 0;
    const QString mapped = requestStunMappedAddress(host, port, &error, &elapsedMs);
    if (mapped.isEmpty()) {
        setBindCheckResult(false, QString("Ошибка STUN: %1").arg(error.isEmpty() ? "нет ответа" : error));
        return;
    }

    netBindAddress_ = mapped;
    emit networkSettingsChanged();
    setBindCheckResult(true, QString("Public endpoint: %1 · %2 ms").arg(netBindAddress_).arg(elapsedMs));
}

void ClientBridge::resolvePublicIpViaStun() {
    const QString host = extractHostFromStunValue(netStunServer_);
    if (host.isEmpty()) {
        setStunCheckResult(false, "Ошибка: пустой STUN URL");
        return;
    }

    QElapsedTimer timer;
    timer.start();
    QProcess pingProcess;
#ifdef Q_OS_WIN
    pingProcess.start("ping", {"-n", "1", "-w", "1500", host});
#else
    pingProcess.start("ping", {"-c", "1", "-W", "2", host});
#endif
    if (!pingProcess.waitForFinished(2500)) {
        pingProcess.kill();
        setStunCheckResult(false, "Таймаут ping");
        return;
    }
    if (pingProcess.exitStatus() != QProcess::NormalExit || pingProcess.exitCode() != 0) {
        setStunCheckResult(false, "Нет ответа");
        return;
    }
    setStunCheckResult(true, QString("OK · %1 ms").arg(timer.elapsed()));
}

void ClientBridge::checkRelayConnection() {
    QString host;
    quint16 port = 0;
    QString parseError;
    if (!parseEndpoint(netRelayServer_, &host, &port, &parseError)) {
        setRelayCheckResult(false, parseError);
        return;
    }

    QElapsedTimer timer;
    timer.start();
    QTcpSocket socket;
    socket.connectToHost(host, port);
    if (!socket.waitForConnected(1800)) {
        setRelayCheckResult(false, "Нет соединения");
        return;
    }

    socket.disconnectFromHost();
    setRelayCheckResult(true, QString("OK · %1 ms").arg(timer.elapsed()));
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
                    const QString signalingFromProfile = QString::fromStdString(profile.getServerUrl());
                    const QString stunUrl = stripStunPrefix(QString::fromStdString(profile.getStunUrl()));
                    QMetaObject::invokeMethod(this, [this, signalingFromProfile, stunUrl]() {
                        netRelayServer_ = displaySignalingInSettingsField(signalingFromProfile);
                        QSettings s("Morze", "MorzeGUI");
                        netBindAddress_ = s.value("ui/netBindAddress", "0.0.0.0").toString();
                        netStunServer_ = stunUrl;
                        if (!netStunServer_.isEmpty() && !stunServerOptions_.contains(netStunServer_)) {
                            stunServerOptions_.prepend(netStunServer_);
                            emit stunServerOptionsChanged();
                        }
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
                    item.roomId = QString::fromStdString(chat.getRoomIdAsString());
                    item.title = QString::fromStdString(chat.getTitle());
                    item.subtitle = QString::fromStdString(chat.getTypeAsString());
                    item.avatarHue = avatarHueForKey(chat.getIdAsString());
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
                    allChatItems_ = std::move(items);
                    applyChatFilter();
                    if (selectedChatIndex_ < 0 && chatModel_.rowCount() > 0) {
                        selectChatByIndex(0);
                    } else if (selectedChatIndex_ >= chatModel_.rowCount()) {
                        selectChatByIndex(chatModel_.rowCount() - 1);
                    } else if (selectedChatIndex_ >= 0) {
                        const auto *selected = chatModel_.itemAt(selectedChatIndex_);
                        if (selected) {
                            const QString newSelectedChatId = selected->chatId;
                            const QString newSelectedRoomId = selected->roomId;
                            const bool changed = (selectedChatId_ != newSelectedChatId) ||
                                                 (selectedRoomId_ != newSelectedRoomId);
                            selectedChatId_ = newSelectedChatId;
                            selectedRoomId_ = newSelectedRoomId;
                            if (changed) {
                                emit selectedChatIdChanged();
                            }
                            refreshMessages();
                            refreshParticipants();
                        }
                    } else {
                        messageModel_.setItems({});
                        participantModel_.setItems({});
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

void ClientBridge::applyChatFilter() {
    if (chatSearchQuery_.isEmpty()) {
        chatModel_.setItems(allChatItems_);
        return;
    }

    const QString needle = chatSearchQuery_.toLower();
    std::vector<ChatListModel::Item> filtered;
    filtered.reserve(allChatItems_.size());
    for (const auto &item : allChatItems_) {
        if (item.title.toLower().contains(needle) || item.lastText.toLower().contains(needle)) {
            filtered.push_back(item);
        }
    }
    chatModel_.setItems(std::move(filtered));
}

void ClientBridge::refreshMessages() {
    if (!messageService_ || !memberService_ || selectedChatId_.isEmpty()) {
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
                const auto members = co_await memberService_->getMembersByChatId(chatUuid);
                std::reverse(messages.begin(), messages.end());

                std::unordered_map<std::string, std::string> memberNamesById;
                memberNamesById.reserve(members.size());
                for (const auto &member : members) {
                    memberNamesById[member.getIdAsString()] = member.getUsername();
                }

                std::vector<MessageListModel::Item> items;
                items.reserve(messages.size());
                for (const auto &msg : messages) {
                    MessageListModel::Item item;
                    item.isOutgoing = (msg.getDirection() == MessageDirection::Outgoing);
                    item.text = QString::fromStdString(msg.getContent());
                    item.time = formatMessageTime(msg.getCreatedAt());
                    if (!item.isOutgoing) {
                        const std::string senderId = msg.getSenderIdAsString();
                        const auto it = memberNamesById.find(senderId);
                        item.name = (it == memberNamesById.end())
                            ? QString::fromStdString(senderId)
                            : QString::fromStdString(it->second);
                    }
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

void ClientBridge::refreshParticipants() {
    if (!memberService_ || !messageService_ || selectedChatId_.isEmpty()) {
        participantModel_.setItems({});
        return;
    }

    const std::string chatId = selectedChatId_.toStdString();
    boost::asio::co_spawn(
        ioc_,
        [this, chatId]() -> boost::asio::awaitable<void> {
            try {
                boost::uuids::string_generator parser;
                const boost::uuids::uuid chatUuid = parser(chatId);
                const auto membersInChat = co_await memberService_->getMembersByChatId(chatUuid);
                const auto allMembers = co_await memberService_->getAllMembers();
                const auto messages = co_await messageService_->getMessagesByChatId(chatUuid);

                std::unordered_map<std::string, ChatMemberModel> membersById;
                membersById.reserve(allMembers.size());
                for (const auto &member : allMembers) {
                    membersById.emplace(member.getIdAsString(), member);
                }

                std::unordered_set<std::string> participantIds;
                participantIds.reserve(membersInChat.size() + messages.size());
                for (const auto &member : membersInChat) {
                    participantIds.insert(member.getIdAsString());
                }
                for (const auto &msg : messages) {
                    const std::string senderId = msg.getSenderIdAsString();
                    if (!senderId.empty()) {
                        participantIds.insert(senderId);
                    }
                }

                std::string myId;
                for (const auto &msg : messages) {
                    if (msg.getDirection() == MessageDirection::Outgoing) {
                        myId = msg.getSenderIdAsString();
                        break;
                    }
                }

                std::vector<ParticipantListModel::Item> items;
                items.reserve(participantIds.size());
                for (const auto &participantId : participantIds) {
                    auto it = membersById.find(participantId);
                    ParticipantListModel::Item item;
                    if (it == membersById.end()) {
                        continue;
                    }
                    const auto &member = it->second;
                    QString displayName = QString::fromStdString(member.getUsername());
                    if (!myId.empty() && participantId == myId) {
                        const QString chatScopedNick = chatNicknames_.value(QString::fromStdString(chatId));
                        if (!chatScopedNick.isEmpty()) {
                            displayName = chatScopedNick;
                        }
                        displayName += " (you)";
                    }
                    item.name = displayName;
                    item.avatarLetter = avatarLetterForTitle(member.getUsername());
                    item.avatarHue = avatarHueForKey(member.getIdAsString());
                    items.push_back(std::move(item));
                }

                QMetaObject::invokeMethod(this, [this, items = std::move(items)]() mutable {
                    participantModel_.setItems(std::move(items));
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

int ClientBridge::avatarHueForKey(const std::string &key) const {
    const uint hashed = qHash(QString::fromStdString(key));
    return static_cast<int>(hashed % 360);
}

QString ClientBridge::avatarLetterForTitle(const std::string &title) const {
    const QString qTitle = QString::fromStdString(title).trimmed();
    if (qTitle.isEmpty()) {
        return QStringLiteral("?");
    }
    return qTitle.left(1).toUpper();
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

QString ClientBridge::resolveConfigJsonPath() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/config.json",
        appDir + "/../config.json",
        appDir + "/../../config.json",
        QDir::currentPath() + "/config.json",
        QDir::currentPath() + "/../config.json"
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

QString ClientBridge::readSignalingServerUrlFromConfigFile() const {
    const QString configPath = resolveConfigJsonPath();
    if (configPath.isEmpty()) {
        return {};
    }

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return {};
    }

    return doc.object().value("signaling").toObject().value("server_url").toString().trimmed();
}

void ClientBridge::loadDefaultNetworkState() {
    {
        QSettings s("Morze", "MorzeGUI");
        netBindAddress_ = s.value("ui/netBindAddress", "0.0.0.0").toString();
    }
    if (netRelayServer_.trimmed().isEmpty()) {
        const QString fromFile = readSignalingServerUrlFromConfigFile();
        if (!fromFile.isEmpty()) {
            netRelayServer_ = displaySignalingInSettingsField(fromFile);
            emit networkSettingsChanged();
        }
    }
}

QString ClientBridge::resolveSignalingServerUrl() const {
    if (!netRelayServer_.trimmed().isEmpty()) {
        return normalizeSignalingForConnect(netRelayServer_);
    }
    return normalizeSignalingForConnect(readSignalingServerUrlFromConfigFile());
}

bool ClientBridge::ensureSignalingConnected() {
    if (!signalingService_) {
        return false;
    }
    if (signalingService_->isConnected()) {
        return true;
    }

    const QString serverUrl = resolveSignalingServerUrl();
    if (serverUrl.isEmpty()) {
        qWarning().noquote() << "[GUI] signaling connect failed: empty server url";
        emit errorOccurred("Укажите адрес сигнального сервера (нижняя строка в настройках, «Signaling / TURN») или signaling.server_url в config.json");
        return false;
    }
    qInfo().noquote() << "[GUI] signaling connectToServer:" << serverUrl;
    signalingService_->connectToServer(QUrl(serverUrl));
    return true;
}

void ClientBridge::handleSignalingJoined(const QString &roomId, const QString &roomType) {
    if (pendingJoinRoomId_.isEmpty() || roomId != pendingJoinRoomId_) {
        qWarning().noquote() << "[GUI] ignore joined event. pendingRoomId=" << pendingJoinRoomId_
                             << " incomingRoomId=" << roomId;
        return;
    }

    const QString nickname = pendingJoinNickname_;
    QString title = pendingJoinTitle_;
    const QString mode = pendingJoinMode_;

    pendingJoinRoomId_.clear();
    pendingJoinNickname_.clear();
    pendingJoinTitle_.clear();
    pendingJoinMode_.clear();
    pendingJoinRoomType_.clear();

    if (title.isEmpty()) {
        title = mode == "create" ? QString("Новый чат %1").arg(roomId.left(6))
                                 : QString("Чат %1").arg(roomId.left(6));
    }

    boost::asio::co_spawn(
        ioc_,
        [this, roomId, roomType, nickname, title]() -> boost::asio::awaitable<void> {
            try {
                boost::uuids::string_generator parser;
                const auto roomUuid = parser(roomId.toStdString());
                const ChatType type = (roomType.compare("group", Qt::CaseInsensitive) == 0)
                                          ? ChatType::Group
                                          : ChatType::Direct;

                const auto chats = co_await chatService_->getAllChats();
                auto chatIt = std::find_if(chats.begin(), chats.end(), [&](const ChatModel &chat) {
                    return chat.getRoomId() == roomUuid;
                });

                ChatModel chatModel;
                if (chatIt == chats.end()) {
                    chatModel = ChatModel(roomUuid, type, title.toStdString(), std::chrono::system_clock::now(), 0);
                    co_await chatService_->addChat(chatModel);
                } else {
                    chatModel = *chatIt;
                }

                ChatMemberModel member(nickname.toStdString(), std::chrono::system_clock::now());
                co_await memberService_->addMember(member);
                try {
                    co_await chatService_->addMemberToChat(chatModel.getId(), member.getId());
                } catch (...) {
                }

                const QString chatId = QString::fromStdString(chatModel.getIdAsString());
                QMetaObject::invokeMethod(this, [this, chatId, nickname]() {
                    setChatNickname(chatId, nickname);
                    refreshChatData();
                }, Qt::QueuedConnection);
            } catch (const std::exception &e) {
                QMetaObject::invokeMethod(this, [this, message = QString::fromUtf8(e.what())]() {
                    emit errorOccurred(message);
                }, Qt::QueuedConnection);
            }
        },
        boost::asio::detached);
}

void ClientBridge::loadStunServersFromConfig() {
    QStringList loaded;
    const QString configPath = resolveConfigJsonPath();
    if (!configPath.isEmpty()) {
        QFile file(configPath);
        if (file.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject()) {
                const auto signalingObj = doc.object().value("signaling").toObject();
                const auto stunUrls = signalingObj.value("stun_urls").toArray();
                for (const auto &value : stunUrls) {
                    const QString item = stripStunPrefix(value.toString().trimmed());
                    if (!item.isEmpty() && !loaded.contains(item)) {
                        loaded.push_back(item);
                    }
                }
                const QString stunUrl = stripStunPrefix(signalingObj.value("stun_url").toString().trimmed());
                if (!stunUrl.isEmpty() && !loaded.contains(stunUrl)) {
                    loaded.push_front(stunUrl);
                }
            }
        }
    }

    if (loaded.isEmpty()) {
        loaded = {"stun.l.google.com:19302", "global.stun.twilio.com:3478"};
    }
    stunServerOptions_ = loaded;
    emit stunServerOptionsChanged();
}

void ClientBridge::loadChatNicknames() {
    QSettings settings("Morze", "MorzeGUI");
    const int size = settings.beginReadArray("chatNicknames");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        const QString chatId = settings.value("chatId").toString().trimmed();
        const QString nickname = settings.value("nickname").toString().trimmed();
        if (!chatId.isEmpty() && !nickname.isEmpty()) {
            chatNicknames_.insert(chatId, nickname);
        }
    }
    settings.endArray();
}

void ClientBridge::persistChatNicknames() const {
    QSettings settings("Morze", "MorzeGUI");
    settings.beginWriteArray("chatNicknames");
    int index = 0;
    for (auto it = chatNicknames_.cbegin(); it != chatNicknames_.cend(); ++it) {
        settings.setArrayIndex(index++);
        settings.setValue("chatId", it.key());
        settings.setValue("nickname", it.value());
    }
    settings.endArray();
}

bool ClientBridge::parseEndpoint(const QString &value, QString *host, quint16 *port, QString *error) const {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        *error = "Ошибка: пустой адрес";
        return false;
    }

    QUrl url(trimmed);
    if (url.isValid() && !url.scheme().isEmpty() && !url.host().isEmpty()) {
        *host = url.host();
        *port = static_cast<quint16>(url.port(3478));
        return true;
    }

    const int sep = trimmed.lastIndexOf(':');
    if (sep <= 0 || sep == trimmed.size() - 1) {
        *error = "Ошибка: формат host:port";
        return false;
    }

    *host = trimmed.left(sep).trimmed();
    bool okPort = false;
    const int intPort = trimmed.mid(sep + 1).toInt(&okPort);
    if (!okPort || intPort <= 0 || intPort > 65535 || host->isEmpty()) {
        *error = "Ошибка: неверный порт";
        return false;
    }
    *port = static_cast<quint16>(intPort);
    return true;
}

QString ClientBridge::stripStunPrefix(const QString &value) const {
    QString trimmed = value.trimmed();
    if (trimmed.startsWith("stun://", Qt::CaseInsensitive)) {
        trimmed = trimmed.mid(7);
    } else if (trimmed.startsWith("stun:", Qt::CaseInsensitive)) {
        trimmed = trimmed.mid(5);
    }
    return trimmed.trimmed();
}

QString ClientBridge::ensureStunPrefix(const QString &value) const {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    if (trimmed.startsWith("stun://", Qt::CaseInsensitive) || trimmed.startsWith("stun:", Qt::CaseInsensitive)) {
        return trimmed;
    }
    return QString("stun:%1").arg(trimmed);
}

QString ClientBridge::requestStunMappedAddress(const QString &host, quint16 port, QString *error, qint64 *elapsedMs) const {
    QElapsedTimer timer;
    timer.start();

    const QHostInfo hostInfo = QHostInfo::fromName(host);
    if (hostInfo.error() != QHostInfo::NoError || hostInfo.addresses().empty()) {
        *error = "Ошибка DNS";
        return {};
    }

    QUdpSocket socket;
    if (!socket.bind(QHostAddress::AnyIPv4, 0)) {
        *error = "Ошибка UDP";
        return {};
    }

    QByteArray req(20, '\0');
    req[0] = 0x00;
    req[1] = 0x01; // Binding Request
    req[2] = 0x00;
    req[3] = 0x00; // no attrs
    req[4] = 0x21;
    req[5] = 0x12;
    req[6] = static_cast<char>(0xA4);
    req[7] = 0x42;
    for (int i = 8; i < 20; ++i) {
        req[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
    }

    const QHostAddress target = hostInfo.addresses().front();
    if (socket.writeDatagram(req, target, port) < 0) {
        *error = "Нет UDP";
        return {};
    }
    if (!socket.waitForReadyRead(1800)) {
        *error = "Таймаут STUN";
        return {};
    }

    QByteArray resp;
    resp.resize(static_cast<int>(socket.pendingDatagramSize()));
    if (socket.readDatagram(resp.data(), resp.size()) <= 0 || resp.size() < 20) {
        *error = "Пустой ответ";
        return {};
    }

    auto u16 = [&](int pos) -> quint16 {
        const auto *p = reinterpret_cast<const unsigned char *>(resp.constData() + pos);
        return static_cast<quint16>((p[0] << 8) | p[1]);
    };
    auto u32 = [&](int pos) -> quint32 {
        const auto *p = reinterpret_cast<const unsigned char *>(resp.constData() + pos);
        return (static_cast<quint32>(p[0]) << 24) | (static_cast<quint32>(p[1]) << 16) |
               (static_cast<quint32>(p[2]) << 8) | static_cast<quint32>(p[3]);
    };

    const quint16 msgType = u16(0);
    if (msgType != 0x0101) {
        *error = "Неверный ответ";
        return {};
    }

    const quint32 cookie = u32(4);
    int pos = 20;
    while (pos + 4 <= resp.size()) {
        const quint16 attrType = u16(pos);
        const quint16 attrLen = u16(pos + 2);
        pos += 4;
        if (pos + attrLen > resp.size()) {
            break;
        }

        if ((attrType == 0x0020 || attrType == 0x0001) && attrLen >= 8) {
            const auto *v = reinterpret_cast<const unsigned char *>(resp.constData() + pos);
            const quint8 family = v[1];
            if (family == 0x01) {
                quint16 mappedPort = static_cast<quint16>((v[2] << 8) | v[3]);
                quint32 mappedAddr = (static_cast<quint32>(v[4]) << 24) | (static_cast<quint32>(v[5]) << 16) |
                                     (static_cast<quint32>(v[6]) << 8) | static_cast<quint32>(v[7]);
                if (attrType == 0x0020) {
                    mappedPort ^= static_cast<quint16>(cookie >> 16);
                    mappedAddr ^= cookie;
                }
                const QString ip = QHostAddress(mappedAddr).toString();
                if (!ip.isEmpty()) {
                    *elapsedMs = timer.elapsed();
                    return QString("%1:%2").arg(ip).arg(mappedPort);
                }
            }
        }

        pos += attrLen;
        const int pad = (4 - (attrLen % 4)) % 4;
        pos += pad;
    }

    *error = "IP не получен";
    return {};
}

QString ClientBridge::extractHostFromStunValue(const QString &value) const {
    QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (!trimmed.contains("://") && trimmed.startsWith("stun:")) {
        trimmed = "stun://" + trimmed.mid(5);
    }

    QUrl url(trimmed);
    if (url.isValid() && !url.host().isEmpty()) {
        return url.host().trimmed();
    }

    if (trimmed.startsWith("stun:")) {
        trimmed = trimmed.mid(5);
    }
    const int sep = trimmed.indexOf(':');
    if (sep > 0) {
        trimmed = trimmed.left(sep);
    }
    return trimmed.trimmed();
}

void ClientBridge::setBindCheckResult(bool ok, const QString &text) {
    bindCheckOk_ = ok;
    bindCheckText_ = text;
    emit connectionChecksChanged();
}

void ClientBridge::setStunCheckResult(bool ok, const QString &text) {
    stunCheckOk_ = ok;
    stunCheckText_ = text;
    emit connectionChecksChanged();
}

void ClientBridge::setRelayCheckResult(bool ok, const QString &text) {
    relayCheckOk_ = ok;
    relayCheckText_ = text;
    emit connectionChecksChanged();
}

void ClientBridge::handleIncomingMessage(const QString &roomId, const QString &senderPeerId, const QString &text) {
    if (text.isEmpty()) return;

    boost::asio::co_spawn(
        ioc_,
        [this, roomId, senderPeerId, text]() -> boost::asio::awaitable<void> {
            try {
                boost::uuids::string_generator parser;
                const auto roomUuid = parser(roomId.toStdString());
                const auto chats = co_await chatService_->getAllChats();
                auto chatIt = std::find_if(chats.begin(), chats.end(), [&](const ChatModel &c) {
                    return c.getRoomId() == roomUuid;
                });
                if (chatIt == chats.end()) co_return;

                // Find or create member for sender
                const auto members = co_await memberService_->getMembersByChatId(chatIt->getId());
                boost::uuids::uuid senderMemberId;
                bool found = false;

                // Try to match by username from peerUsernames_
                const QString key = roomId + ":" + senderPeerId;
                const QString peerName = peerUsernames_.value(key, senderPeerId);
                for (const auto &m : members) {
                    if (m.getUsername() == peerName.toStdString()) {
                        senderMemberId = m.getId();
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    ChatMemberModel peerMember(peerName.toStdString(), std::chrono::system_clock::now());
                    co_await memberService_->addMember(peerMember);
                    try { co_await chatService_->addMemberToChat(chatIt->getId(), peerMember.getId()); } catch (...) {}
                    senderMemberId = peerMember.getId();
                }

                MessageModel message(
                    chatIt->getId(), senderMemberId,
                    MessageDirection::Incoming, text.toStdString(),
                    std::chrono::system_clock::now(), DeliveryState::Delivered);
                co_await messageService_->addMessage(message);

                QMetaObject::invokeMethod(this, [this]() {
                    refreshMessages();
                    refreshChats();
                }, Qt::QueuedConnection);
            } catch (const std::exception &e) {
                QMetaObject::invokeMethod(this, [this, msg = QString::fromUtf8(e.what())]() {
                    emit errorOccurred(msg);
                }, Qt::QueuedConnection);
            }
        },
        boost::asio::detached);
}

void ClientBridge::flushPendingDirectMessages(const QString &peerId) {
    auto it = pendingDirectMessages_.find(peerId);
    if (it == pendingDirectMessages_.end() || it->isEmpty()) return;
    if (!webRTCService_ || !webRTCService_->isDataChannelOpen(peerId)) return;

    for (const auto &data : *it)
        webRTCService_->sendMessage(peerId, data);
    it->clear();
}

