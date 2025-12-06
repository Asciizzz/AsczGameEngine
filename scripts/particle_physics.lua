-- Particle Physics Manager Script
-- Attach this to the container NODE (parent of all particles)
-- Simulates physically accurate sphere collisions and boundary constraints

VARS = {
    -- Bounding box (world space)
    boxMin = Vec3(-5.0, 0.0, -5.0),
    boxMax = Vec3(5.0, 10.0, 5.0),
    
    -- Particle properties
    particleRadius = 0.2,        -- Radius of each spherical particle
    particleMass = 1.0,          -- Mass of each particle (kg)
    
    -- Physics parameters
    gravity = Vec3(0.0, -9.81, 0.0),  -- Gravity vector (m/s²)
    damping = 0.995,             -- Velocity damping (0-1, 1 = no damping) - frame-rate independent
    restitution = 0.7,           -- Bounciness for wall collisions (0-1)
    friction = 0.95,             -- Surface friction (0-1, 1 = no friction)
    
    -- Collision properties
    particleRestitution = 0.8,   -- Bounciness for particle-particle collisions
    collisionDamping = 0.99,     -- Energy loss in collisions
    
    -- Simulation settings
    substeps = 4,                -- Physics substeps per frame (higher = more stable)
    enabled = true,              -- Enable/disable simulation
    
    -- Spatial grid optimization
    useGrid = true,              -- Enable spatial grid acceleration
    gridResolution = 12          -- Grid cells per axis (12^3 = 1728 cells)
}

-- Spatial Grid for optimized collision detection
LOCALS = {
    -- Grid data structures
    gridCellSize = 0.0,
    gridInvCellSize = 0.0,
    gridResSquared = 0,
    gridTotalCells = 0
}

-- Initialize spatial grid
local function initSpatialGrid()
    -- Manually compute boxMax - boxMin
    local gridSizeX = VARS.boxMax.x - VARS.boxMin.x
    local gridSizeY = VARS.boxMax.y - VARS.boxMin.y
    local gridSizeZ = VARS.boxMax.z - VARS.boxMin.z
    local maxDimension = math.max(gridSizeX, math.max(gridSizeY, gridSizeZ))
    LOCALS.gridCellSize = maxDimension / VARS.gridResolution
    LOCALS.gridInvCellSize = 1.0 / LOCALS.gridCellSize
    LOCALS.gridResSquared = VARS.gridResolution * VARS.gridResolution
    LOCALS.gridTotalCells = VARS.gridResolution * VARS.gridResolution * VARS.gridResolution
end

-- Get grid cell index from world position
local function getGridIndex(pos)
    -- Manually compute (pos - boxMin) * invCellSize
    local relX = (pos.x - VARS.boxMin.x) * LOCALS.gridInvCellSize
    local relY = (pos.y - VARS.boxMin.y) * LOCALS.gridInvCellSize
    local relZ = (pos.z - VARS.boxMin.z) * LOCALS.gridInvCellSize
    
    local x = math.floor(relX)
    local y = math.floor(relY)
    local z = math.floor(relZ)
    
    -- Clamp to grid bounds
    x = math.max(0, math.min(x, VARS.gridResolution - 1))
    y = math.max(0, math.min(y, VARS.gridResolution - 1))
    z = math.max(0, math.min(z, VARS.gridResolution - 1))
    
    return x + y * VARS.gridResolution + z * LOCALS.gridResSquared + 1 -- Lua is 1-indexed
end

-- Build spatial grid from particle positions
local function buildSpatialGrid(particles)
    local grid = {}
    
    -- Initialize grid cells
    for i = 1, LOCALS.gridTotalCells do
        grid[i] = Array.int()
    end
    
    -- Insert particles into grid cells
    for i = 1, #particles do
        local p = particles[i]
        local t3d = p.node:transform3D()
        if t3d then
            local pos = t3d:getPos()
            local cellIndex = getGridIndex(pos)
            grid[cellIndex]:push(i)
        end
    end
    
    return grid
end

