#define SDL_MAIN_HANDLED
#include "TinyApp/TinyApp.hpp"
#include <iostream>
#include <stdexcept>

// Lua includes
extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

void testLua() {
    std::cout << "=== Testing Lua Integration ===" << std::endl;
    
    // Create a new Lua state
    lua_State* L = luaL_newstate();
    if (!L) {
        std::cerr << "ERROR: Failed to create Lua state!" << std::endl;
        return;
    }
    
    // Load standard Lua libraries
    luaL_openlibs(L);
    
    // Test 1: Simple arithmetic
    std::cout << "Test 1: Simple arithmetic (2 + 3)..." << std::endl;
    const char* script1 = "result = 2 + 3; print('Lua says: 2 + 3 =', result)";
    
    if (luaL_dostring(L, script1) != LUA_OK) {
        std::cerr << "ERROR: " << lua_tostring(L, -1) << std::endl;
    } else {
        std::cout << "✓ Arithmetic test passed!" << std::endl;
    }
    
    // Test 2: String manipulation
    std::cout << "\nTest 2: String manipulation..." << std::endl;
    const char* script2 = R"(
        message = "Hello from " .. "Lua!"
        print('Lua says:', message)
        print('String length:', #message)
    )";
    
    if (luaL_dostring(L, script2) != LUA_OK) {
        std::cerr << "ERROR: " << lua_tostring(L, -1) << std::endl;
    } else {
        std::cout << "✓ String test passed!" << std::endl;
    }
    
    // Test 3: Function definition and calling
    std::cout << "\nTest 3: Function definition..." << std::endl;
    const char* script3 = R"(
        function greet(name)
            return "Hello, " .. name .. "!"
        end
        
        greeting = greet("AsczGameEngine")
        print('Lua says:', greeting)
    )";
    
    if (luaL_dostring(L, script3) != LUA_OK) {
        std::cerr << "ERROR: " << lua_tostring(L, -1) << std::endl;
    } else {
        std::cout << "✓ Function test passed!" << std::endl;
    }
    
    // Test 4: Get value from Lua back to C++
    std::cout << "\nTest 4: C++ <-> Lua communication..." << std::endl;
    lua_getglobal(L, "result");  // Get the 'result' variable we set earlier
    if (lua_isnumber(L, -1)) {
        int result = (int)lua_tonumber(L, -1);
        std::cout << "✓ Got result from Lua: " << result << std::endl;
    } else {
        std::cout << "✗ Failed to get result from Lua" << std::endl;
    }
    lua_pop(L, 1);  // Clean up stack
    
    // Get Lua version info
    std::cout << "\nLua Version: " << LUA_VERSION << std::endl;
    
    // Clean up
    lua_close(L);
    std::cout << "=== Lua Test Complete ===" << std::endl << std::endl;
}

int main() {
    // Test Lua integration first
    testLua();
    
    try {
        // Peak 2009 window engineering
        TinyApp app("AszcEngine", 1600, 900);
        app.run();
    } catch (const std::exception& e) {
        // Plot twist: things broke
        std::cerr << "Application error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Miraculously, nothing exploded
    return EXIT_SUCCESS;
}