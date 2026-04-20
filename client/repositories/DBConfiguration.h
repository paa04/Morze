#ifndef MORZE_DBCONFIGURATION_H
#define MORZE_DBCONFIGURATION_H

#include <string>
#include <sqlite_orm/sqlite_orm.h>

#include "ChatDAO.h"
#include "ChatMemberDAO.h"
#include "ConnectionProfileDAO.h"
#include "MessageDAO.h"
#include "UserDAO.h"

class DBConfiguration {
public:
    static auto createStorage(const std::string &dbPath) {
        using namespace sqlite_orm;
        return make_storage(dbPath,
                            make_table("chats",
                                       make_column("chat_id", &ChatDAO::getChatIdAsBLOB, &ChatDAO::setChatIdFromBLOB,
                                                   primary_key()),
                                       make_column("room_id", &ChatDAO::getRoomIdAsBLOB, &ChatDAO::setRoomIdFromBLOB),
                                       make_column("type", &ChatDAO::getTypeAsString, &ChatDAO::setTypeFromString),
                                       make_column("title", &ChatDAO::getTitle, &ChatDAO::setTitle),
                                       make_column("created_at", &ChatDAO::getCreatedAtAsUnix,
                                                   &ChatDAO::setCreatedAtFromUnixTime),
                                       make_column("last_message_number", &ChatDAO::getLastMessageNumber,
                                                   &ChatDAO::setLastMessageNumber)
                            ),
                            make_table("chat_members",
                                       make_column("id", &ChatMemberDAO::getIdAsBLOB, &ChatMemberDAO::setIdFromBLOB,
                                                   primary_key()),
                                       make_column("chat_id", &ChatMemberDAO::getChatIdAsBLOB,
                                                   &ChatMemberDAO::setChatIdFromBLOB),
                                       make_column("username", &ChatMemberDAO::getUsername,
                                                   &ChatMemberDAO::setUsername),
                                       make_column("last_online_at", &ChatMemberDAO::getLastOnlineAtAsUnix,
                                                   &ChatMemberDAO::setLastOnlineAtFromUnix)
                            ),
                            make_table("messages",
                                       make_column("message_id", &MessageDAO::getMessageIdAsBLOB,
                                                   &MessageDAO::setMessageIdFromBLOB, primary_key()),
                                       make_column("chat_id", &MessageDAO::getChatIdAsBLOB,
                                                   &MessageDAO::setChatIdFromBLOB),
                                       make_column("sender_id", &MessageDAO::getSenderIdAsBLOB,
                                                   &MessageDAO::setSenderIdFromBLOB),
                                       make_column("direction", &MessageDAO::getDirectionAsString,
                                                   &MessageDAO::setDirectionFromString),
                                       make_column("content", &MessageDAO::getContent, &MessageDAO::setContent),
                                       make_column("created_at", &MessageDAO::getCreatedAtAsUnixTime,
                                                   &MessageDAO::setCreatedAtFromUnixTime),
                                       make_column("delivery_state", &MessageDAO::getDeliveryStateAsString,
                                                   &MessageDAO::setDeliveryStateFromString)
                            ),
                            make_table("connection_profiles",
                                       make_column("id", &ConnectionProfileDAO::getIdAsBLOB,
                                                   &ConnectionProfileDAO::setIdFromBLOB, primary_key()),
                                       make_column("server_url", &ConnectionProfileDAO::getServerUrl,
                                                   &ConnectionProfileDAO::setServerUrl),
                                       make_column("stun_url", &ConnectionProfileDAO::getStunUrl,
                                                   &ConnectionProfileDAO::setStunUrl),
                                       make_column("updated_at", &ConnectionProfileDAO::getUpdatedAtAsUnix,
                                                   &ConnectionProfileDAO::setUpdatedAtFromUnix)
                            ),

                            make_table("users",
                                       make_column("login", &UserDAO::getLogin, &UserDAO::setLogin, primary_key()),
                                       make_column("password_hash", &UserDAO::getPasswordHash,
                                                   &UserDAO::setPasswordHash),
                                       make_column("member_id", &UserDAO::getMemberIdAsBLOB,
                                                   &UserDAO::setMemberIdFromBLOB),
                                       make_column("created_at", &UserDAO::getCreatedAtAsUnix,
                                                   &UserDAO::setCreatedAtFromUnix),
                                       make_column("updated_at", &UserDAO::getUpdatedAtAsUnix,
                                                   &UserDAO::setUpdatedAtFromUnix),
                                       foreign_key(&UserDAO::getMemberId).references(&ChatMemberDAO::getId)
                            )
        );
    }
};

#endif //MORZE_DBCONFIGURATION_H
