#include <iostream>
#include "store/kv_store.hpp"

int main() {
    KVStore store;

    // 1. Initially empty
    std::cout << "Initial size: " << store.size() << '\n';

    // 2. Set a new key
    store.set("name", "Vikas");
    store.set("language", "C++");

    std::cout << "Size after adding 2 keys: "
              << store.size() << '\n';

    // 3. Get an existing key
    auto name = store.get("name");

    if (name) {
        std::cout << "name: " << *name << '\n';
    } else {
        std::cout << "name not found\n";
    }

    // 4. Get a key that doesn't exist
    auto age = store.get("age");

    if (age) {
        std::cout << "age: " << *age << '\n';
    } else {
        std::cout << "age not found\n";
    }

    // 5. Check contains()
    std::cout << "Contains 'name': "
              << (store.contains("name") ? "yes" : "no")
              << '\n';

    std::cout << "Contains 'age': "
              << (store.contains("age") ? "yes" : "no")
              << '\n';

    // 6. Update an existing key
    store.set("name", "Rahul");

    auto updatedName = store.get("name");

    if (updatedName) {
        std::cout << "Updated name: " << *updatedName << '\n';
    }

    // 7. Delete an existing key
    bool deleted = store.del("language");

    std::cout << "Deleted 'language': "
              << (deleted ? "yes" : "no")
              << '\n';

    // 8. Try deleting a key that doesn't exist
    deleted = store.del("age");

    std::cout << "Deleted 'age': "
              << (deleted ? "yes" : "no")
              << '\n';

    // 9. Final state
    std::cout << "Final size: " << store.size() << '\n';

    return 0;
}