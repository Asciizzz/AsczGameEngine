-- Smooth Follower Script using Second Order Dynamics with QUATERNION rotation
-- Attach this to a node that should smoothly follow a target node
-- 
-- Second Order Dynamic System:
-- y + k1*y' + k2*y'' = x + k3*x'
-- 
-- Parameters:
-- f (frequency) - Speed of response (higher = faster, lower = slower)
-- z (damping) - Bounciness (< 1 = bouncy, 1 = critical, > 1 = smooth)
-- r (response) - Initial reaction (negative = delayed, 0 = neutral, positive = overshoot)

function vars()
    return {
        -- Target to follow
        targetNode = Handle("node"),
        
        -- Animation node (optional - if set, will play walk/run/idle animations)
        animeNode = Handle("node"),
        
        -- Second Order Dynamics Parameters
        frequency = 2.0,      -- How fast to follow (higher = faster)
        damping = 1.0,        -- Damping coefficient (1.0 = critically damped, no overshoot)
        response = 0.0,       -- Initial response (-2 to 2, negative = anticipate, positive = overshoot)
        
        -- Follow settings
        followPosition = true,
        heightOffset = 0.0,   -- Y offset from target (useful for cameras)
        orientToTarget = true,  -- Auto-rotate to face the target
        rotationSpeed = 5.0,    -- How fast to rotate (higher = faster)
        
        -- Animation settings
        idleAnim = "Idle_Loop",
        walkAnim = "Walk_Loop",
        runAnim = "Sprint_Loop",
        runThreshold = 2.0,   -- Minimum speed to count as running (vs walking)
        idleThreshold = 0.001, -- Maximum speed to count as idle
        
        -- Internal state (position)
        posY = { x = 0, y = 0, z = 0 },      -- Current output position
        posYd = { x = 0, y = 0, z = 0 },     -- Current velocity
        posXp = { x = 0, y = 0, z = 0 },     -- Previous target position
        
        -- Internal state (rotation - QUATERNION)
        currentQuat = { x = 0, y = 0, z = 0, w = 1 }, -- Current rotation quaternion
        
        -- Computed constants (updated when f, z, r change)
        k1 = 0.0,
        k2 = 0.0,
        k3 = 0.0,
        
        -- Track if we need to update constants
        lastF = 0.0,
        lastZ = 0.0,
        lastR = 0.0,
        
        -- Initialization flag
        initialized = false
    }
end

-- Update k1, k2, k3 constants from f, z, r
function updateConstants()
    local pi = 3.14159265359
    
    VARS.k1 = VARS.damping / (pi * VARS.frequency)
    VARS.k2 = (2 * pi * VARS.frequency) * (2 * pi * VARS.frequency)
    VARS.k3 = VARS.response * VARS.damping / (2 * pi * VARS.frequency)
    
    VARS.lastF = VARS.frequency
    VARS.lastZ = VARS.damping
    VARS.lastR = VARS.response
end

-- Initialize positions from current node position
function initializeState()
    local t3d = NODE:transform3D()
    if not t3d then return end
    
    local pos = t3d:getPos()
    local quat = t3d:getQuat()
    
    -- Initialize position state
    VARS.posY = { x = pos.x, y = pos.y, z = pos.z }
    VARS.posYd = { x = 0, y = 0, z = 0 }
    VARS.posXp = { x = pos.x, y = pos.y, z = pos.z }
    
    -- Initialize rotation state with current quaternion
    VARS.currentQuat = { x = quat.x, y = quat.y, z = quat.z, w = quat.w }
    
    updateConstants()
    VARS.initialized = true
end

-- Second order dynamics update for a single axis
function updateAxis(y, yd, x, xp, T)
    -- Calculate velocity of target (finite difference)
    local xd = (x - xp) / T
    
    -- Update position using current velocity
    y = y + T * yd
    
    -- Update velocity using second order dynamics
    yd = yd + T * (x + VARS.k3 * xd - y - VARS.k1 * yd) * VARS.k2
    
    return y, yd, x
end

