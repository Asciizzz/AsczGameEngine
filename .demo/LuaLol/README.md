# Lua-C++ Communication Demo

This is a minimal example showing how Lua scripts can communicate with C++ in your engine.

## What it demonstrates:

1. **Parameter Passing**: C++ sets parameters that Lua can access
2. **Handle System**: Passing object handles between C++ and Lua
3. **Function Calls**: Lua calling C++ functions and vice versa
4. **Registry Access**: Scripts modifying game objects through handles

## Files:

- `main.cpp` - C++ side with mock registry and Lua integration
- `test_script.lua` - Lua script with exposed parameters and functions

## How to build (you'll need Lua library):

```bash
# Install Lua development libraries first
# Then compile:
g++ main.cpp -llua -o lua_demo
./lua_demo
```

## Expected Output:

```
=== Lua-C++ Communication Demo ===

Lua: Script loaded!
Lua: Script functions defined!

--- Setting up parameters ---

--- Calling Lua functions ---
Lua: Initializing script for player: Hero
Lua: Got valid player handle: 1
C++: Set position of 'PlayerObject' to (10, 20, 30)
Lua: Set initial position to (10.0, 20.0, 30.0)
Lua: Update called
Lua: Current position: (10.0, 20.0, 30.0)
C++: Set position of 'PlayerObject' to (15, 22, 31)
Lua: Moved to (15.0, 22.0, 31.0)
Lua: Showing status...
C++: Lua says: Hello from Lua! Player: Hero
C++: Set position of 'LuaCreatedObject' to (100, 200, 300)
Lua: Created new object with handle: 2
Lua: Positioned new object at (100, 200, 300)

--- Final state ---
C++: Final player position: (15, 22, 31)

Demo complete!
```

## Key Concepts Demonstrated:

1. **Bidirectional Communication**: Both C++ calling Lua and Lua calling C++
2. **Handle-Based System**: Objects referenced by integer handles
3. **Parameter System**: C++ sets script parameters, Lua accesses them
4. **Registry Pattern**: Mock registry manages objects by handle
5. **Function Registration**: C++ functions exposed to Lua scripts

This mirrors exactly how your TinyEngine scripting system would work!