#include "store/kv_store.hpp"
#include "command/command_processor.hpp"
#include "server/tcp_server.hpp"

int main() {

    KVStore store;

    CommandProcessor processor(store);

    TcpServer server(6380, processor);

    server.run();

    return 0;
}