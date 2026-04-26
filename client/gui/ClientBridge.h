#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>

#include "ChatService.h"
#include "ChatRepository.h"
#include "ChatMemberRepository.h"
#include "ChatMemberService.h"
#include "ConnectionProfileService.h"
#include "ConnectionProfileRepository.h"
#include "Database.h"
#include "MessageService.h"
#include "MessageRepository.h"
#include "SignalingService.h"
#include "WebRTCService.h"

class ChatListModel final : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        ChatIdRole = Qt::UserRole + 1,
        TitleRole,
        SubtitleRole,
        LastTextRole,
        TimeRole,
        UnreadRole,
        AvatarLetterRole,
        AvatarHueRole
    };

    struct Item {
        QString chatId;
        QString roomId;
        QString title;
        QString subtitle;
        QString lastText;
        QString time;
        int unread = 0;
        QString avatarLetter;
        int avatarHue = 210;
    };

    explicit ChatListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<Item> items);
    const Item *itemAt(int row) const;

private:
    std::vector<Item> items_;
};

class MessageListModel final : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        IsOutgoingRole = Qt::UserRole + 1,
        NameRole,
        TextRole,
        TimeRole
    };

    struct Item {
        bool isOutgoing = false;
        QString name;
        QString text;
        QString time;
    };

    explicit MessageListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<Item> items);

private:
    std::vector<Item> items_;
};

class ParticipantListModel final : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        AvatarLetterRole,
        AvatarHueRole
    };

    struct Item {
        QString name;
        QString avatarLetter;
        int avatarHue = 210;
    };

    explicit ParticipantListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<Item> items);

private:
    std::vector<Item> items_;
};

class ClientBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *chatModel READ chatModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *messageModel READ messageModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *participantModel READ participantModel CONSTANT)
    Q_PROPERTY(QString selectedChatId READ selectedChatId NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString selectedRoomId READ selectedRoomId NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString selectedTitle READ selectedTitle NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString selectedSubtitle READ selectedSubtitle NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString selectedAvatarLetter READ selectedAvatarLetter NOTIFY selectedChatIdChanged)
    Q_PROPERTY(int selectedAvatarHue READ selectedAvatarHue NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString netBindAddress READ netBindAddress NOTIFY networkSettingsChanged)
    Q_PROPERTY(QString netStunServer READ netStunServer NOTIFY networkSettingsChanged)
    Q_PROPERTY(QString netRelayServer READ netRelayServer NOTIFY networkSettingsChanged)
    Q_PROPERTY(QString chatSearchQuery READ chatSearchQuery NOTIFY chatSearchQueryChanged)
    Q_PROPERTY(QStringList stunServerOptions READ stunServerOptions NOTIFY stunServerOptionsChanged)
    Q_PROPERTY(QString bindCheckText READ bindCheckText NOTIFY connectionChecksChanged)
    Q_PROPERTY(bool bindCheckOk READ bindCheckOk NOTIFY connectionChecksChanged)
    Q_PROPERTY(QString stunCheckText READ stunCheckText NOTIFY connectionChecksChanged)
    Q_PROPERTY(bool stunCheckOk READ stunCheckOk NOTIFY connectionChecksChanged)
    Q_PROPERTY(QString relayCheckText READ relayCheckText NOTIFY connectionChecksChanged)
    Q_PROPERTY(bool relayCheckOk READ relayCheckOk NOTIFY connectionChecksChanged)
public:
    explicit ClientBridge(QObject *parent = nullptr);
    ~ClientBridge() override;

    QAbstractItemModel *chatModel();
    QAbstractItemModel *messageModel();
    QAbstractItemModel *participantModel();

    QString selectedChatId() const;
    QString selectedRoomId() const;
    QString selectedTitle() const;
    QString selectedSubtitle() const;
    QString selectedAvatarLetter() const;
    int selectedAvatarHue() const;

    QString netBindAddress() const;
    QString netStunServer() const;
    QString netRelayServer() const;
    QString chatSearchQuery() const;
    QStringList stunServerOptions() const;
    QString bindCheckText() const;
    bool bindCheckOk() const;
    QString stunCheckText() const;
    bool stunCheckOk() const;
    QString relayCheckText() const;
    bool relayCheckOk() const;

    Q_INVOKABLE void refreshAll();
    Q_INVOKABLE void refreshChatData();
    Q_INVOKABLE void setChatSearchQuery(const QString &query);
    Q_INVOKABLE void selectChatByIndex(int index);
    Q_INVOKABLE void sendMessage(const QString &text);
    Q_INVOKABLE void updateNetworkSettings(const QString &bindAddress, const QString &stunServer, const QString &relayServer);
    /// roomType: "direct" (личный) или "group" (групповой)
    Q_INVOKABLE bool createNewChat(const QString &nickname, const QString &title, const QString &roomType);
    Q_INVOKABLE bool joinChatById(const QString &chatOrRoomId, const QString &nickname, const QString &title);
    /// Удаляет чат с устройства (БД) и снимает выбор.
    Q_INVOKABLE void removeCurrentChat();
    Q_INVOKABLE void setChatNickname(const QString &chatId, const QString &nickname);
    Q_INVOKABLE QString chatNickname(const QString &chatId) const;
    Q_INVOKABLE void copyToClipboard(const QString &text) const;
    Q_INVOKABLE void refreshBindAddress();
    Q_INVOKABLE void resolvePublicIpViaStun();
    Q_INVOKABLE void checkRelayConnection();

