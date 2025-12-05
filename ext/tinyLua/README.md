# tinyLua

A simple, context-free Lua wrapper. Like `tinyUI`, it's just a namespace with inline functions.

## Philosophy

**tinyLua knows NOTHING about:**
- Scenes, entities, nodes, or any game engine concepts
- Your specific application domain
- Custom types beyond basic Lua types

**tinyLua is just:**
- A thin wrapper around Lua C API
- A place to hook custom bindings
- Reusable across any project

## Usage

### Basic Example

```cpp
#include "tinyLua.hpp"

// Create Lua state
lua_State* L = tinyLua::Init();

// Compile code
std::string code = "function test() print('Hello') end";
tinyLua::Compile(L, code);

// Call function
tinyLua::Call(L, "test");

// Cleanup
tinyLua::Close(L);
```

### With Custom Bindings

```cpp
// Set up your custom bindings BEFORE Init()
tinyLua::SetBindings([](lua_State* L) {
    // Register your types, functions, etc.
    lua_pushcfunction(L, myFunction);
    lua_setglobal(L, "myFunction");
});

// Now initialize (bindings will be applied)
lua_State* L = tinyLua::Init();
```

### With Debug Logging

```cpp
tinyDebug debug;

lua_State* L = tinyLua::Init();
tinyLua::Compile(L, code, &debug);
tinyLua::Call(L, "test", &debug);

// Check logs
for (const auto& entry : debug.logs()) {
    printf("%s\n", entry.c_str());
}
```

## How tinyScript Uses It

`tinyScript` is an example of building on top of `tinyLua`:

```cpp
bool tinyScript::compile() {
    // 1. Set scene-specific bindings
    tinyLua::SetBindings([](lua_State* L) {
        registerNodeBindings(L);  // Scene/node/entity stuff
    });
    
    // 2. Use tinyLua to create and compile
    L_ = tinyLua::Init();
    tinyLua::Compile(L_, code, &debug_);
    
    // 3. Do your own caching/processing
    cacheDefaultVars();
}
```

This keeps `tinyLua` generic while `tinyScript` adds engine-specific features.

## Design

- **Simple**: Just a namespace, no classes or state
- **Optional bindings**: Hook point for custom types
- **Debug-friendly**: Optional logging parameter
- **Minimal**: Does one thing well
- **Reusable**: Can outlive any single project

## Files

- `tinyLuaDef.hpp` - Shared types (tinyVar, tinyDebug, tinyText)
- `tinyLua.hpp` - Core wrapper functions
