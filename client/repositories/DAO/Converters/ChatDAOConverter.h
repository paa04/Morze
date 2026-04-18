//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_CHATDAOCONVERTER_H
#define MORZE_CHATDAOCONVERTER_H
#include "ChatDAO.h"
#include "ChatModel.h"


class ChatDAOConverter {
public:
    static ChatModel convert(const ChatDAO &dao) {
        return ChatModel{
            dao.getChatId(),
            dao.getRoomId(),
            dao.getType(),
            dao.getTitle(),
            dao.getCreatedAt(),
            dao.getLastMessageNumber()
        };
    }

    static ChatDAO convert(const ChatModel &model) {
        return ChatDAO{
            model.getChatId(),
            model.getRoomId(),
            model.getType(),
            model.getTitle(),
            model.getCreatedAt(),
            model.getLastMessageNumber()
        };
    }
};


#endif //MORZE_CHATDAOCONVERTER_H
