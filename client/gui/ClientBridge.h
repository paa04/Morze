#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QString>
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

class ClientBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *chatModel READ chatModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *messageModel READ messageModel CONSTANT)
    Q_PROPERTY(QString selectedChatId READ selectedChatId NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString selectedTitle READ selectedTitle NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString selectedSubtitle READ selectedSubtitle NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString selectedAvatarLetter READ selectedAvatarLetter NOTIFY selectedChatIdChanged)
    Q_PROPERTY(int selectedAvatarHue READ selectedAvatarHue NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString netBindAddress READ netBindAddress NOTIFY networkSettingsChanged)
    Q_PROPERTY(QString netStunServer READ netStunServer NOTIFY networkSettingsChanged)
    Q_PROPERTY(QString netRelayServer READ netRelayServer NOTIFY networkSettingsChanged)
public:
    explicit ClientBridge(QObject *parent = nullptr);
    ~ClientBridge() override;

    QAbstractItemModel *chatModel();
    QAbstractItemModel *messageModel();

    QString selectedChatId() const;
    QString selectedTitle() const;
    QString selectedSubtitle() const;
    QString selectedAvatarLetter() const;
    int selectedAvatarHue() const;

    QString netBindAddress() const;
    QString netStunServer() const;
    QString netRelayServer() const;

    Q_INVOKABLE void refreshAll();
    Q_INVOKABLE void selectChatByIndex(int index);
    Q_INVOKABLE void sendMessage(const QString &text);
    Q_INVOKABLE void updateNetworkSettings(const QString &bindAddress, const QString &stunServer, const QString &relayServer);

signals:
    void selectedChatIdChanged();
    void networkSettingsChanged();
    void errorOccurred(const QString &message);

private:
    void refreshProfiles();
    void refreshChats();
    void refreshMessages();
    QString formatMessageTime(const std::chrono::system_clock::time_point &tp) const;
    int avatarHueForTitle(const std::string &title) const;
    QString avatarLetterForTitle(const std::string &title) const;
    std::string resolveDatabasePath() const;

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
    ChatListModel chatModel_;
    MessageListModel messageModel_;
    QString selectedChatId_;
    int selectedChatIndex_ = -1;
    QString netBindAddress_ = "0.0.0.0";
    QString netStunServer_ = "stun:stun.l.google.com:19302";
    QString netRelayServer_;
};

