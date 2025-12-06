# Array/Vector API Reference

## Overview
The script system now supports arrays (vectors) of all supported types. Arrays are represented as Lua tables and can be used in VARS, LOCALS, and GLOBALS.

## Supported Array Types
- `vector<float>` - Array of floating-point numbers
- `vector<int>` - Array of integers
- `vector<bool>` - Array of booleans
- `vector<vec2>` - Array of 2D vectors
- `vector<vec3>` - Array of 3D vectors
- `vector<vec4>` - Array of 4D vectors (including quaternions)
- `vector<string>` - Array of strings
- `vector<handle>` - Array of node handles

## Creating Arrays

### In VARS/LOCALS/GLOBALS Tables
```lua
VARS = {
    -- Simple initialization with values
    myNumbers = {1.0, 2.0, 3.0},
    myVectors = {Vec3(0, 0, 0), Vec3(1, 1, 1)},
    
    -- Empty array (will be typed on first use)
    emptyArray = {}
}
```

### Using Array() Constructor (Optional)
```lua
-- Array(type, ...values)
local myArray = Array("float", 1.0, 2.0, 3.0)
local vecArray = Array("vec3", Vec3(0, 0, 0), Vec3(1, 1, 1))
```

## Array Operations

### array:push(value)
Add an element to the end of the array.
```lua
VARS.myNumbers:push(4.0)
-- myNumbers is now {1.0, 2.0, 3.0, 4.0}
```

### array:pop() -> value
Remove and return the last element.
```lua
local last = VARS.myNumbers:pop()
print(last)  -- 4.0
-- myNumbers is now {1.0, 2.0, 3.0}
```

### array:size() -> int
Get the number of elements in the array.
```lua
local count = VARS.myNumbers:size()
print(count)  -- 3
```

### array:insert(index, value)
Insert an element at a specific position (1-based indexing).
```lua
VARS.myNumbers:insert(2, 1.5)
-- myNumbers is now {1.0, 1.5, 2.0, 3.0}
```

### array:remove(index) -> value
Remove and return element at specific position.
```lua
local removed = VARS.myNumbers:remove(2)
print(removed)  -- 1.5
-- myNumbers is now {1.0, 2.0, 3.0}
```

### array:clear()
Remove all elements from the array.
```lua
VARS.myNumbers:clear()
-- myNumbers is now {}
```

## Direct Access

### Reading Elements
```lua
local first = VARS.myNumbers[1]  -- Get first element
local second = VARS.myNumbers[2]  -- Get second element
```

### Writing Elements
```lua
VARS.myNumbers[1] = 5.0  -- Set first element
VARS.myVectors[3] = Vec3(10, 20, 30)  -- Set third vector
```

### Iteration
```lua
-- Using array:size()
for i = 1, VARS.myNumbers:size() do
    print(VARS.myNumbers[i])
end

-- Using # operator (standard Lua)
for i = 1, #VARS.myNumbers do
    print(VARS.myNumbers[i])
end

-- Using ipairs (standard Lua)
for i, value in ipairs(VARS.myNumbers) do
    print(i .. ": " .. value)
end
```

## Example Use Cases

### Particle System
```lua
VARS = {
    particlePositions = {},
    particleVelocities = {},
    particleLifetimes = {}
}

function update()
    -- Spawn new particle
    VARS.particlePositions:push(Vec3(0, 0, 0))
    VARS.particleVelocities:push(Vec3(math.random(), math.random(), math.random()))
    VARS.particleLifetimes:push(5.0)
    
    -- Update particles
    for i = 1, VARS.particlePositions:size() do
        local pos = VARS.particlePositions[i]
        local vel = VARS.particleVelocities[i]
        local life = VARS.particleLifetimes[i]
        
        -- Update position
        pos.x = pos.x + vel.x * DELTATIME
        pos.y = pos.y + vel.y * DELTATIME
        pos.z = pos.z + vel.z * DELTATIME
        VARS.particlePositions[i] = pos
        
        -- Update lifetime
        life = life - DELTATIME
        VARS.particleLifetimes[i] = life
        
        -- Remove dead particles
        if life <= 0 then
            VARS.particlePositions:remove(i)
            VARS.particleVelocities:remove(i)
            VARS.particleLifetimes:remove(i)
        end
    end
end
```

### Waypoint System
```lua
VARS = {
    waypoints = {Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(10, 10, 0)},
    currentWaypoint = 1
}

function update()
    local transform = NODE:transform3D()
    local pos = transform:getPos()
    local target = VARS.waypoints[VARS.currentWaypoint]
    
    -- Move towards waypoint
    local dir = Vec3(
        target.x - pos.x,
        target.y - pos.y,
        target.z - pos.z
    )
    
    -- Check if reached
    local distSq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z
    if distSq < 0.1 then
        VARS.currentWaypoint = VARS.currentWaypoint + 1
        if VARS.currentWaypoint > VARS.waypoints:size() then
            VARS.currentWaypoint = 1  -- Loop back
        end
    end
end
```

### Node Collection Manager
```lua
VARS = {
    enemyNodes = {},
    collectibleNodes = {}
}

function update()
    -- Process all enemy nodes
    for i = 1, VARS.enemyNodes:size() do
        local enemyHandle = VARS.enemyNodes[i]
        local enemy = SCENE:node(enemyHandle)
        if enemy then
            -- Do something with enemy
        else
            -- Node was deleted, remove from array
            VARS.enemyNodes:remove(i)
        end
    end
end
```

## Notes
- Arrays use 1-based indexing (standard Lua convention)
- Arrays automatically sync between C++ and Lua on each update
- Type consistency is maintained - all elements should be the same type
- Empty arrays are valid and will adopt type on first element addition
