#include "client.h"

#include <td/telegram/td_api.h>
#include <iostream>
#include <thread>
#include <chrono>
// overloaded
namespace detail {
    template <class... Fs>
    struct overload;

    template <class F>
    struct overload<F> : public F {
        explicit overload(F f) : F(f) {
        }
    };
    template <class F, class... Fs>
    struct overload<F, Fs...>
            : public overload<F>
                    , public overload<Fs...> {
        overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
        }
        using overload<F>::operator();
        using overload<Fs...>::operator();
    };
}  // namespace detail

template <class... F>
auto overloaded(F... f) {
    return detail::overload<F...>(f...);
}

Client::Client() {
    td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(1));
    client_manager_ = std::make_unique<td::ClientManager>();
    client_id_ = client_manager_->create_client_id();
    SendQuery(td_api::make_object<td_api::getOption>("version"), {});
}

void Client::Prepare() {
    if (need_restart_) {
        client_manager_.reset();
        *this = Client();
        Prepare();
    }
    if (!are_authorized_) {
        ProcessResponse(client_manager_->receive(10));
        Prepare();
    }
}

void Client::SendMessage(const std::string &name, const std::string& message) {
    auto send_message = td_api::make_object<td_api::sendMessage>();
    auto chat_id =  chats_.find(name);
    if (chat_id == chats_.end()) {
        return;
    }
    send_message->chat_id_ = chat_id->second;
    auto message_content = td_api::make_object<td_api::inputMessageText>();
    message_content->text_ = td_api::make_object<td_api::formattedText>();
    message_content->text_->text_ = message;
    send_message->input_message_content_ = std::move(message_content);
    SendQuery(std::move(send_message), {});
    GetChanges();
}

