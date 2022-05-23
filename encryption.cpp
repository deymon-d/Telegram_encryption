#include "encryption.h"

std::string encryption(const std::string& text) {
    std::string copy = text;
    for (size_t i = 0; i < copy.size(); ++i) {
        copy[i]++;
    }
    return copy;
}

std::string decryption(const std::string& text) {
    std::string copy = text;
    for (size_t i = 0; i < copy.size(); ++i) {
        copy[i]--;
    }
    return copy;
}