
VARS = {
    -- Node references using unified Handle system
    -- Use Handle("node", index, version) to create node handles
    rootNode = Handle("node", 0xFFFFFFFF, 0xFFFFFFFF),   -- Root node for movement
    animeNode = Handle("node", 0xFFFFFFFF, 0xFFFFFFFF),  -- Animation node
    enemiesNode = Handle("node", 0xFFFFFFFF, 0xFFFFFFFF), -- Parent node containing all enemies

    -- Stats
    isPlayer = true,
    vel = 2.0,
    hp = 100.0,
    maxHp = 100.0,
    isDead = false,
    
    -- Combat
    attackRange = 1.0,  -- Distance at which player can hit enemies
    attackDamage = 10.0,

    -- Animation names (configure for your model)
    idleAnim = "Idle_Loop",
    walkAnim = "Walk_Loop",
    runAnim = "Sprint_Loop",
    deathAnim = "Death01"  -- Death animation (non-looping)
}

function update()
    -- Get node references
    local root = SCENE:node(VARS.rootNode)
    local anime = SCENE:node(VARS.animeNode)

    -- ========== HEALTH MANAGEMENT ==========
    -- Clamp HP
    if VARS.hp < 0 then
        VARS.hp = 0
    end
    if VARS.hp > VARS.maxHp then
        VARS.hp = VARS.maxHp
    end

    -- Check for death
    if VARS.hp <= 0 and not VARS.isDead then
        VARS.isDead = true
        print("Player died!")
    end

    -- If dead, play death animation (non-looping) and return early
    if VARS.isDead and anime then
        local anim3d = anime:anim3D()
        if anim3d then
            local deathHandle = anim3d:get(VARS.deathAnim)
            local curHandle = anim3d:current()
            
            -- Only play death animation once (using == operator via __eq metamethod)
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
        
        return  -- Don't process movement or other logic while dead
    end

    -- ========== ENEMY COLLISION & COMBAT ==========
    if root and VARS.isPlayer and not VARS.isDead then
        local rootT3D = root:transform3D()
        if rootT3D then
            local playerPos = rootT3D:getPos()
            local enemiesContainer = SCENE:node(VARS.enemiesNode)
            
            if enemiesContainer then
                -- Get all enemy children using the new children() method!
                local enemies = enemiesContainer:children()
                
                -- Loop through each enemy and check distance
                for i = 1, #enemies do
                    local enemy = enemies[i]
                    
                    -- Skip collision check with self!
                    if enemy ~= NODE then
                        local enemyT3D = enemy:transform3D()
                        if enemyT3D then
                            local enemyPos = enemyT3D:getPos()
                            
                            -- Calculate distance to enemy
                            local dx = playerPos.x - enemyPos.x
                            local dy = playerPos.y - enemyPos.y
                            local dz = playerPos.z - enemyPos.z
                            local distSq = dx * dx + dy * dy + dz * dz
                            local distance = math.sqrt(distSq)
                            
                            -- If within attack range, deal damage
                            if distance <= VARS.attackRange then
                                -- Deal damage over time to player (enemies hurt the player)
                                VARS.hp = VARS.hp - VARS.attackDamage * DTIME
                                
                                -- Optional: Visual feedback - make enemy flash or scale
                                local scale = enemyT3D:getScl()
                                -- Pulse effect: slightly scale up when near player
                                local pulse = 1.0 + 0.1 * math.sin(DTIME * 10.0)
                                enemyT3D:setScl({x = pulse, y = pulse, z = pulse})
                            end
                        end
                    end
                end
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
    local vx = (k_right and -1 or 0) - (k_left and -1 or 0) -- I really need to fix the coord system lol
    local isMoving = (vx ~= 0) or (vz ~= 0)
    local moveSpeed = ((running and isMoving) and 4.0 or 1.0) * VARS.vel
    
    -- ========== MOVEMENT (Root Node) ==========
    if root and isMoving then
        local rootT3D = root:transform3D()
        if rootT3D then
            local pos = rootT3D:getPos()
            
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
            rootT3D:setPos(pos)
            
            -- Apply rotation (face movement direction)
            local targetYaw = math.atan(moveDir.x, moveDir.z)
            rootT3D:setRot({x = 0, y = targetYaw, z = 0})
        end
    end
    
    -- ========== ANIMATION (Anime Node) ==========
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
            
            -- Set animation speed
            anim3d:setSpeed(isMoving and moveSpeed or 1.0)
            
            if isMoving then
                -- Choose run or walk
                local playHandle = running and runHandle or walkHandle
                
                -- Only restart when switching from idle to walk/run (using == via __eq)
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