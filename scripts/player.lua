-- Character Controller with Enemy Detection
-- This script demonstrates the new hierarchy features:
--   - node:children() - Get all child nodes
--   - node:parent() - Get parent node
--   - nodeHandle global - Current node's handle
--
-- Nodes:
--   rootNode: Movement and rotation
--   animeNode: Animation playback
--   enemiesNode: Parent of all enemy nodes (loop through children!)
-- 
-- Drag node handles from scene hierarchy into the fields below

function vars()
    return {
        -- Node references (drag nodes from scene hierarchy)
        rootNode = nHandle(0xFFFFFFFF, 0xFFFFFFFF),   -- Root node for movement
        animeNode = nHandle(0xFFFFFFFF, 0xFFFFFFFF),  -- Animation node
        enemiesNode = nHandle(0xFFFFFFFF, 0xFFFFFFFF), -- Parent node containing all enemies

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
end

function update()
    -- Get node references
    local root = scene:node(vars.rootNode)
    local anime = scene:node(vars.animeNode)

    -- ========== HEALTH MANAGEMENT ==========
    -- Clamp HP
    if vars.hp < 0 then
        vars.hp = 0
    end
    if vars.hp > vars.maxHp then
        vars.hp = vars.maxHp
    end

    -- Check for death
    if vars.hp <= 0 and not vars.isDead then
        vars.isDead = true
        print("Player died!")
    end

    -- If dead, play death animation (non-looping) and return early
    if vars.isDead and anime then
        local anim3d = anime:anim3D()
        if anim3d then
            local deathHandle = anim3d:get(vars.deathAnim)
            local curHandle = anim3d:current()
            
            -- Only play death animation once
            if not handleEqual(curHandle, deathHandle) then
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
    if root and vars.isPlayer and not vars.isDead then
        local rootT3D = root:transform3D()
        if rootT3D then
            local playerPos = rootT3D:getPos()
            local enemiesContainer = scene:node(vars.enemiesNode)
            
            if enemiesContainer then
                -- Get all enemy children using the new children() method!
                local enemies = enemiesContainer:children()
                
                -- Loop through each enemy and check distance
                for i = 1, #enemies do
                    local enemy = enemies[i]
                    
                    -- Skip collision check with self!
                    if enemy ~= node then
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
                            if distance <= vars.attackRange then
                                -- Deal damage over time to player (enemies hurt the player)
                                vars.hp = vars.hp - vars.attackDamage * dTime
                                
                                -- Optional: Visual feedback - make enemy flash or scale
                                local scale = enemyT3D:getScl()
                                -- Pulse effect: slightly scale up when near player
                                local pulse = 1.0 + 0.1 * math.sin(dTime * 10.0)
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
    local moveSpeed = ((running and isMoving) and 4.0 or 1.0) * vars.vel
    
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
            pos.x = pos.x + moveDir.x * moveSpeed * dTime
            pos.z = pos.z + moveDir.z * moveSpeed * dTime
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
            local idleHandle = anim3d:get(vars.idleAnim)
            local walkHandle = anim3d:get(vars.walkAnim)
            local runHandle = anim3d:get(vars.runAnim)
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
                
                -- Only restart when switching from idle to walk/run
                local shouldRestart = not (handleEqual(curHandle, runHandle) or 
                                           handleEqual(curHandle, walkHandle))
                anim3d:play(playHandle, shouldRestart)
            else
                -- Play idle
                local shouldRestart = not handleEqual(curHandle, idleHandle)
                anim3d:play(idleHandle, shouldRestart)
            end
        end
    end
end