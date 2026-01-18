#include <iostream>
#include "KVStore.h"


int main(){

    KVStore store("config/system_config.json");

    std::string value;

    // PUT operations
    store.put("name", "AuroraKV");
    store.put("version", "1.0");
    store.put("author", "Harshit");

    // GET operations
    if (store.get("name", value)) {
        std::cout << "name = " << value << "\n";
    }

    if (store.get("version", value)) {
        std::cout << "version = " << value << "\n";
    }

    // DELETE operation
    store.deleteKey("author");

    if(!store.get("author", value)){
        std::cout << "author key deleted\n";
    }

    return 0;
}
