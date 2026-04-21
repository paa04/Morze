#ifndef MORZE_CHATMEMBERDAOCONVERTER_H
#define MORZE_CHATMEMBERDAOCONVERTER_H

#include "ChatMemberDAO.h"
#include "ChatMemberModel.h"

class ChatMemberDAOConverter {
public:
    static ChatMemberModel convert(const ChatMemberDAO &dao) {
        return ChatMemberModel{
            dao.getId(),
            dao.getUsername(),
            dao.getLastOnlineAt()
        };
    }

    static ChatMemberDAO convert(const ChatMemberModel &model) {
        return ChatMemberDAO{
            model.getId(),
            model.getUsername(),
            model.getLastOnlineAt()
        };
    }
};

#endif //MORZE_CHATMEMBERDAOCONVERTER_H