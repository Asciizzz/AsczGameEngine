# Unified Handle System Migration Guide

## Overview

The handle system has been completely reworked to use a **unified `Handle("type", index, version)` structure** instead of the old `nHandle` and `rHandle` functions. This makes the system more extensible, type-safe, and easier to use.

---

## Quick Migration Reference

### Old System → New System

| Old Code | New Code | Description |
|----------|----------|-------------|
| `nHandle(index, version)` | `Handle("node", index, version)` | Node handle (scene-specific) |
| `rHandle(index, version)` | `Handle("script", index, version)` | Script resource handle |
| `rHandle(index, version)` | `Handle("scene", index, version)` | Scene resource handle |
| `handleEqual(h1, h2)` | `h1 == h2` or `h1 ~= h2` | Handle comparison (uses `__eq` metamethod) |

---

## Supported Handle Types

### Scene-Specific Handles (Remapped on Scene Load)
- **`Handle("node", ...)`** - References to nodes within a scene
  - Gets remapped when a scene is instantiated
  - Used for hierarchy operations, component access, etc.

### Global Registry Handles (Never Remapped)
- **`Handle("script", ...)`** - References to script resources in the filesystem registry
- **`Handle("scene", ...)`** - References to scene resources in the filesystem registry
- **`Handle("mesh", ...)`** - References to mesh resources (future support)
- **`Handle("texture", ...)`** - References to texture resources (future support)
- **`Handle("material", ...)`** - References to material resources (future support)
- **`Handle("skeleton", ...)`** - References to skeleton resources (future support)

### Internal Handles
- **`Handle("animation", ...)`** - Animation handles returned by `anim3D:get()` and `anim3D:current()`

---

## Migration Examples

### Example 1: Script Variables

**Old Code:**
```lua
function vars()
    return {
        targetNode = nHandle(0xFFFFFFFF, 0xFFFFFFFF),
        scriptHandle = rHandle(0xFFFFFFFF, 0xFFFFFFFF),
        sceneHandle = rHandle(0xFFFFFFFF, 0xFFFFFFFF)
    }
end
```

**New Code:**
```lua
function vars()
    return {
        targetNode = Handle("node", 0xFFFFFFFF, 0xFFFFFFFF),
        scriptHandle = Handle("script", 0xFFFFFFFF, 0xFFFFFFFF),
        sceneHandle = Handle("scene", 0xFFFFFFFF, 0xFFFFFFFF)
    }
end
```

### Example 2: Handle Comparison

**Old Code:**
```lua
if handleEqual(curHandle, idleHandle) then
    print("Playing idle animation")
end
```

**New Code:**
```lua
if curHandle == idleHandle then
    print("Playing idle animation")
end

-- Or use ~= for inequality
if curHandle ~= idleHandle then
    print("Not playing idle animation")
end
```

### Example 3: Scene Instantiation

**Old Code:**
```lua
-- Both arguments had to be the correct handle type (nHandle vs rHandle)
SCENE:addScene(sceneRHandle, parentNHandle)
```

**New Code:**
```lua
-- Handle types are validated automatically
SCENE:addScene(sceneHandle, parentNodeHandle)

-- The system will error if you pass the wrong type
-- e.g., passing a script handle instead of scene handle
```

### Example 4: Accessing Resources

**Old Code:**
```lua
-- Had to use specific methods for each resource type
local script = FS:script(scriptHandle)
```

**New Code:**
```lua
-- Unified FS:get() method works for all resource types
local script = FS:get(scriptHandle)  -- scriptHandle must be Handle("script", ...)

-- The type is automatically determined from the handle
```

### Example 5: Handle Validation

**Old Code:**
```lua
if VARS.nodeHandle then
    -- Use the handle
end
```

**New Code:**
```lua
-- Use the :valid() method for explicit validation
if VARS.nodeHandle:valid() then
    -- Use the handle
end

-- Handles also have introspection methods
local handleType = VARS.nodeHandle:type()      -- "node"
local handleIndex = VARS.nodeHandle:index()    -- 123
local handleVersion = VARS.nodeHandle:version() -- 456
```

### Example 6: Animation Handles

**Old Code:**
```lua
local idleHandle = anim3d:get("Idle_Loop")
local curHandle = anim3d:current()

if not handleEqual(curHandle, idleHandle) then
    anim3d:play(idleHandle, true)
end
```

