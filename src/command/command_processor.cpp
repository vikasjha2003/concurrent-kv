#include "command_processor.hpp"

#include <sstream>

CommandProcessor::CommandProcessor(KVStore& store)
    : store_(store) {

    handlers_["SET"] = [this](const auto& args) {
        return handleSet(args);
    };

    handlers_["GET"] = [this](const auto& args) {
        return handleGet(args);
    };

    handlers_["DEL"] = [this](const auto& args) {
        return handleDel(args);
    };

    handlers_["EXISTS"] = [this](const auto& args) {
        return handleExists(args);
    };

    handlers_["PING"] = [this](const auto& args) {
        return handlePing(args);
    };
}

std::string CommandProcessor::process(const std::string& input_line) {

    std::istringstream stream(input_line);

    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token) {
        tokens.push_back(token);
    }

    if (tokens.empty()) {
        return "ERR empty command";
    }

    const std::string& command = tokens[0];

    std::vector<std::string> args(
        tokens.begin() + 1,
        tokens.end()
    );

    auto it = handlers_.find(command);

    if (it == handlers_.end()) {
        return "ERR unknown command";
    }

    return it->second(args);
}

std::string CommandProcessor::handleSet(
    const std::vector<std::string>& args) {

    if (args.size() != 2) {
        return "ERR SET requires key and value";
    }

    store_.set(args[0], args[1]);

    return "OK";
}

std::string CommandProcessor::handleGet(
    const std::vector<std::string>& args) {

    if (args.size() != 1) {
        return "ERR GET requires key";
    }

    auto value = store_.get(args[0]);

    if (!value) {
        return "NIL";
    }

    return "VALUE " + *value;
}

std::string CommandProcessor::handleDel(
    const std::vector<std::string>& args) {

    if (args.size() != 1) {
        return "ERR DEL requires key";
    }

    if (store_.del(args[0])) {
        return "OK";
    }

    return "NIL";
}

std::string CommandProcessor::handleExists(
    const std::vector<std::string>& args) {

    if (args.size() != 1) {
        return "ERR EXISTS requires key";
    }

    return store_.contains(args[0])
        ? "VALUE 1"
        : "VALUE 0";
}

std::string CommandProcessor::handlePing(
    const std::vector<std::string>& args) {

    if (!args.empty()) {
        return "ERR PING takes no arguments";
    }

    return "PONG";
}