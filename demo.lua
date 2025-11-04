-- Complete Node/Scene OOP API Demo
-- This demonstrates all the features of the new API

-- ========== Character Controller Example ==========
function vars()
    return {
        -- Movement settings
        moveSpeed = 2.0,
        
        -- Animation names (configurable for different models)
        idleAnim = "Idle_A",
        walkAnim = "Walking_A",
        runAnim = "Running_A",
        
        -- Example: External node reference (drag a node handle from scene)
        targetNode = Handle(0, 0)  -- Invalid handle by default
    }
end

function update()
    -- ========== INPUT ==========
    local k_up = kState("up")
    local k_down = kState("down")
    local k_left = kState("left")
    local k_right = kState("right")
    local running = kState("shift")
    
    -- Calculate movement direction (arrow keys)
    local vz = (k_up and 1 or 0) - (k_down and 1 or 0)
    local vx = (k_left and -1 or 0) - (k_right and -1 or 0)
    
    local isMoving = (vx ~= 0) or (vz ~= 0)
    vars.moveSpeed = (running and isMoving) and 4.0 or 1.0
    
    -- ========== MOVEMENT (for current node) ==========
    if isMoving then
        -- Get current position using OOP syntax
        local pos = node:getPos()
        if pos then
            -- Calculate movement direction
            local moveDir = {x = vx, y = 0, z = vz}
            
            -- Normalize diagonal movement
            local length = math.sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z)
            if length > 0 then
                moveDir.x = moveDir.x / length
                moveDir.z = moveDir.z / length
            end
            
            -- Calculate target yaw (rotation around Y axis)
            local targetYaw = math.atan(moveDir.x, moveDir.z)
            
            -- Apply movement
            pos.x = pos.x + moveDir.x * vars.moveSpeed * dTime
            pos.z = pos.z + moveDir.z * vars.moveSpeed * dTime
            node:setPos(pos)
            
            -- Apply rotation
            node:setRot({x = 0, y = targetYaw, z = 0})
        end
    end
    
    -- ========== ANIMATION (for current node) ==========
    -- Get animation handles by name (configurable)
    local idleHandle = node:getAnimHandle(vars.idleAnim)
    local walkHandle = node:getAnimHandle(vars.walkAnim)
    local runHandle = node:getAnimHandle(vars.runAnim)
    local curHandle = node:getCurAnimHandle()
    
    -- Set animation speed
    node:setAnimSpeed(isMoving and vars.moveSpeed or 1.0)
    
    if isMoving then
        -- Choose run or walk animation
        local playHandle = running and runHandle or walkHandle
        
        -- Only restart when switching FROM idle TO walk/run
        local shouldRestart = not (handleEqual(curHandle, runHandle) or 
                                   handleEqual(curHandle, walkHandle))
        node:playAnim(playHandle, shouldRestart)
    else
        -- Switch to idle
        local shouldRestart = not handleEqual(curHandle, idleHandle)
        node:playAnim(idleHandle, shouldRestart)
    end
    
    -- ========== EXTERNAL NODE MANIPULATION ==========
    -- Example: Get an external node and manipulate it
    if vars.targetNode then
        local target = scene:getNode(vars.targetNode)
        if target then
            -- Do something with the target node
            -- (This is just an example - the actual logic is in the spinner test)
        end
    end
end

-- ========== API Reference ==========
--[[
    Node Methods:
        node:getPos() -> {x, y, z}
        node:setPos({x, y, z})
        node:getRot() -> {x, y, z} (Euler angles)
        node:setRot({x, y, z})
        node:getScl() -> {x, y, z}
        node:setScl({x, y, z})
        node:getAnimHandle(name) -> animHandle or nil
        node:getCurAnimHandle() -> animHandle or nil
        node:playAnim(animHandle, restart)
        node:setAnimSpeed(speed)
        node:isAnimPlaying() -> boolean
    
    Scene Methods:
        scene:getNode(handle) -> Node object
    
    Global Functions:
        kState(keyName) -> boolean
        Handle(index, version) -> handle (for vars())
        handleEqual(h1, h2) -> boolean
        animHandlesEqual(h1, h2) -> boolean (legacy alias)
    
    Global Variables:
        node - The current node (Node object)
        scene - The current scene (Scene object)
        vars - Runtime variables table
        dTime - Delta time (seconds)
]]
