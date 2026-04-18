//
// Created by ivan on 17.04.2026.
//

#ifndef MORZE_DBCONFIGURATION_H
#define MORZE_DBCONFIGURATION_H

#include <string>
#include <sqlite_orm/sqlite_orm.h>

#include "ChatDAO.h"
#include "ChatMemberDAO.h"
#include "ConnectionProfileDAO.h"
#include "MessageDAO.h"


class DBConfiguration {
public:
    static auto createStorage(const std::string& dbPath) {
    using namespace sqlite_orm;
    return make_storage(dbPath,
        make_table("chats",
            make_column("chat_id", &ChatDAO::getChatId, &ChatDAO::setChatId, primary_key()),
            make_column("room_id", &ChatDAO::getRoomId, &ChatDAO::setRoomId),
            make_column("type", &ChatDAO::getTypeAsString, &ChatDAO::setTypeFromString),
            make_column("title", &ChatDAO::getTitle, &ChatDAO::setTitle),
            make_column("created_at", &ChatDAO::getCreatedAt, &ChatDAO::setCreatedAt)
        ),
        make_table("chat_members",
            make_column("id", &ChatMemberDAO::getId, &ChatMemberDAO::setId),
            make_column("chat_id", &ChatMemberDAO::getChatId, &ChatMemberDAO::setChatId),
            make_column("member_id", &ChatMemberDAO::getMemberId, &ChatMemberDAO::setMemberId),
            make_column("peer_id", &ChatMemberDAO::getPeerId, &ChatMemberDAO::setPeerId),
            make_column("username", &ChatMemberDAO::getUsername, &ChatMemberDAO::setUsername),
            make_column("last_online_at", &ChatMemberDAO::getLastOnlineAt, &ChatMemberDAO::setLastOnlineAt)
        ),
        make_table("messages",
            make_column("message_id", &MessageDAO::getMessageId, &MessageDAO::setMessageId),
            make_column("chat_id", &MessageDAO::getChatId, &MessageDAO::setChatId),
            make_column("sender_name", &MessageDAO::getSenderName, &MessageDAO::setSenderName),
            make_column("direction", &MessageDAO::getDirectionAsString, &MessageDAO::setDirectionFromString),
            make_column("content", &MessageDAO::getContent, &MessageDAO::setContent),
            make_column("created_at", &MessageDAO::getCreatedAt, &MessageDAO::setCreatedAt),
            make_column("delivery_state", &MessageDAO::getDeliveryStateAsString, &MessageDAO::setDeliveryStateFromString)
        ),
        make_table("connection_profiles",
            make_column("id", &ConnectionProfileDAO::getId, &ConnectionProfileDAO::setId),
            make_column("server_url", &ConnectionProfileDAO::getServerUrl, &ConnectionProfileDAO::setServerUrl),
            make_column("stun_url", &ConnectionProfileDAO::getStunUrl, &ConnectionProfileDAO::setStunUrl),
            make_column("updated_at", &ConnectionProfileDAO::getUpdatedAt, &ConnectionProfileDAO::setUpdatedAt)
        )
    );
}
};


#endif //MORZE_DBCONFIGURATION_H