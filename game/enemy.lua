-- Enemy Script - Chases and damages player on collision
-- This enemy is a complete scene with its own transform

VARS = {
    -- Target
    playerHandle = Handle("node"),  -- Handle to the player node to chase
    
    -- Stats
    moveSpeed = 1.5,     -- Chase speed
    hp = 50.0,           -- Enemy health
    maxHp = 50.0,
    isDead = false,
    
    -- Combat
    damage = 5.0,        -- Damage per second when touching player
    attackCooldown = 0.0,  -- Time remaining until next damage tick
    attackRate = 0.5,    -- Time between damage ticks (seconds)
    
    -- Collision detection
    collisionRadius = 1.0,  -- Distance at which enemy "touches" player
    
    -- Rotation
    currentQuat = { x = 0, y = 0, z = 0, w = 1 },  -- Current rotation quaternion
    rotationSpeed = 5.0  -- How fast to rotate towards player
}

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
        print("Enemy died!")
        -- Delete self using new node:delete() method
        NODE:delete()
        return
    end
    
    -- If dead, don't process further
    if VARS.isDead then return end
    
    -- ========== VALIDATE PLAYER ==========
    if not VARS.playerHandle:valid() then
        -- No player to chase
        return
    end
    
    local player = SCENE:node(VARS.playerHandle)
    if not player then
        -- Player doesn't exist
        return
    end
    
    local playerT3d = player:transform3D()
    if not playerT3d then return end
    
    local playerScript = player:script()
    if not playerScript then return end
    
    -- ========== CHASE PLAYER ==========
    local myPos = myT3d:getPos()
    local playerPos = playerT3d:getPos()
    
    -- Calculate direction to player
    local dirX = playerPos.x - myPos.x
    local dirZ = playerPos.z - myPos.z
    local distanceToPlayer = math.sqrt(dirX * dirX + dirZ * dirZ)
    
    -- Check if close enough to attack
    local isColliding = distanceToPlayer < VARS.collisionRadius
    
    if isColliding then
        -- ========== ATTACK PLAYER ==========
        -- Update attack cooldown
        if VARS.attackCooldown > 0 then
            VARS.attackCooldown = VARS.attackCooldown - DELTATIME
        end
        
        -- Deal damage if cooldown is ready
        if VARS.attackCooldown <= 0 then
            -- Get player's current HP
            local playerHp = playerScript:getVar("hp")
            if playerHp then
                -- Damage player
                playerScript:setVar("hp", playerHp - VARS.damage)
                print("Enemy damaged player! Player HP: " .. (playerHp - VARS.damage))
                
                -- Reset cooldown
                VARS.attackCooldown = VARS.attackRate
            end
        end
    else
        -- ========== MOVE TOWARDS PLAYER ==========
        if distanceToPlayer > 0.01 then
            -- Normalize direction
            dirX = dirX / distanceToPlayer
            dirZ = dirZ / distanceToPlayer
            
            -- Move towards player
            myPos.x = myPos.x + dirX * VARS.moveSpeed * DELTATIME
            myPos.z = myPos.z + dirZ * VARS.moveSpeed * DELTATIME
            myT3d:setPos(myPos)
            
            -- ========== SMOOTH ROTATION TOWARDS PLAYER ==========
            -- Create forward vector for lookAt
            local forward = { x = dirX, y = 0, z = dirZ }
            local up = { x = 0, y = 1, z = 0 }
            
            -- Create target quaternion using lookAt
            local targetQuat = quat_lookAt(forward, up)
            
            -- Spherical linear interpolation (slerp) for smooth rotation
            local t = math.min(1.0, VARS.rotationSpeed * DELTATIME)
            VARS.currentQuat = quat_slerp(VARS.currentQuat, targetQuat, t)
            
            -- Apply the interpolated rotation
            myT3d:setQuat(VARS.currentQuat)
        end
    end
end