std::vector<std::string> Client::GetMessages(const std::string &name, size_t count) {
    auto chat_id =  chats_.find(name);
    if (chat_id == chats_.end()) {
        return {};
    }
    std::vector<td_api::string> tmp;
    auto get_history = td_api::make_object<td_api::getChatHistory>(chat_id->second, 0, 0, count, false);
    SendQuery(std::move(get_history), [this, &tmp](Object object) {
        auto messages = td::move_tl_object_as<td_api::messages>(object);
        for (auto& message : messages->messages_) {
            auto text = td::move_tl_object_as<td_api::messageText>(message->content_);
            tmp.emplace_back(text->text_->text_);
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    GetChanges();
    return tmp;
}

void Client::GetChanges() {
    while (true) {
        auto response = client_manager_->receive(1);
        if (response.object) {
            ProcessResponse(std::move(response));
        } else {
            return;
        }
    }
}

void Client::Update() {
    SendQuery(td_api::make_object<td_api::getChats>(nullptr, 20), [this](Object object) {
        if (object->get_id() == td_api::error::ID) {
            return;
        }
        auto chats = td::move_tl_object_as<td_api::chats>(object);
        for (auto chat_id : chats->chat_ids_) {
            std::cout << "[chat_id:" << chat_id << "] [title:" << chat_id << "]" << std::endl;
        }});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    GetChanges();
}

std::uint64_t Client::nextQueryId() {
    return ++current_query_id_;
}

void Client::SendQuery(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
    auto query_id = nextQueryId();
    if (handler) {
        handlers_.emplace(query_id, std::move(handler));
    }
    client_manager_->send(client_id_, query_id, std::move(f));
}

void Client::ProcessResponse(td::ClientManager::Response response) {
    if (!response.object) {
        return;
    }
    if (response.request_id == 0) {
        td_api::downcast_call(*response.object, overloaded(
                [this](td_api::updateAuthorizationState &update_authorization_state) {
                    authorization_state_ = std::move(update_authorization_state.authorization_state_);
                    OnAuthorizationStateUpdate();
                },
                [this](td_api::updateNewChat &update_new_chat) {
                    chats_[update_new_chat.chat_->title_] = update_new_chat.chat_->id_;
                },
                [this](td_api::updateChatTitle &update_chat_title) {
                    chats_[update_chat_title.title_] = update_chat_title.chat_id_;
                },
                [this](auto &update) {}
                ));
        return;
    }
    auto it = handlers_.find(response.request_id);
    if (it != handlers_.end()) {
        it->second(std::move(response.object));
        handlers_.erase(it);
    }
}

auto Client::CreateAuthenticationQueryHandler() {
    return [this, id = authentication_query_id_](Object object) {
        if (id == authentication_query_id_ && object->get_id() == td_api::error::ID) {
            auto error = td::move_tl_object_as<td_api::error>(object);
            std::cout << "Error: " << to_string(error) << std::flush;
            OnAuthorizationStateUpdate();
        }
    };
}

void Client::OnAuthorizationStateUpdate() {
    authentication_query_id_++;
    td_api::downcast_call(
            *authorization_state_,
            overloaded(
                    [this](td_api::authorizationStateReady &) {
                        are_authorized_ = true;
                        std::cout << "Got authorization" << std::endl;
                    },
                    [this](td_api::authorizationStateLoggingOut &) {
                        are_authorized_ = false;
                        std::cout << "Logging out" << std::endl;
                    },
                    [this](td_api::authorizationStateClosing &) { std::cout << "Closing" << std::endl; },
                    [this](td_api::authorizationStateClosed &) {
                        are_authorized_ = false;
                        need_restart_ = true;
                        std::cout << "Terminated" << std::endl;
                    },
                    [this](td_api::authorizationStateWaitCode &) {
                        std::cout << "Enter authentication code: " << std::flush;
                        std::string code;
                        std::cin >> code;
                        SendQuery(td_api::make_object<td_api::checkAuthenticationCode>(code),
                                   CreateAuthenticationQueryHandler());
                    },
                    [this](td_api::authorizationStateWaitRegistration &) {
                        std::string first_name;
                        std::string last_name;
                        std::cout << "Enter your first name: " << std::flush;
                        std::cin >> first_name;
                        std::cout << "Enter your last name: " << std::flush;
                        std::cin >> last_name;
                        SendQuery(td_api::make_object<td_api::registerUser>(first_name, last_name),
                                   CreateAuthenticationQueryHandler());
                    },
                    [this](td_api::authorizationStateWaitPassword &) {
                        std::cout << "Enter authentication password: " << std::flush;
                        std::string password;
                        std::getline(std::cin, password);
                        SendQuery(td_api::make_object<td_api::checkAuthenticationPassword>(password),
                                   CreateAuthenticationQueryHandler());
                    },
                    [this](td_api::authorizationStateWaitOtherDeviceConfirmation &state) {
                        std::cout << "Confirm this login link on another device: " << state.link_ << std::endl;
                    },
                    [this](td_api::authorizationStateWaitPhoneNumber &) {
                        std::cout << "Enter phone number: " << std::flush;
                        std::string phone_number;
                        std::cin >> phone_number;
                        SendQuery(td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone_number, nullptr),
                                   CreateAuthenticationQueryHandler());
                    },
                    [this](td_api::authorizationStateWaitEncryptionKey &) {
                        std::cout << "Enter encryption key or DESTROY: " << std::flush;
                        std::string key;
                        std::getline(std::cin, key);
                        if (key == "DESTROY") {
                            SendQuery(td_api::make_object<td_api::destroy>(), CreateAuthenticationQueryHandler());
                        } else {
                            SendQuery(td_api::make_object<td_api::checkDatabaseEncryptionKey>(std::move(key)),
                                       CreateAuthenticationQueryHandler());
                        }
                    },
                    [this](td_api::authorizationStateWaitTdlibParameters &) {
                        auto parameters = td_api::make_object<td_api::tdlibParameters>();
                        parameters->database_directory_ = "tdlib";
                        parameters->use_message_database_ = true;
                        parameters->use_secret_chats_ = true;
                        parameters->api_id_ = 94575;
                        parameters->api_hash_ = "a3406de8d171bb422bb6ddf3bbd800e2";
                        parameters->system_language_code_ = "en";
                        parameters->device_model_ = "Desktop";
                        parameters->application_version_ = "1.0";
                        parameters->enable_storage_optimizer_ = true;
                        SendQuery(td_api::make_object<td_api::setTdlibParameters>(std::move(parameters)),
                                   CreateAuthenticationQueryHandler());
                    }));
}