-- Check collision between two particles
local function checkParticleCollision(p1, p2, radiusSquared)
    local t3d1 = p1.node:transform3D()
    local t3d2 = p2.node:transform3D()
    
    if not (t3d1 and t3d2) then
        return false
    end
    
    local pos1 = t3d1:getPos()
    local pos2 = t3d2:getPos()
    
    local dx = pos1.x - pos2.x
    local dy = pos1.y - pos2.y
    local dz = pos1.z - pos2.z
    local distSq = dx * dx + dy * dy + dz * dz
    
    return distSq < radiusSquared and distSq > 0.0001
end

-- Resolve collision between two particles
local function resolveCollision(p1, p2, vars1, vars2)
    local t3d1 = p1.node:transform3D()
    local t3d2 = p2.node:transform3D()
    local pos1 = t3d1:getPos()
    local pos2 = t3d2:getPos()
    
    local dx = pos1.x - pos2.x
    local dy = pos1.y - pos2.y
    local dz = pos1.z - pos2.z
    local distSq = dx * dx + dy * dy + dz * dz
    local dist = math.sqrt(distSq)
    
    -- Normalize
    local invDist = 1.0 / dist
    local nx = dx * invDist
    local ny = dy * invDist
    local nz = dz * invDist
    
    -- Separate particles
    local overlap = VARS.particleRadius * 2.0 - dist
    local separationHalf = overlap * 0.5
    
    pos1.x = pos1.x + nx * separationHalf
    pos1.y = pos1.y + ny * separationHalf
    pos1.z = pos1.z + nz * separationHalf
    
    pos2.x = pos2.x - nx * separationHalf
    pos2.y = pos2.y - ny * separationHalf
    pos2.z = pos2.z - nz * separationHalf
    
    t3d1:setPos(pos1)
    t3d2:setPos(pos2)
    
    -- Resolve velocities
    local vel1 = vars1.vel
    local vel2 = vars2.vel
    
    if vel1 and vel2 then
        local relVelX = vel1.x - vel2.x
        local relVelY = vel1.y - vel2.y
        local relVelZ = vel1.z - vel2.z
        
        local velAlongNormal = relVelX * nx + relVelY * ny + relVelZ * nz
        
        if velAlongNormal > 0 then return end -- Separating
        
        local impulse = -(1.0 + VARS.particleRestitution) * velAlongNormal * 0.5
        local impulseX = impulse * nx * VARS.collisionDamping
        local impulseY = impulse * ny * VARS.collisionDamping
        local impulseZ = impulse * nz * VARS.collisionDamping
        
        vel1.x = vel1.x + impulseX
        vel1.y = vel1.y + impulseY
        vel1.z = vel1.z + impulseZ
        
        vel2.x = vel2.x - impulseX
        vel2.y = vel2.y - impulseY
        vel2.z = vel2.z - impulseZ
        
        -- Write back velocities
        vars1.vel = vel1
        vars2.vel = vel2
        
        -- Handle angular velocity from collision
        local tangentialX = relVelX - velAlongNormal * nx
        local tangentialY = relVelY - velAlongNormal * ny
        local tangentialZ = relVelZ - velAlongNormal * nz
        local tangentialSpeed = math.sqrt(tangentialX * tangentialX + tangentialY * tangentialY + tangentialZ * tangentialZ)
        
        if tangentialSpeed > 0.01 then
            local spinScale = tangentialSpeed / VARS.particleRadius * 0.5
            local angVel1 = vars1.angVel
            local angVel2 = vars2.angVel
            
            if angVel1 and angVel2 then
                angVel1.x = angVel1.x + (ny * tangentialZ - nz * tangentialY) * spinScale
                angVel1.y = angVel1.y + (nz * tangentialX - nx * tangentialZ) * spinScale
                angVel1.z = angVel1.z + (nx * tangentialY - ny * tangentialX) * spinScale
                
                angVel2.x = angVel2.x - (ny * tangentialZ - nz * tangentialY) * spinScale
                angVel2.y = angVel2.y - (nz * tangentialX - nx * tangentialZ) * spinScale
                angVel2.z = angVel2.z - (nx * tangentialY - ny * tangentialX) * spinScale
                
                vars1.angVel = angVel1
                vars2.angVel = angVel2
            end
        end
    end
end

