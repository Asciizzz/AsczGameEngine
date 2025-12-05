#pragma once

#include <string>
#include <variant>
#include <vector>
#include <unordered_map>

// Other
#include <glm/glm.hpp>
#include "tinyType.hpp"

using tinyVar = std::variant<float, int, bool, glm::vec2, glm::vec3, glm::vec4, std::string, tinyHandle>;
using tinyVarsMap = std::unordered_map<std::string, tinyVar>;

// Debug log (FIFO)
class tinyDebug {
public:
    struct Entry {
        std::string str;
        float color[3] = {1.0f, 1.0f, 1.0f};
        const char* c_str() const { return str.c_str(); }
    };

    explicit tinyDebug(size_t maxLogs = 16) : maxLogs_(maxLogs) {}

    void log(const std::string& message, float r, float g, float b) {
        if (message.empty()) return;

        Entry entry;
        entry.str = message;
        entry.color[0] = r;
        entry.color[1] = g;
        entry.color[2] = b;

        if (logs_.size() >= maxLogs_) {
            logs_.erase(logs_.begin());
        }

        logs_.push_back(entry);
    }

    const std::vector<Entry>& logs() const { return logs_; }
    size_t maxLogs() const { return maxLogs_; }
    bool empty() const { return logs_.empty(); }
    size_t size() const { return logs_.size(); }
    void clear() { logs_.clear(); }

private:
    size_t maxLogs_;
    std::vector<Entry> logs_;
};

#include <fstream>

// Simple text file structure
struct tinyText {
    std::string str;
    const char* c_str() const { return str.c_str(); }

    static std::string readFrom(const std::string& filePath) noexcept {
        std::ifstream fileStream(filePath);
        if (!fileStream) return "";

        // Correct way to read entire file into string
        std::string content((std::istreambuf_iterator<char>(fileStream)),
                            (std::istreambuf_iterator<char>()));
        return content;
    }
};