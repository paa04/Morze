//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_CHATMEMBERDAOCONVERTER_H
#define MORZE_CHATMEMBERDAOCONVERTER_H
#include "ChatMemberDAO.h"
#include "ChatMemberModel.h"


class ChatMemberDAOConverter {
public:
    static ChatMemberModel convert(const ChatMemberDAO &dao) {
        return ChatMemberModel{
            dao.getId(),
            dao.getChatId(),
            dao.getMemberId(),
            dao.getPeerId(),
            dao.getUsername(),
            dao.getLastOnlineAt()
        };
    }

    static ChatMemberDAO convert(const ChatMemberModel &model) {
        return ChatMemberDAO{
            model.getId(),
            model.getChatId(),
            model.getMemberId(),
            model.getPeerId(),
            model.getUsername(),
            model.getLastOnlineAt()
        };
    }
};


#endif //MORZE_CHATMEMBERDAOCONVERTER_H
