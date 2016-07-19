#ifndef DISCORDBOT_UTILS_H
#define DISCORDBOT_UTILS_H

#include <string>

bool stringNullOrEmpty(std::string in) {
    return in == NULL || in == "";
}

#endif //DISCORDBOT_UTILS_H
