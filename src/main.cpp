#include <iostream>
#include "store/kv_store.hpp"
#include "command/command_processor.hpp"

int main() {

    KVStore store;
    CommandProcessor processor(store);

    std::cout << processor.process("PING") << '\n';

    std::cout << processor.process("SET name Vikas") << '\n';

    std::cout << processor.process("GET name") << '\n';

    std::cout << processor.process("EXISTS name") << '\n';

    std::cout << processor.process("GET age") << '\n';

    std::cout << processor.process("DEL name") << '\n';

    std::cout << processor.process("EXISTS name") << '\n';

    std::cout << processor.process("DEL name") << '\n';

    std::cout << processor.process("SET foo") << '\n';

    std::cout << processor.process("GET") << '\n';

    std::cout << processor.process("FOO bar") << '\n';

    std::cout << processor.process("") << '\n';

    std::cout << processor.process("PING hello") << '\n';

    return 0;
}