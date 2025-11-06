-- Collision Teleport Script
-- Attach this to a node that should teleport when another node gets too close
-- Perfect for tag games, chase scenarios, or infinite running demos!

VARS = {
    -- The chaser node (e.g., the follower trying to catch this node)
    chaserNode = Handle("node"),
        
        -- Collision settings
        collisionRadius = 2.0,  -- Distance threshold for collision detection
        
        -- Teleport boundary (rectangular area on XZ plane)
        boundaryMinX = -20.0,
        boundaryMaxX = 20.0,
        boundaryMinZ = -20.0,
        boundaryMaxZ = 20.0,
        
        -- Height settings
        keepCurrentHeight = true,  -- If true, maintains Y position after teleport
        teleportHeight = 0.0,      -- If keepCurrentHeight is false, use this Y value
        
        -- Minimum distance from chaser when teleporting
        minDistanceFromChaser = 5.0,  -- Won't teleport too close to the chaser
        
        -- Visual feedback (optional)
        showDebugInfo = true,
        
    -- Internal state
    lastCollisionTime = 0.0,
    totalCollisions = 0,
    currentTime = 0.0
}-- Generate a random float between min and max
function randomFloat(min, max)
    return min + (max - min) * math.random()
end

-- Calculate 3D distance between two positions
function distance3D(pos1, pos2)
    local dx = pos2.x - pos1.x
    local dy = pos2.y - pos1.y
    local dz = pos2.z - pos1.z
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

-- Calculate 2D distance (XZ plane only)
function distance2D(pos1, pos2)
    local dx = pos2.x - pos1.x
    local dz = pos2.z - pos1.z
    return math.sqrt(dx * dx + dz * dz)
end

-- Generate a random position within bounds, far enough from chaser
function generateRandomPosition(chaserPos, currentY)
    local maxAttempts = 20
    local attempts = 0
    
    while attempts < maxAttempts do
        -- Generate random position
        local newPos = {
            x = randomFloat(VARS.boundaryMinX, VARS.boundaryMaxX),
            y = VARS.keepCurrentHeight and currentY or VARS.teleportHeight,
            z = randomFloat(VARS.boundaryMinZ, VARS.boundaryMaxZ)
        }
        
        -- Check if it's far enough from chaser
        local dist = distance2D(newPos, chaserPos)
        if dist >= VARS.minDistanceFromChaser then
            return newPos
        end
        
        attempts = attempts + 1
    end
    
    -- Fallback: just pick a random position even if close
    return {
        x = randomFloat(VARS.boundaryMinX, VARS.boundaryMaxX),
        y = VARS.keepCurrentHeight and currentY or VARS.teleportHeight,
        z = randomFloat(VARS.boundaryMinZ, VARS.boundaryMaxZ)
    }
end

function update()
    VARS.currentTime = VARS.currentTime + DTIME
    
    local myT3d = NODE:transform3D()
    if not myT3d then
        return
    end
    
    local myPos = myT3d:getPos()
    
    -- Check if we have a valid chaser
    if not VARS.chaserNode:valid() then
        if VARS.showDebugInfo then
            print("Collision Teleport: No chaser node set!")
        end
        return
    end
    
    local chaserNodeObj = SCENE:node(VARS.chaserNode)
    if not chaserNodeObj then
        return
    end
    
    local chaserT3d = chaserNodeObj:transform3D()
    if not chaserT3d then
        return
    end
    
    local chaserPos = chaserT3d:getPos()
    
    -- Calculate distance between nodes
    local dist = distance3D(myPos, chaserPos)
    
    -- Check for collision
    if dist <= VARS.collisionRadius then
        -- Collision detected! Teleport to random position
        VARS.totalCollisions = VARS.totalCollisions + 1
        VARS.lastCollisionTime = VARS.currentTime
        
        -- Generate new random position
        local newPos = generateRandomPosition(chaserPos, myPos.y)
        
        -- Teleport!
        myT3d:setPos(newPos)
        
        if VARS.showDebugInfo then
            print(string.format("TELEPORT! Collision #%d - Distance: %.2f - New pos: (%.1f, %.1f, %.1f)", 
                VARS.totalCollisions, dist, newPos.x, newPos.y, newPos.z))
        end
    else
        -- Optional: Print distance info periodically (every 2 seconds)
        if VARS.showDebugInfo and math.floor(VARS.currentTime) % 2 == 0 and VARS.currentTime - VARS.lastCollisionTime > 0.5 then
            -- Only print once per 2-second interval
            local timeKey = math.floor(VARS.currentTime / 2)
            if not VARS.lastPrintTime or VARS.lastPrintTime ~= timeKey then
                VARS.lastPrintTime = timeKey
                print(string.format("Distance to chaser: %.2f / %.2f", dist, VARS.collisionRadius))
            end
        end
    end
end