function update()
    -- Check if constants need updating
    if VARS.frequency ~= VARS.lastF or VARS.damping ~= VARS.lastZ or VARS.response ~= VARS.lastR then
        updateConstants()
    end
    
    -- Initialize on first frame
    if not VARS.initialized then
        initializeState()
    end
    
    local myT3d = NODE:transform3D()
    if not myT3d then
        return
    end
    
    -- Initialize velocity magnitude (for animation even without target)
    local velocityMagnitude = 0.0
    
    -- Check if we have a valid target for following
    if VARS.targetNode:valid() then
        local targetNodeObj = SCENE:node(VARS.targetNode)
        if targetNodeObj then
            local targetT3d = targetNodeObj:transform3D()
            
            if targetT3d then
                -- Get target transform
                local targetPos = targetT3d:getPos()
                
                -- Apply height offset
                targetPos.y = targetPos.y + VARS.heightOffset
                
                -- Update position using second order dynamics
                if VARS.followPosition then
                    VARS.posY.x, VARS.posYd.x, VARS.posXp.x = updateAxis(
                        VARS.posY.x, VARS.posYd.x, targetPos.x, VARS.posXp.x, DTIME
                    )
                    VARS.posY.y, VARS.posYd.y, VARS.posXp.y = updateAxis(
                        VARS.posY.y, VARS.posYd.y, targetPos.y, VARS.posXp.y, DTIME
                    )
                    VARS.posY.z, VARS.posYd.z, VARS.posXp.z = updateAxis(
                        VARS.posY.z, VARS.posYd.z, targetPos.z, VARS.posXp.z, DTIME
                    )
                    
                    myT3d:setPos(VARS.posY)
                end
                
                -- Calculate current velocity magnitude for animation
                velocityMagnitude = math.sqrt(
                    VARS.posYd.x * VARS.posYd.x + 
                    VARS.posYd.y * VARS.posYd.y + 
                    VARS.posYd.z * VARS.posYd.z
                )
                
                -- Smooth quaternion-based rotation to face the target
                if VARS.orientToTarget then
                    -- Get current position
                    local myPos = myT3d:getPos()
                    
                    -- Calculate direction vector from follower to target
                    local dirX = targetPos.x - myPos.x
                    local dirZ = targetPos.z - myPos.z
                    local dirLength = math.sqrt(dirX * dirX + dirZ * dirZ)
                    
                    -- Only rotate if we're not basically on top of the target
                    if dirLength > 0.01 then
                        -- Create forward and up vectors for lookAt
                        -- Assuming -Z is forward in your engine (common convention)
                        local forward = { x = dirX / dirLength, y = 0, z = dirZ / dirLength }
                        local up = { x = 0, y = 1, z = 0 }
                        
                        -- Create target quaternion using lookAt
                        local targetQuat = quat_lookAt(forward, up)
                        
                        -- Spherical linear interpolation (slerp) for smooth rotation
                        local t = math.min(1.0, VARS.rotationSpeed * DTIME)
                        VARS.currentQuat = quat_slerp(VARS.currentQuat, targetQuat, t)
                        
                        -- Apply the interpolated rotation
                        myT3d:setQuat(VARS.currentQuat)
                    end
                end
            end
        end
    end
    
    -- ========== ANIMATION SYSTEM ==========
    if VARS.animeNode:valid() then
        local animeNodeObj = SCENE:node(VARS.animeNode)
        if animeNodeObj then
            local anim3d = animeNodeObj:anim3D()
            if anim3d then
                -- Get animation handles
                local idleHandle = anim3d:get(VARS.idleAnim)
                local walkHandle = anim3d:get(VARS.walkAnim)
                local runHandle = anim3d:get(VARS.runAnim)
                local curHandle = anim3d:current()
                
                -- Ensure looping is enabled
                if not anim3d:isLoop() then
                    anim3d:setLoop(true)
                end
                
                -- Determine which animation to play based on velocity
                local playHandle = nil
                local animSpeed = 1.0
                
                if velocityMagnitude < VARS.idleThreshold then
                    -- Idle
                    playHandle = idleHandle
                    animSpeed = 1.0
                elseif velocityMagnitude < VARS.runThreshold then
                    -- Walking
                    playHandle = walkHandle
                    -- Scale animation speed with velocity (relative to run threshold)
                    animSpeed = velocityMagnitude / VARS.runThreshold
                    animSpeed = math.max(0.5, animSpeed) -- Clamp minimum speed
                else
                    -- Running
                    playHandle = runHandle
                    -- Scale animation speed with velocity (above run threshold)
                    animSpeed = velocityMagnitude / VARS.runThreshold
                    animSpeed = math.min(2.0, animSpeed) -- Clamp maximum speed
                end
                
                -- Set animation speed based on current velocity
                anim3d:setSpeed(animSpeed)
                
                -- Only restart animation when switching between different animations
                local shouldRestart = (curHandle ~= playHandle)
                if playHandle then
                    anim3d:play(playHandle, shouldRestart)
                end
            end
        end
    end
end