signals:
    void selectedChatIdChanged();
    void networkSettingsChanged();
    void chatSearchQueryChanged();
    void stunServerOptionsChanged();
    void connectionChecksChanged();
    void errorOccurred(const QString &message);

private:
    void refreshProfiles();
    void refreshChats();
    void applyChatFilter();
    void refreshMessages();
    void refreshParticipants();
    QString formatMessageTime(const std::chrono::system_clock::time_point &tp) const;
    int avatarHueForKey(const std::string &key) const;
    QString avatarLetterForTitle(const std::string &title) const;
    std::string resolveDatabasePath() const;
    QString resolveConfigJsonPath() const;
    void loadStunServersFromConfig();
    /// IP bind + выбранный до первого refreshProfiles пустой релей: из config.json
    void loadDefaultNetworkState();
    QString readSignalingServerUrlFromConfigFile() const;
    void loadChatNicknames();
    void persistChatNicknames() const;
    bool parseEndpoint(const QString &value, QString *host, quint16 *port, QString *error) const;
    QString resolveSignalingServerUrl() const;
    bool ensureSignalingConnected();
    void handleSignalingJoined(const QString &roomId, const QString &roomType);
    void handleIncomingMessage(const QString &roomId, const QString &senderPeerId, const QString &text);
    void flushPendingDirectMessages(const QString &peerId);
    QString requestStunMappedAddress(const QString &host, quint16 port, QString *error, qint64 *elapsedMs) const;
    QString extractHostFromStunValue(const QString &value) const;
    QString stripStunPrefix(const QString &value) const;
    QString ensureStunPrefix(const QString &value) const;
    void setBindCheckResult(bool ok, const QString &text);
    void setStunCheckResult(bool ok, const QString &text);
    void setRelayCheckResult(bool ok, const QString &text);

    boost::asio::io_context ioc_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> workGuard_;
    std::thread ioThread_;
    std::shared_ptr<Database> database_;
    std::shared_ptr<ChatRepository> chatRepository_;
    std::shared_ptr<MessageRepository> messageRepository_;
    std::shared_ptr<ConnectionProfileRepository> profileRepository_;
    std::shared_ptr<ChatMemberRepository> memberRepository_;
    std::shared_ptr<ChatService> chatService_;
    std::shared_ptr<ChatMemberService> memberService_;
    std::shared_ptr<MessageService> messageService_;
    std::shared_ptr<ConnectionProfileService> profileService_;
    std::shared_ptr<SignalingService> signalingService_;
    ChatListModel chatModel_;
    std::vector<ChatListModel::Item> allChatItems_;
    MessageListModel messageModel_;
    ParticipantListModel participantModel_;
    QString selectedChatId_;
    QString selectedRoomId_;
    int selectedChatIndex_ = -1;
    QString netBindAddress_ = "0.0.0.0";
    QString netStunServer_ = "stun:stun.l.google.com:19302";
    QString netRelayServer_;
    QString chatSearchQuery_;
    QHash<QString, QString> chatNicknames_;
    QStringList stunServerOptions_;
    QString bindCheckText_;
    QString stunCheckText_;
    QString relayCheckText_;
    bool bindCheckOk_ = false;
    bool stunCheckOk_ = false;
    bool relayCheckOk_ = false;
    QString pendingJoinRoomId_;
    QString pendingJoinNickname_;
    QString pendingJoinTitle_;
    QString pendingJoinMode_;
    QString pendingJoinRoomType_;
    /// own peerId from signaling "joined" per roomId (for leave)
    QHash<QString, QString> myPeerIdByRoom_;
    std::shared_ptr<WebRTCService> webRTCService_;
    QHash<QString, QString> roomTypeByRoom_;        // roomId → "direct"/"group"
    QHash<QString, QString> peerToRoom_;             // peerId → roomId
    QHash<QString, QString> peerUsernames_;           // "roomId:peerId" → username
    QHash<QString, QList<QByteArray>> pendingDirectMessages_; // peerId → queued msgs
};

