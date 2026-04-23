#ifndef MORZE_USERDAOCONVERTER_H
#define MORZE_USERDAOCONVERTER_H

#include "UserDAO.h"
#include "UserModel.h"

class UserDAOConverter {
public:
    static UserModel convert(const UserDAO& dao) {
        return UserModel{
            dao.getLogin(),
            dao.getPasswordHash(),
            dao.getMemberId(),
            dao.getCreatedAt(),
            dao.getUpdatedAt()
        };
    }

    static UserDAO convert(const UserModel& model) {
        return UserDAO{
            model.getLogin(),
            model.getPasswordHash(),
            model.getMemberId(),
            model.getCreatedAt(),
            model.getUpdatedAt()
        };
    }
};

#endif //MORZE_USERDAOCONVERTER_H