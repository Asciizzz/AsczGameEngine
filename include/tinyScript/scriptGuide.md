# Lua Scripting API Guide

This guide documents all available functions and objects in the Lua scripting environment.

---

## Table of Contents
1. [Global Variables](#global-variables)
2. [Global Functions](#global-functions)
3. [Handle Functions](#handle-functions)
4. [Scene Object](#scene-object)
5. [Node Object](#node-object)
6. [Transform3D Component](#transform3d-component)
7. [Anim3D Component](#anim3d-component)
8. [Script Component](#script-component)
9. [Script Structure](#script-structure)

---

## Global Variables

Available in every script during execution:

```
VARS         -- Table containing all script variables (defined in vars() function)
DELTATIME        -- Delta time since last frame (float, in seconds)
SCENE        -- Scene object (Scene userdata)
NODE         -- Current node object (Node userdata)
FS           -- Filesystem registry accessor (FS userdata)
```

---

## Global Functions

### `print(...)`
Print messages to the debug log.

**Parameters:**
- `...` - Any number of values to print (strings, numbers, booleans, etc.)

**Example:**
```lua
print("Hello, world!")
print("Position:", pos.x, pos.y, pos.z)
print("Count:", count, "Active:", active)
```

---

### `kState(keyName)`
Check if a keyboard key is currently pressed.

**Parameters:**
- `keyName` (string) - Name of the key to check

**Returns:**
- `boolean` - True if key is pressed, false otherwise

**Supported Keys:**
- Letters: `"a"` to `"z"`
- Numbers: `"0"` to `"9"`
- Arrow keys: `"up"`, `"down"`, `"left"`, `"right"`
- Modifiers: `"shift"`, `"lshift"`, `"rshift"`, `"ctrl"`, `"lctrl"`, `"rctrl"`, `"alt"`, `"lalt"`, `"ralt"`
- Function keys: `"f1"` to `"f12"`
- Special: `"space"`, `"enter"`, `"return"`, `"escape"`, `"esc"`, `"tab"`, `"backspace"`, `"delete"`, `"insert"`, `"home"`, `"end"`, `"pageup"`, `"pagedown"`

**Example:**
```lua
if kState("w") then
    -- Move forward
end

if kState("shift") and kState("space") then
    -- Jump higher
end
```

---

## Handle System

Handles are unified objects that reference both nodes and resources. All handles are created with the `Handle(type, index?, version?)` constructor.

### `Handle(type, index?, version?)`
Create a handle of the specified type.

**Parameters:**
- `type` (string) - Handle type: `"node"`, `"scene"`, `"script"`, `"animation"`, `"mesh"`, `"texture"`, `"material"`, `"skeleton"`, or `"resource"`
- `index` (integer, optional) - Handle index (defaults to 0xFFFFFFFF for invalid handle)
- `version` (integer, optional) - Handle version (defaults to 0xFFFFFFFF for invalid handle)

**Returns:**
- `Handle` - Handle userdata object

**Example:**
```lua
-- Create specific handles
local nodeHandle = Handle("node", 0, 0)
local sceneHandle = Handle("scene", 5, 0)
local scriptHandle = Handle("script", 10, 1)

-- Create invalid handles (will be set later by editor or other scripts)
local targetNode = Handle("node")
local weaponMesh = Handle("mesh")

-- Invalid handles preserve their type
local invalid = Handle("script")
print(invalid:type())  -- Still prints "script" even though invalid
```

---

### Handle Methods

Handles have methods to inspect their properties:

#### `handle:type()`
Get the type string of a handle.

**Returns:**
- `string` - Type name ("node", "scene", "script", etc.)

**Example:**
```lua
local h = Handle("node", 0, 0)
print(h:type())  -- "node"
```

---

#### `handle:index()`
Get the index of a handle.

**Returns:**
- `integer` - Handle index

**Example:**
```lua
local h = Handle("scene", 5, 0)
print(h:index())  -- 5
```

---

#### `handle:version()`
Get the version of a handle.

**Returns:**
- `integer` - Handle version

**Example:**
```lua
local h = Handle("script", 10, 3)
print(h:version())  -- 3
```

---

#### `handle:valid()`
Check if a handle is valid (not 0xFFFFFFFF).

**Returns:**
- `boolean` - True if valid

**Example:**
```lua
local h = Handle("node")
if not h:valid() then
    print("Handle not yet assigned")
end
```

---

### Handle Comparison

Handles support the `==` and `~=` operators for comparison:

**Example:**
```lua
local h1 = Handle("node", 0, 0)
local h2 = Handle("node", 0, 0)
local h3 = Handle("node", 1, 0)

if h1 == h2 then
    print("Same handle!")
end

if h1 ~= h3 then
    print("Different handles!")
end
```

**Note:** Handles are equal if both their type and underlying handle values match.

---

## Scene Object

The scene object manages the scene hierarchy and provides access to nodes. Access via the global `SCENE` variable.

```
SCENE
├── :node(handle)
└── :addScene(sceneHandle, parentHandle?)
```

### `SCENE:node(handle)`
Get a node object from its handle.

**Parameters:**
- `handle` (Handle) - Handle with type "node"

**Returns:**
- `Node` - Node userdata object, or `nil` if not found or wrong type

**Example:**
```lua
local targetHandle = Handle("node", 10, 0)
local targetNode = SCENE:node(targetHandle)
if targetNode then
    local t3d = targetNode:transform3D()
    if t3d then
        local pos = t3d:getPos()
    end
end

-- Can also use NODE:handle() to get own handle
-- NODE is literall NODE, I don't think you need to call this
local currentNode = SCENE:node(NODE:handle())
```

---

### `SCENE:addScene(sceneHandle, parentHandle?)`
Instantiate a scene from the resource registry into the current scene.

**Parameters:**
- `sceneHandle` (Handle) - Handle with type "scene" pointing to Scene resource
- `parentHandle` (Handle, optional) - Handle with type "node" for parent (defaults to root if omitted)

**Returns:**
- None (void)

**Example:**
```lua
-- Load scene and attach to root
local sceneRes = Handle("scene", 5, 0)
SCENE:addScene(sceneRes)

-- Load scene and attach to current node
SCENE:addScene(sceneRes, NODE:handle())

-- Load scene and attach to specific node
local parentHandle = Handle("node", 10, 0)
SCENE:addScene(sceneRes, parentHandle)
```

---

## Node Object

Node objects represent scene nodes with components attached. Access the current node via the global `NODE` variable.

```
NODE
├── :transform3D()
├── :anim3D()
├── :script()
├── :parent()
├── :children()
├── :parentHandle()
└── :childrenHandles()
```

### `node:transform3D()`
Get the Transform3D component of this node.

**Returns:**
- `Transform3D` - Transform component, or `nil` if not present

**Example:**
```lua
local t3d = NODE:transform3D()
if t3d then
    local pos = t3d:getPos()
    print("Position:", pos.x, pos.y, pos.z)
end
```

---

### `node:anim3D()`
Get the Anim3D (animation) component of this node.

**Returns:**
- `Anim3D` - Animation component, or `nil` if not present

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    anim:play("walk")
    anim:setSpeed(1.5)
end
```

---

### `node:script()`
Get the Script component of this node.

**Returns:**
- `Script` - Script component, or `nil` if not present

**Example:**
```lua
local script = NODE:script()
if script then
    local value = script:getVar("myVar")
    script:setVar("myVar", value + 1)
end
```

---

### `node:parent()`
Get the parent node.

**Returns:**
- `Node` - Parent node userdata, or `nil` if this is root

**Example:**
```lua
local parentNode = NODE:parent()
if parentNode then
    local parentT3d = parentNode:transform3D()
end
```

---

### `node:children()`
Get all child nodes.

**Returns:**
- `table` - Array of Node userdata objects

**Example:**
```lua
local children = NODE:children()
for i, child in ipairs(children) do
    print("Child", i, "found")
    local childT3d = child:transform3D()
end
```

---

### `node:parentHandle()`
Get the parent node's handle.

**Returns:**
- `Handle` - Handle with type "node", or `nil` if this is root

**Example:**
```lua
local parentHandle = NODE:parentHandle()
if parentHandle then
    local parentNode = SCENE:node(parentHandle)
end
```

---

### `node:childrenHandles()`
Get handles of all child nodes.

**Returns:**
- `table` - Array of Handle objects with type "node"

**Example:**
```lua
local childHandles = NODE:childrenHandles()
for i, handle in ipairs(childHandles) do
    local child = SCENE:node(handle)
end
```

---

## Transform3D Component

Controls position, rotation, and scale of a node.

```
Transform3D
├── :getPos()
├── :setPos(vec3)
├── :getRot()
├── :setRot(vec3)
├── :getScl()
└── :setScl(vec3)
```

### `transform3D:getPos()`
Get the local position.

**Returns:**
- `table` - Vector3 table with `x`, `y`, `z` fields

**Example:**
```lua
local t3d = node:transform3D()
if t3d then
    local pos = t3d:getPos()
    print(pos.x, pos.y, pos.z)
end
```

---

### `transform3D:setPos(position)`
Set the local position.

**Parameters:**
- `position` (table) - Vector3 table with `x`, `y`, `z` fields

**Example:**
```lua
local t3d = NODE:transform3D()
if t3d then
    t3d:setPos({ x = 0, y = 5, z = 10 })
    
    -- Or modify existing position
    local pos = t3d:getPos()
    pos.y = pos.y + 1.0
    t3d:setPos(pos)
end
```

---

### `transform3D:getRot()`
Get the local rotation as Euler angles.

**Returns:**
- `table` - Vector3 table with `x`, `y`, `z` fields (radians)

**Example:**
```lua
local t3d = NODE:transform3D()
if t3d then
    local rot = t3d:getRot()
    print("Euler angles:", rot.x, rot.y, rot.z)
end
```

---

### `transform3D:setRot(rotation)`
Set the local rotation from Euler angles.

**Parameters:**
- `rotation` (table) - Vector3 table with `x`, `y`, `z` fields (radians)

**Example:**
```lua
local t3d = NODE:transform3D()
if t3d then
    -- Set rotation to 45 degrees on Y axis
    t3d:setRot({ x = 0, y = math.pi / 4, z = 0 })
    
    -- Or modify existing rotation
    local rot = t3d:getRot()
    rot.y = rot.y + 0.01
    t3d:setRot(rot)
end
```

---

### `transform3D:getScl()`
Get the local scale.

**Returns:**
- `table` - Vector3 table with `x`, `y`, `z` fields

**Example:**
```lua
local t3d = NODE:transform3D()
if t3d then
    local scale = t3d:getScl()
    print("Scale:", scale.x, scale.y, scale.z)
end
```

---

### `transform3D:setScl(scale)`
Set the local scale.

**Parameters:**
- `scale` (table) - Vector3 table with `x`, `y`, `z` fields

**Example:**
```lua
local t3d = NODE:transform3D()
if t3d then
    -- Uniform scale
    t3d:setScl({ x = 2, y = 2, z = 2 })
    
    -- Non-uniform scale
    t3d:setScl({ x = 1, y = 2, z = 1 })
end
```

---

## Anim3D Component

Controls animations.

```
Anim3D
├── :get(name)
├── :current()
├── :play(handle|name, restart?)
├── :setSpeed(speed)
├── :isPlaying()
├── :getTime()
├── :setTime(time)
├── :getDuration(handle?)
├── :setLoop(enabled)
├── :isLoop()
├── :pause()
└── :resume()
```

### `anim3D:get(name)`
Get animation handle by name.

**Parameters:**
- `name` (string) - Animation name

**Returns:**
- `Handle` - Handle with type "animation", or `nil` if not found

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    local walkHandle = anim:get("walk")
    if walkHandle then
        anim:play(walkHandle)
    end
end
```

---

### `anim3D:current()`
Get the currently playing animation handle.

**Returns:**
- `Handle` - Current animation Handle with type "animation", or `nil` if none

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    local current = anim:current()
    if current then
        print("Animation duration:", anim:getDuration(current))
    end
end
```

---

### `anim3D:play(animation, restart?)`
Play an animation.

**Parameters:**
- `animation` (Handle or string) - Animation handle or name
- `restart` (boolean, optional) - Whether to restart if already playing (default: true)

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    -- Play by name
    anim:play("walk")
    
    -- Play by handle
    local runHandle = anim:get("run")
    anim:play(runHandle, true)
    
    -- Don't restart if already playing
    anim:play("idle", false)
end
```

---

### `anim3D:setSpeed(speed)`
Set animation playback speed.

**Parameters:**
- `speed` (number) - Speed multiplier (1.0 = normal, 2.0 = double speed, 0.5 = half speed)

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    anim:setSpeed(1.5)  -- Play 50% faster
end
```

---

### `anim3D:isPlaying()`
Check if animation is currently playing.

**Returns:**
- `boolean` - True if playing

**Example:**
```lua
local anim = NODE:anim3D()
if anim and not anim:isPlaying() then
    anim:play("idle")
end
```

---

### `anim3D:getTime()`
Get current playback time.

**Returns:**
- `number` - Current time in seconds

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    local time = anim:getTime()
    print("Animation time:", time)
end
```

---

### `anim3D:setTime(time)`
Set playback time (seek to specific time).

**Parameters:**
- `time` (number) - Time in seconds

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    anim:setTime(2.5)  -- Jump to 2.5 seconds
end
```

---

### `anim3D:getDuration(handle?)`
Get animation duration.

**Parameters:**
- `handle` (Handle, optional) - Animation handle (defaults to current animation)

**Returns:**
- `number` - Duration in seconds

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    local duration = anim:getDuration()
    print("Current animation length:", duration)
    
    local walkHandle = anim:get("walk")
    print("Walk duration:", anim:getDuration(walkHandle))
end
```

---

### `anim3D:setLoop(enabled)`
Enable or disable animation looping.

**Parameters:**
- `enabled` (boolean) - True to loop, false to play once

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    anim:setLoop(true)   -- Loop forever
    anim:setLoop(false)  -- Play once
end
```

---

### `anim3D:isLoop()`
Check if animation is set to loop.

**Returns:**
- `boolean` - True if looping

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    if not anim:isLoop() then
        print("Animation will play once and stop")
    end
end
```

---

### `anim3D:pause()`
Pause animation playback.

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    anim:pause()
end
```

---

### `anim3D:resume()`
Resume paused animation.

**Example:**
```lua
local anim = NODE:anim3D()
if anim then
    anim:resume()
end
```

---

## Script Component

Access script variables on other nodes.

```
Script
├── :getVar(name)
└── :setVar(name, value)
```

### `script:getVar(name)`
Get a script variable value.

**Parameters:**
- `name` (string) - Variable name

**Returns:**
- `any` - Variable value (type depends on the variable)

**Example:**
```lua
local otherNode = SCENE:node(otherHandle)
if otherNode then
    local script = otherNode:script()
    if script then
        local health = script:getVar("health")
        print("Other node health:", health)
    end
end
```

---

### `script:setVar(name, value)`
Set a script variable value.

**Parameters:**
- `name` (string) - Variable name
- `value` (any) - New value

**Example:**
```lua
local otherNode = SCENE:node(otherHandle)
if otherNode then
    local script = otherNode:script()
    if script then
        script:setVar("health", 100)
        script:setVar("active", true)
    end
end
```

**Note:** For the current node's variables, use the `VARS` table directly:
```lua
-- Current node
VARS.health = VARS.health - 10

-- Other node
local script = otherNode:script()
if script then
    local otherHealth = script:getVar("health")
    script:setVar("health", otherHealth - 10)
end
```

---

## Script Structure

Every Lua script should define these functions:

```lua
-- Define script variables and their default values
function vars()
    return {
        -- Basic types
        myFloat = 0.0,
        myInt = 42,
        myBool = true,
        myString = "Hello",
        myVec3 = { x = 0, y = 0, z = 0 },
        
        -- Handles (will be set in editor or by other scripts)
        targetNode = Handle("node"),
        weaponMesh = Handle("mesh"),
        hitSound = Handle("resource"),
        scenePrefab = Handle("scene"),
        myScript = Handle("script")
    }
end

-- Called every frame
function update()
    -- Access variables through VARS table
    VARS.myFloat = VARS.myFloat + DELTATIME
    
    -- Use global objects
    local t3d = NODE:transform3D()
    if t3d then
        local pos = t3d:getPos()
        pos.y = pos.y + math.sin(VARS.myFloat)
        t3d:setPos(pos)
    end
    
    -- Check input
    if kState("space") then
        print("Space pressed!")
    end
    
    -- Use handles
    if VARS.targetNode:valid() then
        local target = SCENE:node(VARS.targetNode)
        if target then
            -- Do something with target
        end
    end
end
```

### Variable Types

Supported types in `vars()`:
- `number` (integer) - Integer values
- `number` (float) - Floating-point values
- `boolean` - True/false
- `string` - Text strings
- `table` - Vector3 with `x`, `y`, `z` fields
- `Handle` - Handle objects (created with `Handle(type, index?, version?)`)

---

## Complete Example

```lua
-- Character controller script

function vars()
    return {
        -- Movement
        moveSpeed = 5.0,
        rotSpeed = 3.0,
        
        -- Animation
        idleAnim = nil,
        walkAnim = nil,
        runAnim = nil,
        
        -- State
        isMoving = false,
        isRunning = false
    }
end

function update()
    local t3d = NODE:transform3D()
    local anim = NODE:anim3D()
    
    if not t3d then return end
    
    -- Get input
    local forward = 0
    local right = 0
    
    if kState("w") then forward = forward + 1 end
    if kState("s") then forward = forward - 1 end
    if kState("d") then right = right + 1 end
    if kState("a") then right = right - 1 end
    
    local isRunning = kState("shift")
    
    -- Calculate movement
    local moving = (forward ~= 0 or right ~= 0)
    
    if moving then
        -- Get current position and rotation
        local pos = t3d:getPos()
        local rot = t3d:getRot()
        
        -- Calculate direction
        local angle = math.atan2(right, forward)
        local targetYaw = rot.y + angle
        
        -- Rotate towards movement direction
        rot.y = rot.y + (targetYaw - rot.y) * VARS.rotSpeed * DELTATIME
        t3d:setRot(rot)
        
        -- Move forward
        local speed = isRunning and VARS.moveSpeed * 2.0 or VARS.moveSpeed
        pos.x = pos.x + math.sin(rot.y) * speed * DELTATIME
        pos.z = pos.z + math.cos(rot.y) * speed * DELTATIME
        t3d:setPos(pos)
        
        -- Play appropriate animation
        if anim then
            if isRunning then
                local current = anim:current()
                if current ~= VARS.runAnim then
                    anim:play(VARS.runAnim or "run")
                end
            else
                local current = anim:current()
                if current ~= VARS.walkAnim then
                    anim:play(VARS.walkAnim or "walk")
                end
            end
        end
    else
        -- Play idle animation
        if anim then
            local current = anim:current()
            if current ~= VARS.idleAnim then
                anim:play(VARS.idleAnim or "idle")
            end
        end
    end
    
    VARS.isMoving = moving
    VARS.isRunning = isRunning
end
```

---

## Tips and Best Practices

1. **Always check for nil:**
   ```lua
   local t3d = NODE:transform3D()
   if t3d then
       -- Safe to use t3d
   end
   ```

2. **Cache component references when possible:**
   ```lua
   local t3d = NODE:transform3D()
   local anim = NODE:anim3D()
   
   if t3d and anim then
       -- Use both components multiple times
   end
   ```

3. **Use delta time for frame-rate independent movement:**
   ```lua
   pos.x = pos.x + velocity * DELTATIME
   ```

4. **Store animation handles in vars() for efficiency:**
   ```lua
   function vars()
       return {
           walkAnim = Handle("animation")  -- Invalid, will be set later
       }
   end
   
   function update()
       local anim = NODE:anim3D()
       if anim and not VARS.walkAnim:valid() then
           VARS.walkAnim = anim:get("walk")
       end
   end
   ```

5. **Compare handles with == operator:**
   ```lua
   if currentAnim == VARS.idleAnim then
       -- Same animation
   end
   
   -- Also works with ~= (not equal)
   if targetHandle ~= NODE:handle() then
       -- Different nodes
   end
   ```

6. **Check handle validity before use:**
   ```lua
   if VARS.targetNode:valid() then
       local target = SCENE:node(VARS.targetNode)
       -- Use target
   end
   ```

---

## Debugging

Use `print()` liberally to debug your scripts:

```lua
print("Position:", pos.x, pos.y, pos.z)
print("Animation playing:", anim:isPlaying())
print("Delta time:", deltaTime)
```

All print output appears in the engine's debug log window.

---

*End of Lua Scripting API Guide*
