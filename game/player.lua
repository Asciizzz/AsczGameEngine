-- Player Script with Smooth Quaternion Rotation
-- This script uses NODE directly to modify its own transformation
-- No animation system - pure movement and combat logic

VARS = {
    -- Node references
    animeNode = Handle("node"),  -- Animation node (optional - child of this node)

    -- Bullet
    bulletContainerNode = Handle("node"),  -- Container for all bullets
    bulletScene = Handle("scene"),  -- Bullet scene to instantiate

    -- Enemies
    enemyContainerNode = Handle("node"), -- Parent node containing all enemies

    -- Stats
    vel = 2.0,
    hp = 100.0,
    maxHp = 100.0,
    isDead = false,
    
    -- Combat
    shotCooldown = 0.0,  -- Time remaining until can shoot again
    shotRate = 0.2,      -- Time between shots (seconds)
    shotDamage = 10.0,
    shotSpeed = 10.0,

    -- Animation names (configure for your model)
    idleAnim = "Idle_Loop",
    walkAnim = "Walk_Loop",
    runAnim = "Sprint_Loop",
    deathAnim = "Death01",  -- Death animation (non-looping)
    shootAnim = "Spell_Simple_Shoot", -- Shooting animation (non-looping)
    
    -- Smooth rotation parameters (quaternion-based)
    rotationSpeed = 8.0,    -- How fast to rotate (higher = faster, smoother with quaternions)
    currentQuat = { x = 0, y = 0, z = 0, w = 1 }, -- Current rotation quaternion
}

function quat_forward(quat)
    -- Calculate forward vector from quaternion (0, 0, 1) rotated by quat
    local qx, qy, qz, qw = quat.x, quat.y, quat.z, quat.w
    return {
        x = 2 * (qx * qz + qw * qy),
        y = 2 * (qy * qz - qw * qx),
        z = 1 - 2 * (qx * qx + qy * qy)
    }
end