**New Code:**
```lua
local idleHandle = anim3d:get("Idle_Loop")  -- Returns Handle("animation", ...)
local curHandle = anim3d:current()          -- Returns Handle("animation", ...)

-- Use == operator directly
if curHandle ~= idleHandle then
    anim3d:play(idleHandle, true)
end
```

---

## Handle Methods

All handles support the following methods:

| Method | Returns | Description |
|--------|---------|-------------|
| `:type()` | `string` | Returns the type string (e.g., "node", "script", "scene") |
| `:index()` | `number` | Returns the handle's index component |
| `:version()` | `number` | Returns the handle's version component |
| `:valid()` | `boolean` | Returns true if the handle is valid (not invalid/nil) |

---

## Handle Comparison

Handles support the `==` and `~=` operators through the `__eq` metamethod:

```lua
local h1 = Handle("node", 100, 1)
local h2 = Handle("node", 100, 1)
local h3 = Handle("node", 200, 1)

if h1 == h2 then
    print("Same handle!")  -- This will print
end

if h1 ~= h3 then
    print("Different handles!")  -- This will print
end
```

**Important:** Handles are equal if both their **type** and **underlying tinyHandle** match.

---

## Global Variables

The following global variables are automatically set in every script's `update()` function:

| Variable | Type | Description |
|----------|------|-------------|
| `NODE` | Node object | Current node as a Node userdata (for calling methods like `NODE:transform3D()`) |
| `SCENE` | Scene object | Current scene object |
| `FS` | FS object | Filesystem registry accessor |
| `VARS` | table | Script's runtime variables |
| `DTIME` | number | Delta time since last frame |

---

## Type Safety

The new system provides better type safety:

```lua
-- This will work
SCENE:addScene(sceneHandle, nodeHandle)  -- sceneHandle is Handle("scene", ...)

-- This will error with a clear message
SCENE:addScene(scriptHandle, nodeHandle)  -- Error: expects scene handle, got 'script'

-- This will also error
SCENE:node(sceneHandle)  -- Error: expects node handle, got 'scene'
```

---

## Backward Compatibility

The old `nHandle()`, `rHandle()`, and `handleEqual()` functions have been **removed**. All scripts must be updated to use the new unified `Handle()` system.

---

## Best Practices

1. **Always specify the type explicitly:**
   ```lua
   Handle("node", index, version)  -- Good
   ```

2. **Use `:valid()` to check handle validity:**
   ```lua
   if myHandle:valid() then
       -- Use the handle
   end
   ```

3. **Use `==` and `~=` for comparisons:**
   ```lua
   if handle1 == handle2 then
       -- Same handle
   end
   ```

4. **Use descriptive variable names with type hints:**
   ```lua
   targetNodeHandle = Handle("node", ...)  -- Clear what type it is
   playerScriptHandle = Handle("script", ...)
   mainSceneHandle = Handle("scene", ...)
   ```

5. **Leverage handle introspection for debugging:**
   ```lua
   print("Handle info: " .. tostring(myHandle))
   -- Output: Handle("node", 123, 456)
   ```

---

## Complete Example: Before and After

### Before (Old System)

```lua
function vars()
    return {
        targetNode = nHandle(0xFFFFFFFF, 0xFFFFFFFF),
        scriptRes = rHandle(0xFFFFFFFF, 0xFFFFFFFF)
    }
end

function update()
    local node = SCENE:node(VARS.targetNode)
    if node then
        local script = FS:script(VARS.scriptRes)
        if script then
            script:call("doSomething")
        end
    end
    
    local anim = NODE:anim3D()
    if anim then
        local idle = anim:get("Idle")
        local cur = anim:current()
        if not handleEqual(cur, idle) then
            anim:play(idle, true)
        end
    end
end
```

### After (New System)

```lua
function vars()
    return {
        targetNode = Handle("node", 0xFFFFFFFF, 0xFFFFFFFF),
        scriptRes = Handle("script", 0xFFFFFFFF, 0xFFFFFFFF)
    }
end

function update()
    if VARS.targetNode:valid() then
        local node = SCENE:node(VARS.targetNode)
        if node and VARS.scriptRes:valid() then
            local script = FS:get(VARS.scriptRes)
            if script then
                script:call("doSomething")
            end
        end
    end
    
    local anim = NODE:anim3D()
    if anim then
        local idle = anim:get("Idle")
        local cur = anim:current()
        if cur ~= idle then
            anim:play(idle, true)
        end
    end
end
```

---

## Need Help?

See `scripts/handle_demo.lua` for a comprehensive demonstration of all handle features!
