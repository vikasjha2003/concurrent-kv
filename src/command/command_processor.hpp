#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../store/kv_store.hpp"

class CommandProcessor {
public:
    explicit CommandProcessor(KVStore& store);

    /*
     * Response convention:
     *
     * SET key value  -> "OK"
     * GET key        -> "VALUE <value>" or "NIL"
     * DEL key        -> "OK" or "NIL"
     * EXISTS key     -> "VALUE 1" or "VALUE 0"
     * PING           -> "PONG"
     * Invalid input  -> "ERR <reason>"
     *
     * Values returned by GET are always prefixed with "VALUE",
     * so they cannot be confused with response keywords.
     */
    std::string process(const std::string& input_line);

private:
    std::string handleSet(const std::vector<std::string>& args);
    std::string handleGet(const std::vector<std::string>& args);
    std::string handleDel(const std::vector<std::string>& args);
    std::string handleExists(const std::vector<std::string>& args);
    std::string handlePing(const std::vector<std::string>& args);

    KVStore& store_;

    using Handler =
        std::function<std::string(const std::vector<std::string>&)>;

    std::unordered_map<std::string, Handler> handlers_;
};