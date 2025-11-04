#pragma once

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

inline json read_configuration(std::string filepath) {
    std::ifstream fs(filepath);
    return json::parse(fs);
}