function update()
    if not VARS.enabled then
        return
    end
    
    -- Get all child nodes
    local children = NODE:children()
    if #children == 0 then
        return
    end
    
    -- Filter children that have particle scripts attached
    local particles = {}
    for i = 1, #children do
        local child = children[i]
        local script = child:script()
        if script then
            table.insert(particles, {
                node = child,
                script = script
            })
        end
    end
    
    if #particles == 0 then
        return
    end
    
    -- Initialize spatial grid on first frame
    if LOCALS.gridCellSize == 0.0 then
        initSpatialGrid()
    end
    
    -- Calculate substep time
    local substepDt = DELTATIME / VARS.substeps
    
    -- Run multiple substeps for stability
    for step = 1, VARS.substeps do
        -- ========== PHASE 1: Apply Forces & Update Velocities ==========
        for i = 1, #particles do
            local p = particles[i]
            local vars = p.script:vars()
            local vel = vars.vel
            
            if vel then
                -- Apply gravity
                vel.y = vel.y + VARS.gravity.y * substepDt
                vars.vel = vel
            end
        end
        
        -- ========== PHASE 2: Update Positions ==========
        for i = 1, #particles do
            local p = particles[i]
            local t3d = p.node:transform3D()
            local vars = p.script:vars()
            local vel = vars.vel
            
            if t3d and vel then
                local pos = t3d:getPos()
                
                -- Integrate position
                pos.x = pos.x + vel.x * substepDt
                pos.y = pos.y + vel.y * substepDt
                pos.z = pos.z + vel.z * substepDt
                
                t3d:setPos(pos)
            end
        end
        
        -- ========== PHASE 3: Boundary Constraints ==========
        for i = 1, #particles do
            local p = particles[i]
            local t3d = p.node:transform3D()
            local vars = p.script:vars()
            local vel = vars.vel
            
            if t3d and vel then
                local pos = t3d:getPos()
                local radius = VARS.particleRadius
                local posChanged = false
                
                -- X boundaries
                if pos.x - radius < VARS.boxMin.x then
                    pos.x = VARS.boxMin.x + radius
                    vel.x = math.abs(vel.x) * VARS.restitution
                    vel.z = vel.z * VARS.friction
                    posChanged = true
                elseif pos.x + radius > VARS.boxMax.x then
                    pos.x = VARS.boxMax.x - radius
                    vel.x = -math.abs(vel.x) * VARS.restitution
                    vel.z = vel.z * VARS.friction
                    posChanged = true
                end
                
                -- Y boundaries
                if pos.y - radius < VARS.boxMin.y then
                    pos.y = VARS.boxMin.y + radius
                    vel.y = math.abs(vel.y) * VARS.restitution
                    vel.x = vel.x * VARS.friction
                    vel.z = vel.z * VARS.friction
                    posChanged = true
                elseif pos.y + radius > VARS.boxMax.y then
                    pos.y = VARS.boxMax.y - radius
                    vel.y = -math.abs(vel.y) * VARS.restitution
                    vel.x = vel.x * VARS.friction
                    vel.z = vel.z * VARS.friction
                    posChanged = true
                end
                
                -- Z boundaries
                if pos.z - radius < VARS.boxMin.z then
                    pos.z = VARS.boxMin.z + radius
                    vel.z = math.abs(vel.z) * VARS.restitution
                    vel.x = vel.x * VARS.friction
                    posChanged = true
                elseif pos.z + radius > VARS.boxMax.z then
                    pos.z = VARS.boxMax.z - radius
                    vel.z = -math.abs(vel.z) * VARS.restitution
                    vel.x = vel.x * VARS.friction
                    posChanged = true
                end
                
                if posChanged then
                    t3d:setPos(pos)
                    -- Write back the modified velocity
                    vars.vel = vel
                end
            end
        end
        
        -- ========== PHASE 4: Particle-Particle Collisions (Spatial Grid Optimized) ==========
        if VARS.useGrid then
            -- Build spatial grid
            local grid = buildSpatialGrid(particles)
            local radiusSquared = VARS.particleRadius * VARS.particleRadius * 4.0
            
            -- Process each non-empty cell
            for cellIdx = 1, LOCALS.gridTotalCells do
                local cell = grid[cellIdx]
                local cellSize = cell:size()
                
                if cellSize > 0 then
                    -- Self-collisions within cell
                    for i = 1, cellSize do
                        for j = i + 1, cellSize do
                            local idx1 = cell[i]
                            local idx2 = cell[j]
                            local p1 = particles[idx1]
                            local p2 = particles[idx2]
                            
                            if checkParticleCollision(p1, p2, radiusSquared) then
                                resolveCollision(p1, p2, p1.script:vars(), p2.script:vars())
                            end
                        end
                    end
                    
                    -- Cross-cell collisions (check only forward neighbors to avoid duplicates)
                    -- Convert linear cell index to 3D coordinates
                    local z = math.floor((cellIdx - 1) / LOCALS.gridResSquared)
                    local rem = (cellIdx - 1) % LOCALS.gridResSquared
                    local y = math.floor(rem / VARS.gridResolution)
                    local x = rem % VARS.gridResolution
                    
                    -- Check 13 forward neighbors (half of 26 neighbors)
                    local neighborOffsets = {
                        {1, 0, 0}, {0, 1, 0}, {0, 0, 1},           -- 6-connectivity
                        {1, 1, 0}, {1, -1, 0}, {1, 0, 1}, {1, 0, -1}, -- edges
                        {0, 1, 1}, {0, 1, -1},                     -- more edges
                        {1, 1, 1}, {1, 1, -1}, {1, -1, 1}, {1, -1, -1} -- corners
                    }
                    
                    for _, offset in ipairs(neighborOffsets) do
                        local nx = x + offset[1]
                        local ny = y + offset[2]
                        local nz = z + offset[3]
                        
                        -- Check bounds
                        if nx >= 0 and nx < VARS.gridResolution and
                           ny >= 0 and ny < VARS.gridResolution and
                           nz >= 0 and nz < VARS.gridResolution then
                            
                            local neighborIdx = nx + ny * VARS.gridResolution + nz * LOCALS.gridResSquared + 1
                            local neighborCell = grid[neighborIdx]
                            local neighborSize = neighborCell:size()
                            
                            -- Check collisions between current cell and neighbor cell
                            for i = 1, cellSize do
                                local idx1 = cell[i]
                                local p1 = particles[idx1]
                                
                                for j = 1, neighborSize do
                                    local idx2 = neighborCell[j]
                                    local p2 = particles[idx2]
                                    
                                    if checkParticleCollision(p1, p2, radiusSquared) then
                                        resolveCollision(p1, p2, p1.script:vars(), p2.script:vars())
                                    end
                                end
                            end
                        end
                    end
                end
                
                ::continue_cell::
            end
        else
            -- Fallback: brute force O(n²) collision detection
            local radiusSquared = VARS.particleRadius * VARS.particleRadius * 4.0
            for i = 1, #particles do
                for j = i + 1, #particles do
                    local p1 = particles[i]
                    local p2 = particles[j]
                    
                    if checkParticleCollision(p1, p2, radiusSquared) then
                        resolveCollision(p1, p2, p1.script:vars(), p2.script:vars())
                    end
                end
            end
        end
    end -- end for step
    
    -- Apply frame-rate independent damping once per frame
    -- Convert per-frame damping to per-second damping for frame-rate independence
    local dampingFactor = VARS.damping ^ (DELTATIME * 60.0)
    local angularDampingFactor = 0.98 ^ (DELTATIME * 60.0) -- Slightly more damping for angular
    
    for i = 1, #particles do
        local p = particles[i]
        local vars = p.script:vars()
        local vel = vars.vel
        local angVel = vars.angVel
        
        -- Linear velocity damping
        if vel then
            vel.x = vel.x * dampingFactor
            vel.y = vel.y * dampingFactor
            vel.z = vel.z * dampingFactor
            -- Write back modified velocity
            vars.vel = vel
        end
        
        -- Angular velocity damping
        if angVel then
            angVel.x = angVel.x * angularDampingFactor
            angVel.y = angVel.y * angularDampingFactor
            angVel.z = angVel.z * angularDampingFactor
            -- Write back modified angular velocity
            vars.angVel = angVel
        end
    end
end
