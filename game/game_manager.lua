-- Game Manager Script
-- Spawns enemies at the border of a bounding box
-- Manages game state and wave progression

VARS = {
    -- Player reference
    playerHandle = Handle("node"),  -- Must be set to the player node
    
    -- Enemy spawning
    enemyScene = Handle("scene"),  -- Enemy scene to instantiate
    enemyContainerNode = Handle("node"),  -- Parent node for all enemies
    
    -- Spawn settings
    spawnRate = 5.0,  -- Time between spawns (seconds)
    spawnTimer = 5.0,  -- Start with delay so player has time to prepare!
    
    -- Spawn distance from player (radial)
    minSpawnRadius = 10.0,  -- Minimum distance from player
    maxSpawnRadius = 15.0,  -- Maximum distance from player
    
    -- Enemy stats progression
    baseEnemyHP = 50.0,
    baseEnemySpeed = 1.5,
    baseEnemyDamage = 5.0,
    
    -- Wave system
    currentWave = 1,
    enemiesSpawned = 0,
    enemiesPerWave = 3,  -- Start with just 3 enemies per wave
    
    -- Difficulty scaling
    hpScaling = 1.2,      -- HP multiplier per wave
    speedScaling = 1.05,  -- Speed multiplier per wave
    damageScaling = 1.1,  -- Damage multiplier per wave
    
    -- Game state
    isGameActive = true,
    totalEnemiesKilled = 0,
}

-- Helper function to spawn enemy at random distance from player
function spawnEnemy()
    if not VARS.enemyScene:valid() or not VARS.enemyContainerNode:valid() then
        print("GameManager: Cannot spawn enemy - invalid scene or container!")
        return
    end
    
    if not VARS.playerHandle:valid() then
        print("GameManager: Cannot spawn enemy - invalid player handle!")
        return
    end
    
    -- Get player position
    local player = SCENE:node(VARS.playerHandle)
    if not player then
        print("GameManager: Cannot get player node!")
        return
    end
    
    local playerT3d = player:transform3D()
    if not playerT3d then
        print("GameManager: Player has no transform!")
        return
    end
    
    local playerPos = playerT3d:getPos()
    
    -- Generate random direction (angle in radians)
    local angle = math.random() * 2 * math.pi
    
    -- Generate random distance within min-max range
    local distance = VARS.minSpawnRadius + math.random() * (VARS.maxSpawnRadius - VARS.minSpawnRadius)
    
    -- Calculate spawn position
    local spawnX = playerPos.x + math.cos(angle) * distance
    local spawnZ = playerPos.z + math.sin(angle) * distance
    
    -- Spawn enemy
    local enemyNode = SCENE:addScene(VARS.enemyScene, VARS.enemyContainerNode)
    
    if enemyNode then
        -- Set enemy position
        local enemyT3d = enemyNode:transform3D()
        if enemyT3d then
            enemyT3d:setPos({x = spawnX, y = 0, z = spawnZ})
        end
        
        -- Configure enemy stats based on current wave
        local enemyScript = enemyNode:script()
        if enemyScript then
            local waveMultiplier = VARS.currentWave - 1
            
            enemyScript:setVar("playerHandle", VARS.playerHandle)
            enemyScript:setVar("hp", VARS.baseEnemyHP * (VARS.hpScaling ^ waveMultiplier))
            enemyScript:setVar("maxHp", VARS.baseEnemyHP * (VARS.hpScaling ^ waveMultiplier))
            enemyScript:setVar("moveSpeed", VARS.baseEnemySpeed * (VARS.speedScaling ^ waveMultiplier))
            enemyScript:setVar("damage", VARS.baseEnemyDamage * (VARS.damageScaling ^ waveMultiplier))
        end
        
        VARS.enemiesSpawned = VARS.enemiesSpawned + 1
        print("GameManager: Spawned enemy #" .. VARS.enemiesSpawned .. " (Wave " .. VARS.currentWave .. ") at (" .. spawnX .. ", " .. spawnZ .. ")")
    end
end

function update()
    if not VARS.isGameActive then
        return
    end
    
    -- Check if player is still alive
    if VARS.playerHandle:valid() then
        local player = SCENE:node(VARS.playerHandle)
        if player then
            local playerScript = player:script()
            if playerScript then
                local playerHP = playerScript:getVar("hp")
                if playerHP and playerHP <= 0 then
                    VARS.isGameActive = false
                    print("======================")
                    print("GAME OVER!")
                    print("Wave reached: " .. VARS.currentWave)
                    print("Enemies killed: " .. VARS.totalEnemiesKilled)
                    print("======================")

                    -- Delete all remaining enemies
                    local enemyContainer = SCENE:node(VARS.enemyContainerNode)
                    if enemyContainer then
                        local enemyChildren = enemyContainer:childrenHandles()
                        for _, enemyNode in ipairs(enemyChildren) do
                            if enemyNode then SCENE:delete(enemyNode) end
                        end
                    end
                    return
                end
            end
        end
    end
    
    -- Update spawn timer
    VARS.spawnTimer = VARS.spawnTimer + DTIME
    
    -- Spawn enemy when timer is ready
    if VARS.spawnTimer >= VARS.spawnRate then
        spawnEnemy()
        VARS.spawnTimer = 0.0
        
        -- Check if we should advance to next wave
        if VARS.enemiesSpawned >= VARS.enemiesPerWave * VARS.currentWave then
            VARS.currentWave = VARS.currentWave + 1
            print("======================")
            print("WAVE " .. VARS.currentWave .. " STARTING!")
            print("======================")
            
            -- Optionally increase spawn rate for higher waves
            if VARS.currentWave % 3 == 0 then
                VARS.spawnRate = math.max(2.0, VARS.spawnRate - 0.5)
                print("Spawn rate increased! Now: " .. VARS.spawnRate .. "s")
            end
        end
    end
    
    -- Count alive enemies (for statistics)
    if VARS.enemyContainerNode:valid() then
        local enemyContainer = SCENE:node(VARS.enemyContainerNode)
        if enemyContainer then
            local enemyChildren = enemyContainer:children()
            local aliveCount = 0
            for _, enemyNode in ipairs(enemyChildren) do
                if enemyNode then
                    aliveCount = aliveCount + 1
                end
            end
            
            -- Calculate killed enemies
            local expectedAlive = VARS.enemiesSpawned
            VARS.totalEnemiesKilled = expectedAlive - aliveCount
        end
    end
end
