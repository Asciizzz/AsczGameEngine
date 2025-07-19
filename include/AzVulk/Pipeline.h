#pragma once

#include <vector>

namespace AzVulk {

class Pipeline {
public:
    Pipeline(const char *vertfile, const char *fragfile);

private:
    static std::vector<char> readFile(const char *path);

    void createGraphicsPipeline(const char *vertfile, const char *fragfile);
};

} // namespace AzVulk