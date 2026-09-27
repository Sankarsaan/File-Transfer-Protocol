#pragma once
#define ASIO_STANDALONE
#include <cstdint>
#include <cstring>

struct Header {
    char command[16];   // "PUT", "GET", "EXISTS", "OK", "YES", "NO", "ERR"
    char filename[256]; // Name of the file
    uint64_t filesize;  // Size of the file payload to follow

    Header() { clear(); }
    void clear() {
        std::memset(command, 0, sizeof(command));
        std::memset(filename, 0, sizeof(filename));
        filesize = 0;
    }
};