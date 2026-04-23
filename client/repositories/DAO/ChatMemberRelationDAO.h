#ifndef MORZE_CHATMEMBERRELATIONDAO_H
#define MORZE_CHATMEMBERRELATIONDAO_H

#include <boost/uuid/uuid.hpp>
#include "UUIDConverter.h"

class ChatMemberRelationDAO {
public:
    ChatMemberRelationDAO() = default;

    ChatMemberRelationDAO(boost::uuids::uuid id,
                          boost::uuids::uuid chat_id,
                          boost::uuids::uuid member_id)
        : id_(id), chat_id_(chat_id), member_id_(member_id) {}

    const boost::uuids::uuid& getId() const { return id_; }
    std::vector<char> getIdAsBLOB() const { return UUIDConverter::toBlob(id_); }
    std::string getIdAsString() const { return UUIDConverter::toString(id_); }

    const boost::uuids::uuid& getChatId() const { return chat_id_; }
    std::vector<char> getChatIdAsBLOB() const { return UUIDConverter::toBlob(chat_id_); }
    std::string getChatIdAsString() const { return UUIDConverter::toString(chat_id_); }

    const boost::uuids::uuid& getMemberId() const { return member_id_; }
    std::vector<char> getMemberIdAsBLOB() const { return UUIDConverter::toBlob(member_id_); }
    std::string getMemberIdAsString() const { return UUIDConverter::toString(member_id_); }

    void setId(boost::uuids::uuid id) { id_ = id; }
    void setIdFromBLOB(const std::vector<char>& blob) { id_ = UUIDConverter::fromBlob(blob); }
    void setIdFromString(const std::string& str) { id_ = UUIDConverter::fromString(str); }

    void setChatId(boost::uuids::uuid chat_id) { chat_id_ = chat_id; }
    void setChatIdFromBLOB(const std::vector<char>& blob) { chat_id_ = UUIDConverter::fromBlob(blob); }
    void setChatIdFromString(const std::string& str) { chat_id_ = UUIDConverter::fromString(str); }

    void setMemberId(boost::uuids::uuid member_id) { member_id_ = member_id; }
    void setMemberIdFromBLOB(const std::vector<char>& blob) { member_id_ = UUIDConverter::fromBlob(blob); }
    void setMemberIdFromString(const std::string& str) { member_id_ = UUIDConverter::fromString(str); }

private:
    boost::uuids::uuid id_;
    boost::uuids::uuid chat_id_;
    boost::uuids::uuid member_id_;
};

#endif // MORZE_CHATMEMBERRELATIONDAO_H