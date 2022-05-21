#include "client.h"
#include "symmetry_encryption.h"
#include <iostream>
#include <functional>
#include <map>

class Manager {
public:
    Manager() {
        functions_["exit"] = &Manager::Exit;
        functions_["send"] = &Manager::SendMessage;
        functions_["get"] = &Manager::GetMessages;
        functions_["update"] = &Manager::Update;
    }

    void loop() {
        std::string command;
        at_work_ = true;
        while (at_work_) {
            client_.Prepare();
            std::cin >> command;
            auto it = functions_.find(command);
            if (it != functions_.end()) {
                it->second(*this);
            }
        }
    }

private:
    Client client_;
    std::map<std::string, std::function<void(Manager&)>> functions_;
    bool at_work_{false};

    void Exit() {
        at_work_ = false;
    }

    void SendMessage() {
        std::string name, text;
        std::cin >> name >> text;
        text = encryption(text);
        std::cout << name << ' ' << text << std::endl;
        client_.SendMessage(name, text);
    }

    void GetMessages() {
        std::string name;
        size_t count;
        std::cin >> name >> count;
        for (auto& message : client_.GetMessages(name, count)) {
            std::cout << decryption(message) << std::endl;
        }
    }

    void Update() {
        client_.Update();
    }
};

int main() {
    Manager manager;
    manager.loop();
}

