-- Bullet Script - Moves in a direction and damages enemies on contact

VARS = {
    -- Movement
    dirX = 0.0,  -- Direction X (set by spawner)
    dirZ = 0.0,  -- Direction Z (set by spawner)
    speed = 10.0,  -- Movement speed (set by spawner)
    
    -- Combat
    damage = 10.0,  -- Damage dealt (set by spawner)
    friendly = true,  -- Is this a player bullet? (set by spawner)
    
    -- Lifetime
    lifetime = 5.0,  -- Seconds before auto-delete
    age = 0.0,

    -- Enemy node
    enemyContainerNode = Handle("node"),

    -- Active flag
    active = false
}

function update()
    -- Ignore if not active
    if not VARS.active then
        return
    end

    -- Update age
    VARS.age = VARS.age + DELTATIME
    
    -- Auto-delete after lifetime expires
    if VARS.age >= VARS.lifetime then
        SCENE:delete(NODE:handle())
        return
    end
    
    -- Get own transform
    local myT3d = NODE:transform3D()
    if not myT3d then
        return
    end
    
    -- ========== MOVEMENT ==========
    local pos = myT3d:getPos()
    pos.x = pos.x + VARS.dirX * VARS.speed * DELTATIME
    pos.z = pos.z + VARS.dirZ * VARS.speed * DELTATIME
    myT3d:setPos(pos)

    -- ========== COLLISION WITH ENEMIES ==========
    if VARS.enemyContainerNode:valid() then
        local enemyContainer = SCENE:node(VARS.enemyContainerNode)
        if enemyContainer then
            local enemyChildren = enemyContainer:children()
            for _, enemyNode in ipairs(enemyChildren) do
                -- enemyNode is already a Node object, no need to call SCENE:node()
                if enemyNode then
                    local enemyT3d = enemyNode:transform3D()
                    local enemyScript = enemyNode:script()

                    if enemyT3d and enemyScript then
                        local enemyPos = enemyT3d:getPos()
                        local myPos = myT3d:getPos()
                        
                        -- Simple distance check for collision
                        local dx = enemyPos.x - myPos.x
                        local dz = enemyPos.z - myPos.z
                        local distanceSq = dx * dx + dz * dz
                        local collisionRadiusSq = 0.5 * 0.5  -- Assume both have radius 0.5
                        
                        if distanceSq <= collisionRadiusSq then
                            -- Collision detected
                            local currentHp = enemyScript:getVar("hp") or 0
                            currentHp = currentHp - VARS.damage
                            enemyScript:setVar("hp", currentHp)
                            
                            -- Delete bullet after hitting an enemy
                            SCENE:delete(NODE:handle())
                            return
                        end
                    end
                end
            end
        end
    end

end
