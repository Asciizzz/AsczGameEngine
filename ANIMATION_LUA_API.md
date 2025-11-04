# Animation Lua API Reference

Based on the legacy `haveFun()` function, these bindings allow full animation control from Lua scripts.

## Available Functions

### `getAnimHandle(nodeHandle, animName)`
Get an animation handle by name.
- **Parameters:**
  - `nodeHandle`: The node handle (usually `__nodeHandle`)
  - `animName`: String name of the animation (e.g., "Walking_A", "Idle")
- **Returns:** Animation handle table `{index, version}` or `nil` if not found

```lua
local walkHandle = getAnimHandle(__nodeHandle, "Walking_A")
local idleHandle = getAnimHandle(__nodeHandle, "Idle")
```

---

### `getCurAnimHandle(nodeHandle)`
Get the currently playing animation handle.
- **Parameters:**
  - `nodeHandle`: The node handle
- **Returns:** Animation handle table `{index, version}` or `nil`

```lua
local curHandle = getCurAnimHandle(__nodeHandle)
```

---

### `playAnim(nodeHandle, animHandle, restart)`
Play an animation.
- **Parameters:**
  - `nodeHandle`: The node handle
  - `animHandle`: The animation handle to play
  - `restart`: (optional, default `true`) Whether to restart the animation from the beginning
- **Returns:** Nothing

```lua
playAnim(__nodeHandle, walkHandle, true)  -- Play walk animation, restart
playAnim(__nodeHandle, idleHandle, false) -- Play idle, don't restart if already playing
```

---

### `setAnimSpeed(nodeHandle, speed)`
Set the animation playback speed.
- **Parameters:**
  - `nodeHandle`: The node handle
  - `speed`: Speed multiplier (1.0 = normal, 2.0 = double speed, 0.5 = half speed)
- **Returns:** Nothing

```lua
setAnimSpeed(__nodeHandle, 2.0)  -- Play at double speed
setAnimSpeed(__nodeHandle, 1.0)  -- Normal speed
```

---

### `isAnimPlaying(nodeHandle)`
Check if an animation is currently playing.
- **Parameters:**
  - `nodeHandle`: The node handle
- **Returns:** Boolean (`true` if playing, `false` if paused/stopped)

```lua
if isAnimPlaying(__nodeHandle) then
    -- Animation is playing
end
```

---

### `animHandlesEqual(handle1, handle2)`
Compare two animation handles for equality.
- **Parameters:**
  - `handle1`: First animation handle
  - `handle2`: Second animation handle
- **Returns:** Boolean (`true` if equal, `false` otherwise)

```lua
local curHandle = getCurAnimHandle(__nodeHandle)
if animHandlesEqual(curHandle, walkHandle) then
    -- Currently playing walk animation
end
```

---

## Complete Example (Legacy haveFun() Equivalent)

```lua
function vars()
    return {
        -- Add your variables here
    }
end

function update()
    -- Get animation handles
    local idleHandle = getAnimHandle(__nodeHandle, "Idle")
    local walkHandle = getAnimHandle(__nodeHandle, "Walking_A")
    local runHandle = getAnimHandle(__nodeHandle, "Running_A")
    local curHandle = getCurAnimHandle(__nodeHandle)
    
    -- Check movement input
    local isMoving = (kState("w") or kState("a") or kState("s") or kState("d"))
    local running = kState("shift")
    local moveSpeed = running and 4.0 or 1.0
    
    -- Set animation speed
    setAnimSpeed(__nodeHandle, isMoving and moveSpeed or 1.0)
    
    if isMoving then
        -- Choose run or walk based on shift key
        local playHandle = running and runHandle or walkHandle
        
        -- Only restart when switching FROM idle TO walk/run
        -- Don't restart when switching between walk and run
        local shouldRestart = not (animHandlesEqual(curHandle, runHandle) or 
                                   animHandlesEqual(curHandle, walkHandle))
        playAnim(__nodeHandle, playHandle, shouldRestart)
    else
        -- Play idle animation, only restart if not already idle
        local shouldRestart = not animHandlesEqual(curHandle, idleHandle)
        playAnim(__nodeHandle, idleHandle, shouldRestart)
    end
end
```

---

## Notes

- All animation handles are tables with `{index, version}` fields
- `__nodeHandle` is automatically available in the `update()` function
- Animation names must match exactly with your model's animation names
- Use `animHandlesEqual()` to compare handles, not `==` (tables compare by reference)
- Set `restart = false` when you want smooth transitions without resetting the animation
