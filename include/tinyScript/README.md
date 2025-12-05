# tinyScript

A **completely engine-independent** Lua scripting system.

## Philosophy

tinyScript doesn't know what a Scene is. Or an Entity. Or a Transform. Or your application domain at all.

Just like `tinyUI` doesn't know what Vulkan is, `tinyScript` doesn't know what your engine is. It's **pure Lua scripting** with optional custom API injection.

## Core Features (Engine-Independent)

- ✅ Compile and execute Lua code
- ✅ Call Lua functions by name
- ✅ VARS/LOCALS table system for persistent script data
- ✅ Debug logging with FIFO circular buffer
- ✅ Version tracking for hot-reloading
- ✅ Move semantics for efficient ownership transfer

## Usage

### Standalone (No Bindings)

```cpp
#include "tinyScript/tinyScript.hpp"

tinyScript script;
script.code = R"(
    function greet(name)
        print("Hello, " .. name .. "!")
    end
)";

if (script.compile()) {
    script.call("greet");  // Prints: Hello, !
}
```

### With Custom Bindings (Your Engine/App)

```cpp
#include "tinyScript/tinyScript.hpp"

// 1. Define your custom bindings
struct MyAppBindings : public IScriptBindings {
    void onCompile(lua_State* L) override {
        // Register YOUR custom Lua APIs
        lua_register(L, "myCustomFunction", my_c_function);
        // Register metatables, globals, whatever you need
    }
    
    void onPreCall(lua_State* L, void* userData) override {
        // Push context into Lua before function calls
        // userData can be anything your app needs
        auto* ctx = static_cast<MyContext*>(userData);
        lua_pushnumber(L, ctx->deltaTime);
        lua_setglobal(L, "DELTATIME");
    }
    
    void onPostCall(lua_State* L, void* userData) override {
        // Pull modified data back from Lua after function calls
    }
};

// 2. Use it
tinyScript script;
script.setBindings(new MyAppBindings());
script.code = "function update() myCustomFunction() end";
script.compile();

MyContext ctx;
script.call("update", &ctx);  // userData passed to callbacks
```

## For AsczGameEngine Users

If you're using this with AsczGameEngine specifically:

```cpp
#include "tinyScript/tinyScript.hpp"
#include "tinyScript/tinyScript_EngineBindings.hpp"

tinyScript script;
script.setBindings(new tinyScript::EngineBindings());
script.compile();

// Now your scripts have access to NODE, SCENE, Transform3D, etc.
```

## Design Inspiration

This follows the same pattern as `tinyUI`:

- **tinyUI.hpp** = Core UI system (doesn't know about Vulkan)
- **tinyUI_Vulkan.hpp** = Vulkan-specific implementation (optional)

Similarly:

- **tinyScript.hpp** = Core scripting system (doesn't know about your engine)
- **tinyScript_EngineBindings.hpp** = Your engine-specific bindings (optional)

## Dependencies

- ✅ **tinyType** (external library for handles) - also engine-independent
- ✅ **glm** (math library for tinyVar support)
- ✅ **Lua** (embedded via luacpp)

That's it! No engine dependencies.

## Future Projects

This design means tinyScript can be reused in:
- Game engines
- Application scripting
- Testing frameworks  
- Build systems
- Anything that needs embedded Lua

Just implement `IBindings` for your domain and inject whatever APIs you want!
