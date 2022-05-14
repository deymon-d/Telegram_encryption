#include "client.h"
#include "symmetry_encryption.h"
#include <iostream>

class Manager {
public:
    void loop() {
        std::string command;
        while (true) {
            client_.Prepare();
            std::cin >> command;
            if (command == "exit") {
                break;
            } else if (command == "send") {
                std::string name, text;
                std::cin >> name >> text;
                text = encryption(text);
                std::cout << name << ' ' << text << std::endl;
                client_.SendMessage(name, text);
            } else if (command == "get") {
                std::string name;
                size_t count;
                std::cin >> name >> count;
                for (auto& message : client_.GetMessages(name, count)) {
                    std::cout << decryption(message) << std::endl;
                }
            } else if (command == "update") {
                client_.Update();\
            }
        }
    }

private:
    Client client_;
};

int main() {
    Manager manager;
    manager.loop();
}

