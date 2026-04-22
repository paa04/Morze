//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_MESSAGEDAOCONVERTER_H
#define MORZE_MESSAGEDAOCONVERTER_H
#include "MessageDAO.h"
#include "MessageModel.h"


class MessageDAOConverter {
    public:
    static MessageModel convert(const MessageDAO &dao) {
        return MessageModel{dao.getId(),
            dao.getChatId(),
            dao.getSenderId(),
            dao.getDirection(),
            dao.getContent(),
            dao.getCreatedAt(),
            dao.getDeliveryState()
        };
    }

    static MessageDAO convert(const MessageModel &model) {
        return MessageDAO{model.getMessageId(),
            model.getChatId(),
            model.getSenderId(),
            model.getDirection(),
            model.getContent(),
            model.getCreatedAt(),
            model.getDeliveryState()
        };
    }

};


#endif //MORZE_MESSAGEDAOCONVERTER_H