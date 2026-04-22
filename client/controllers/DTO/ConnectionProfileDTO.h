//
// Created by ivan on 22.04.2026.
//

#ifndef CLIENT_CONNECTIONPROFILEDTO_H
#define CLIENT_CONNECTIONPROFILEDTO_H


class ConnectionProfileDTO {
public:
    ConnectionProfileDTO() = default;

    const std::string& getId() const { return id_; }
    const std::string& getServerUrl() const { return serverUrl_; }
    const std::string& getStunUrl() const { return stunUrl_; }
    const std::string& getUpdatedAt() const { return updatedAt_; }

    void setId(const std::string& id) { id_ = id; }
    void setServerUrl(const std::string& url) { serverUrl_ = url; }
    void setStunUrl(const std::string& url) { stunUrl_ = url; }
    void setUpdatedAt(const std::string& updatedAt) { updatedAt_ = updatedAt; }

private:
    std::string id_;
    std::string serverUrl_;
    std::string stunUrl_;
    std::string updatedAt_;
};


#endif //CLIENT_CONNECTIONPROFILEDTO_H