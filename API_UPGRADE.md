# Lua API Upgrade - Component-Based Architecture

## Overview
The Lua scripting API has been refactored to use a component-based architecture where each node component returns its own object with specific methods. This makes the API more modular, scalable, and easier to understand.

## New Component-Based API

### Transform3D Component
```lua
local t3d = node:transform3D()  -- Returns Transform3D component or nil
if t3d then
    local pos = t3d:getPos()    -- Returns {x, y, z}
    pos.x = pos.x + 1
    t3d:setPos(pos)
    
    local rot = t3d:getRot()    -- Returns {x, y, z} (Euler angles)
    t3d:setRot(rot)
    
    local scl = t3d:getScl()    -- Returns {x, y, z}
    t3d:setScl(scl)
end
```

### Anim3D Component
```lua
local anim3d = node:anim3D()    -- Returns Anim3D component or nil
if anim3d then
    local walkAnim = anim3d:get("walk")        -- Get animation handle by name
    local currentAnim = anim3d:current()       -- Get current animation handle
    
    anim3d:play("walk", restart)               -- Play by name with optional restart
    anim3d:play(walkAnim, restart)             -- Or play by handle
    
    anim3d:setSpeed(1.5)
    local isPlaying = anim3d:isPlaying()
    
    local time = anim3d:getTime()
    anim3d:setTime(time)
    
    local duration = anim3d:getDuration()      -- Duration of current animation
    local duration = anim3d:getDuration(walkAnim)  -- Duration of specific animation
    
    anim3d:setLoop(true)
    local loop = anim3d:isLoop()
    
    anim3d:pause()
    anim3d:resume()
end
```

### Script Component
```lua
local script = node:script()    -- Returns Script component or nil
if script then
    local value = script:getVar("myVariable")
    script:setVar("myVariable", newValue)
end
```

## Node Hierarchy (Unchanged)
```lua
local parent = node:parent()               -- Returns parent Node or nil
local children = node:children()           -- Returns array of child Nodes

local parentHandle = node:parentHandle()   -- Returns lightuserdata handle or nil
local childHandles = node:childrenHandles() -- Returns array of lightuserdata handles
```

## Benefits

1. **Modularity**: Each component is self-contained with its own methods
2. **Scalability**: Easy to add new components without polluting the Node namespace
3. **Clarity**: Component methods are grouped logically
4. **Nil Safety**: Returns nil if component doesn't exist on the node
5. **Future-Proof**: Script component can be extended with more methods (e.g., call specific functions)

## Migration Guide

### Old API → New API

**Transform:**
```lua
-- Old:
local pos = node:getPos()
node:setPos(pos)

-- New:
local t3d = node:transform3D()
local pos = t3d:getPos()
t3d:setPos(pos)
```

**Animation:**
```lua
-- Old:
local anim = node:getAnimHandle("walk")
node:playAnim(anim, true)
node:setAnimSpeed(1.5)

-- New:
local anim3d = node:anim3D()
local anim = anim3d:get("walk")  -- Or just pass name directly:
anim3d:play("walk", true)
anim3d:setSpeed(1.5)
```

**Script Variables:**
```lua
-- Old:
local value = node:getVar("myVar")
node:setVar("myVar", value)

-- New:
local script = node:script()
local value = script:getVar("myVar")
script:setVar("myVar", value)
```

## Implementation Details

- Each component method returns a userdata object with its own metatable
- Component objects hold the same node handle internally
- Component existence is checked when retrieving the component (returns nil if absent)
- All component methods are inline for performance
- No additional memory allocation beyond the userdata wrapper
