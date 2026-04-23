//
// Created by ivan on 18.04.2026.
//

#ifndef MORZE_CONNECTIONPROFILEDAOCONVERTER_H
#define MORZE_CONNECTIONPROFILEDAOCONVERTER_H
#include "ConnectionProfileDAO.h"
#include "ConnectionProfileModel.h"


class ConnectionProfileDAOConverter {
public:
    static ConnectionProfileModel convert(const ConnectionProfileDAO &dao) {
        return ConnectionProfileModel{
            dao.getId(),
            dao.getServerUrl(),
            dao.getStunUrl(),
            dao.getUpdatedAt()
        };
    }

    static ConnectionProfileDAO convert(const ConnectionProfileModel &model) {
        return ConnectionProfileDAO{
            model.getId(),
            model.getServerUrl(),
            model.getStunUrl(),
            model.getUpdatedAt()
        };
    }
};


#endif //MORZE_CONNECTIONPROFILEDAOCONVERTER_H
