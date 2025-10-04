#include "include/TinyExt/TinyPool.hpp"
#include <iostream>
#include <memory>
#include <string>

int main() {
    std::cout << "Testing TinyPool rework...\n";

    // Test with raw types
    TinyPool<int> intPool;
    std::cout << "Initial count: " << intPool.count << "\n";

    // Test inserting items
    auto handle1 = intPool.insert(42);
    auto handle2 = intPool.insert(24);
    auto handle3 = intPool.insert(13);
    
    std::cout << "After 3 insertions, count: " << intPool.count << "\n";
    std::cout << "Items size: " << intPool.items.size() << "\n";
    std::cout << "Free list size: " << intPool.freeList.size() << "\n";

    // Test getting values
    int* val1 = intPool.get(handle1);
    int* val2 = intPool.get(handle2);
    int* val3 = intPool.get(handle3);
    
    if (val1) std::cout << "Handle1 value: " << *val1 << "\n";
    if (val2) std::cout << "Handle2 value: " << *val2 << "\n";
    if (val3) std::cout << "Handle3 value: " << *val3 << "\n";

    // Test removing middle item
    intPool.remove(handle2);
    std::cout << "After removing handle2, count: " << intPool.count << "\n";
    std::cout << "Free list size: " << intPool.freeList.size() << "\n";

    // Test inserting after removal (should reuse the slot)
    auto handle4 = intPool.insert(99);
    std::cout << "After inserting 99, count: " << intPool.count << "\n";
    std::cout << "Items size: " << intPool.items.size() << "\n";
    std::cout << "Free list size: " << intPool.freeList.size() << "\n";

    int* val4 = intPool.get(handle4);
    if (val4) std::cout << "Handle4 value: " << *val4 << " (should reuse slot)\n";

    // Test with unique_ptr
    std::cout << "\nTesting with unique_ptr...\n";
    TinyPool<std::unique_ptr<std::string>> stringPool;
    
    auto strHandle1 = stringPool.insert(std::make_unique<std::string>("Hello"));
    auto strHandle2 = stringPool.insert(std::make_unique<std::string>("World"));
    
    std::cout << "String pool count: " << stringPool.count << "\n";
    
    std::string* str1 = stringPool.get(strHandle1);
    std::string* str2 = stringPool.get(strHandle2);
    
    if (str1) std::cout << "String1: " << *str1 << "\n";
    if (str2) std::cout << "String2: " << *str2 << "\n";

    stringPool.remove(strHandle1);
    std::cout << "After removing string1, count: " << stringPool.count << "\n";
    
    // Test inserting raw type into unique_ptr pool
    auto strHandle3 = stringPool.insert(std::string("Test"));
    std::string* str3 = stringPool.get(strHandle3);
    if (str3) std::cout << "String3: " << *str3 << "\n";

    std::cout << "All tests completed!\n";
    return 0;
}