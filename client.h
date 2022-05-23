#pragma once

#include <td/telegram/td_api.h>
#include <td/telegram/Client.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <map>

namespace td_api = td::td_api;

struct Message {
    Message(std::string author_in, std::string content_in) :
               author(std::move(author_in)), content(std::move(content_in)) {};

    std::string author;
    std::string content;
};

class Client {
public:
    Client();
    void Prepare();
    void SendMessage(const std::string& name, const std::string& message);
    std::vector<Message> GetMessages(const std::string& name, size_t count);
    void Update();
private:
    using Object = td_api::object_ptr<td_api::Object>;

    std::unique_ptr<td::ClientManager> client_manager_;
    std::int32_t client_id_{0};
    std::uint64_t current_query_id_{0};
    std::uint64_t authentication_query_id_{0};
    td_api::object_ptr<td_api::AuthorizationState> authorization_state_;
    bool are_authorized_{false};
    bool need_restart_{false};
    std::map<std::uint64_t, std::function<void(Object)>> handlers_;
    std::map<std::string, int64_t> chats_;
    std::map<int64_t, std::string> chat_ids_;

    std::uint64_t nextQueryId();
    void SendQuery(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler);
    void ProcessResponse(td::ClientManager::Response response);
    void OnAuthorizationStateUpdate();
    auto CreateAuthenticationQueryHandler();
    void GetChanges();
};