function update()
    -- Get own transform
    local myT3d = NODE:transform3D()
    if not myT3d then return end

    -- ========== HEALTH MANAGEMENT ==========
    -- Clamp HP
    if VARS.hp < 0 then VARS.hp = 0 end
    if VARS.hp > VARS.maxHp then VARS.hp = VARS.maxHp end

    -- Check for death
    if VARS.hp <= 0 and not VARS.isDead then
        VARS.isDead = true
        print("Player died!")
    end

    -- If dead, play death animation and don't process movement or combat
    if VARS.isDead then
        -- Play death animation (non-looping)
        if VARS.animeNode:valid() then
            local anime = SCENE:node(VARS.animeNode)
            if anime then
                local anim3d = anime:anim3D()
                if anim3d then
                    local deathHandle = anim3d:get(VARS.deathAnim)
                    local curHandle = anim3d:current()
                    
                    -- Only play death animation once
                    if curHandle ~= deathHandle then
                        anim3d:setLoop(false)  -- Death animation doesn't loop
                        anim3d:play(deathHandle, true)
                    else
                        -- Check if death animation has finished
                        local animTime = anim3d:getTime()
                        local animDuration = anim3d:getDuration()
                        
                        if animTime >= animDuration - 0.01 then
                            -- Death animation finished, pause at last frame
                            anim3d:pause()
                        end
                    end
                end
            end
        end
        return
    end

    -- ========== SHOOTING ==========
    -- Update cooldown
    if VARS.shotCooldown > 0 then
        VARS.shotCooldown = VARS.shotCooldown - DTIME
    end
    
    -- Check for shoot input
    local k_shoot = kState("space")
    if k_shoot and VARS.shotCooldown <= 0 and VARS.bulletScene:valid() and VARS.bulletContainerNode:valid() then
        -- Get bullet container
        local bulletContainer = SCENE:node(VARS.bulletContainerNode)
        if bulletContainer then
            -- Spawn bullet at player position
            local bulletNode = SCENE:addScene(VARS.bulletScene, VARS.bulletContainerNode)
            
            if bulletNode then
                -- Set bullet position to player position
                local bulletT3d = bulletNode:transform3D()
                if bulletT3d then
                    local myPos = myT3d:getPos()
                    bulletT3d:setPos(myPos)
                end
                
                -- Get player forward direction from quaternion
                -- Calculate forward vector from quaternion (0, 0, 1) rotated by quat
                local forward = quat_forward(VARS.currentQuat)
                
                -- Set bullet variables via script
                local bulletScript = bulletNode:script()
                if bulletScript then
                    bulletScript:setVar("active", true)
                    bulletScript:setVar("friendly", true)
                    bulletScript:setVar("dirX", forward.x)
                    bulletScript:setVar("dirZ", forward.z)
                    bulletScript:setVar("speed", VARS.shotSpeed)
                    bulletScript:setVar("damage", VARS.shotDamage)
                    bulletScript:setVar("enemyContainerNode", VARS.enemyContainerNode)
                end
                
                -- Reset cooldown
                VARS.shotCooldown = VARS.shotRate
            end
        end
    end

    -- ========== INPUT DETECTION ==========
    local k_up = kState("up")
    local k_down = kState("down")
    local k_left = kState("left")
    local k_right = kState("right")
    local running = kState("shift")
    
    -- Calculate movement direction
    local vz = (k_up and 1 or 0) - (k_down and 1 or 0)
    local vx = (k_right and -1 or 0) - (k_left and -1 or 0)
    local isMoving = (vx ~= 0) or (vz ~= 0)
    local moveSpeed = ((running and isMoving) and 4.0 or 1.0) * VARS.vel
    
    -- ========== MOVEMENT & ROTATION (Direct on NODE) ==========
    if isMoving then
        local pos = myT3d:getPos()
        
        -- Calculate normalized movement direction
        local moveDir = {x = vx, y = 0, z = vz}
        local length = math.sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z)
        if length > 0 then
            moveDir.x = moveDir.x / length
            moveDir.z = moveDir.z / length
        end
        
        -- Apply movement
        pos.x = pos.x + moveDir.x * moveSpeed * DTIME
        pos.z = pos.z + moveDir.z * moveSpeed * DTIME
        myT3d:setPos(pos)
        
        -- ========== SMOOTH QUATERNION ROTATION ==========
        -- Calculate direction for rotation (face movement direction)
        local dirX = moveDir.x
        local dirZ = moveDir.z
        local dirLength = math.sqrt(dirX * dirX + dirZ * dirZ)
        
        if dirLength > 0.01 then
            -- Create forward and up vectors for lookAt
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
    
    -- ========== ANIMATION (Anime Node) ==========
    if VARS.animeNode:valid() then
        local anime = SCENE:node(VARS.animeNode)
        if anime then
            local anim3d = anime:anim3D()
            if anim3d then
                -- Get animation handles
                local idleHandle = anim3d:get(VARS.idleAnim)
                local walkHandle = anim3d:get(VARS.walkAnim)
                local runHandle = anim3d:get(VARS.runAnim)
                local curHandle = anim3d:current()
                
                -- Ensure looping is enabled for normal animations
                if not anim3d:isLoop() then
                    anim3d:setLoop(true)
                end
                
                -- Set animation speed based on movement speed
                anim3d:setSpeed(isMoving and moveSpeed or 1.0)
                
                if isMoving then
                    -- Choose run or walk based on running state
                    local playHandle = running and runHandle or walkHandle
                    
                    -- Only restart when switching from idle to walk/run
                    local shouldRestart = not (curHandle == runHandle or curHandle == walkHandle)
                    anim3d:play(playHandle, shouldRestart)
                else
                    -- Play idle
                    local shouldRestart = curHandle ~= idleHandle
                    anim3d:play(idleHandle, shouldRestart)
                end
            end
        end
    end
end
