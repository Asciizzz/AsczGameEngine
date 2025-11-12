-- Particle Physics Manager Script
-- Attach this to the container NODE (parent of all particles)
-- Simulates physically accurate sphere collisions and boundary constraints

function vars()
    return {
        -- Bounding box (world space)
        boxMin = {x = -5.0, y = 0.0, z = -5.0},
        boxMax = {x = 5.0, y = 10.0, z = 5.0},
        
        -- Particle properties
        particleRadius = 0.2,        -- Radius of each spherical particle
        particleMass = 1.0,          -- Mass of each particle (kg)
        
        -- Physics parameters
        gravity = {x = 0.0, y = -9.81, z = 0.0},  -- Gravity vector (m/s²)
        damping = 0.995,             -- Velocity damping (0-1, 1 = no damping) - frame-rate independent
        restitution = 0.7,           -- Bounciness for wall collisions (0-1)
        friction = 0.95,             -- Surface friction (0-1, 1 = no friction)
        
        -- Collision properties
        particleRestitution = 0.8,   -- Bounciness for particle-particle collisions
        collisionDamping = 0.99,     -- Energy loss in collisions
        
        -- Simulation settings
        substeps = 4,                -- Physics substeps per frame (higher = more stable)
        enabled = true               -- Enable/disable simulation
    }
end

function update()
    if not VARS.enabled then
        return
    end
    
    -- Get all particle children
    local particles = NODE:children()
    if #particles == 0 then
        return
    end
    
    -- Calculate substep time
    local substepDt = DELTATIME / VARS.substeps
    
    -- Run multiple substeps for stability
    for step = 1, VARS.substeps do
        -- ========== PHASE 1: Apply Forces & Update Velocities ==========
        for i = 1, #particles do
            local particle = particles[i]
            
            -- Get particle's velocity from its script
            local velX = particle:getVar("velX") or 0.0
            local velY = particle:getVar("velY") or 0.0
            local velZ = particle:getVar("velZ") or 0.0
            
            -- Apply gravity
            velY = velY + VARS.gravity.y * substepDt
            
            -- Store updated velocity (damping applied later, per frame not per substep)
            particle:setVar("velX", velX)
            particle:setVar("velY", velY)
            particle:setVar("velZ", velZ)
        end
        
        -- ========== PHASE 2: Update Positions ==========
        for i = 1, #particles do
            local particle = particles[i]
            local t3d = particle:transform3D()
            if not t3d then goto continue_phase2 end
            
            local pos = t3d:getPos()
            
            local velX = particle:getVar("velX") or 0.0
            local velY = particle:getVar("velY") or 0.0
            local velZ = particle:getVar("velZ") or 0.0
            
            -- Integrate position
            pos.x = pos.x + velX * substepDt
            pos.y = pos.y + velY * substepDt
            pos.z = pos.z + velZ * substepDt
            
            t3d:setPos(pos)
            
            ::continue_phase2::
        end
        
        -- ========== PHASE 3: Boundary Constraints ==========
        for i = 1, #particles do
            local particle = particles[i]
            local t3d = particle:transform3D()
            if not t3d then goto continue_phase3 end
            
            local pos = t3d:getPos()
            
            local velX = particle:getVar("velX") or 0.0
            local velY = particle:getVar("velY") or 0.0
            local velZ = particle:getVar("velZ") or 0.0
            
            local radius = VARS.particleRadius
            local posChanged = false
            
            -- X boundaries
            if pos.x - radius < VARS.boxMin.x then
                pos.x = VARS.boxMin.x + radius
                velX = math.abs(velX) * VARS.restitution
                velZ = velZ * VARS.friction
                posChanged = true
            elseif pos.x + radius > VARS.boxMax.x then
                pos.x = VARS.boxMax.x - radius
                velX = -math.abs(velX) * VARS.restitution
                velZ = velZ * VARS.friction
                posChanged = true
            end
            
            -- Y boundaries
            if pos.y - radius < VARS.boxMin.y then
                pos.y = VARS.boxMin.y + radius
                velY = math.abs(velY) * VARS.restitution
                velX = velX * VARS.friction
                velZ = velZ * VARS.friction
                posChanged = true
            elseif pos.y + radius > VARS.boxMax.y then
                pos.y = VARS.boxMax.y - radius
                velY = -math.abs(velY) * VARS.restitution
                velX = velX * VARS.friction
                velZ = velZ * VARS.friction
                posChanged = true
            end
            
            -- Z boundaries
            if pos.z - radius < VARS.boxMin.z then
                pos.z = VARS.boxMin.z + radius
                velZ = math.abs(velZ) * VARS.restitution
                velX = velX * VARS.friction
                posChanged = true
            elseif pos.z + radius > VARS.boxMax.z then
                pos.z = VARS.boxMax.z - radius
                velZ = -math.abs(velZ) * VARS.restitution
                velX = velX * VARS.friction
                posChanged = true
            end
            
            if posChanged then
                t3d:setPos(pos)
                particle:setVar("velX", velX)
                particle:setVar("velY", velY)
                particle:setVar("velZ", velZ)
            end
            
            ::continue_phase3::
        end
        
        -- ========== PHASE 4: Particle-Particle Collisions ==========
        for i = 1, #particles do
            for j = i + 1, #particles do
                local p1 = particles[i]
                local p2 = particles[j]
                
                local t3d1 = p1:transform3D()
                local t3d2 = p2:transform3D()
                if not t3d1 or not t3d2 then goto continue_collision end
                
                local pos1 = t3d1:getPos()
                local pos2 = t3d2:getPos()
                
                -- Calculate distance vector
                local dx = pos2.x - pos1.x
                local dy = pos2.y - pos1.y
                local dz = pos2.z - pos1.z
                local distSq = dx * dx + dy * dy + dz * dz
                local minDist = VARS.particleRadius * 2.0
                local minDistSq = minDist * minDist
                
                -- Check for collision
                if distSq < minDistSq and distSq > 0.0001 then
                    local dist = math.sqrt(distSq)
                    
                    -- Normalize collision normal
                    local nx = dx / dist
                    local ny = dy / dist
                    local nz = dz / dist
                    
                    -- Separate particles more aggressively to prevent overlap
                    local overlap = minDist - dist
                    local separation = overlap * 0.5
                    
                    -- Add extra separation force for deeply overlapping particles
                    local separationForce = 1.0
                    if overlap > VARS.particleRadius * 0.5 then
                        -- Deep overlap - push apart more forcefully
                        separationForce = 1.5
                    end
                    
                    pos1.x = pos1.x - nx * separation * separationForce
                    pos1.y = pos1.y - ny * separation * separationForce
                    pos1.z = pos1.z - nz * separation * separationForce
                    
                    pos2.x = pos2.x + nx * separation * separationForce
                    pos2.y = pos2.y + ny * separation * separationForce
                    pos2.z = pos2.z + nz * separation * separationForce
                    
                    t3d1:setPos(pos1)
                    t3d2:setPos(pos2)
                    
                    -- Get velocities
                    local v1x = p1:getVar("velX") or 0.0
                    local v1y = p1:getVar("velY") or 0.0
                    local v1z = p1:getVar("velZ") or 0.0
                    
                    local v2x = p2:getVar("velX") or 0.0
                    local v2y = p2:getVar("velY") or 0.0
                    local v2z = p2:getVar("velZ") or 0.0
                    
                    -- Relative velocity
                    local dvx = v2x - v1x
                    local dvy = v2y - v1y
                    local dvz = v2z - v1z
                    
                    -- Velocity along collision normal
                    local dvn = dvx * nx + dvy * ny + dvz * nz
                    
                    -- Don't resolve if velocities are separating
                    if dvn < 0 then
                        -- Calculate impulse (assuming equal mass)
                        local restitution = VARS.particleRestitution
                        local impulse = -(1.0 + restitution) * dvn * 0.5
                        
                        -- Apply impulse
                        local impX = nx * impulse * VARS.collisionDamping
                        local impY = ny * impulse * VARS.collisionDamping
                        local impZ = nz * impulse * VARS.collisionDamping
                        
                        v1x = v1x - impX
                        v1y = v1y - impY
                        v1z = v1z - impZ
                        
                        v2x = v2x + impX
                        v2y = v2y + impY
                        v2z = v2z + impZ
                        
                        -- Update velocities
                        p1:setVar("velX", v1x)
                        p1:setVar("velY", v1y)
                        p1:setVar("velZ", v1z)
                        
                        p2:setVar("velX", v2x)
                        p2:setVar("velY", v2y)
                        p2:setVar("velZ", v2z)
                        
                        -- Calculate angular velocity from collision (tangential impulse)
                        -- Get tangential velocity (perpendicular to collision normal)
                        local tangentialX = dvx - dvn * nx
                        local tangentialY = dvy - dvn * ny
                        local tangentialZ = dvz - dvn * nz
                        
                        -- Convert tangential velocity to angular velocity
                        -- Angular velocity = cross product of collision point and tangential velocity
                        -- Simplified: use tangential velocity magnitude to spin particles
                        local tangentialSpeed = math.sqrt(tangentialX * tangentialX + 
                                                         tangentialY * tangentialY + 
                                                         tangentialZ * tangentialZ)
                        
                        if tangentialSpeed > 0.01 then
                            -- Create spin axis perpendicular to collision normal and tangential direction
                            local spinScale = tangentialSpeed / VARS.particleRadius * 0.5
                            
                            -- Apply angular velocity to both particles (opposite spins)
                            local ang1x = (p1:getVar("angVelX") or 0.0) + ny * tangentialZ - nz * tangentialY * spinScale
                            local ang1y = (p1:getVar("angVelY") or 0.0) + nz * tangentialX - nx * tangentialZ * spinScale
                            local ang1z = (p1:getVar("angVelZ") or 0.0) + nx * tangentialY - ny * tangentialX * spinScale
                            
                            local ang2x = (p2:getVar("angVelX") or 0.0) - ny * tangentialZ + nz * tangentialY * spinScale
                            local ang2y = (p2:getVar("angVelY") or 0.0) - nz * tangentialX + nx * tangentialZ * spinScale
                            local ang2z = (p2:getVar("angVelZ") or 0.0) - nx * tangentialY + ny * tangentialX * spinScale
                            
                            p1:setVar("angVelX", ang1x)
                            p1:setVar("angVelY", ang1y)
                            p1:setVar("angVelZ", ang1z)
                            
                            p2:setVar("angVelX", ang2x)
                            p2:setVar("angVelY", ang2y)
                            p2:setVar("angVelZ", ang2z)
                        end
                    end -- end if dvn < 0
                end -- end if distSq < minDistSq
                
                ::continue_collision::
            end -- end for j
        end -- end for i  
    end -- end for step
    
    -- Apply frame-rate independent damping once per frame
    -- Convert per-frame damping to per-second damping for frame-rate independence
    local dampingFactor = math.pow(VARS.damping, DELTATIME * 60.0)
    local angularDampingFactor = math.pow(0.98, DELTATIME * 60.0) -- Slightly more damping for angular
    
    for i = 1, #particles do
        local particle = particles[i]
        
        -- Linear velocity damping
        local velX = particle:getVar("velX") or 0.0
        local velY = particle:getVar("velY") or 0.0
        local velZ = particle:getVar("velZ") or 0.0
        
        velX = velX * dampingFactor
        velY = velY * dampingFactor
        velZ = velZ * dampingFactor
        
        particle:setVar("velX", velX)
        particle:setVar("velY", velY)
        particle:setVar("velZ", velZ)
        
        -- Angular velocity damping
        local angVelX = particle:getVar("angVelX") or 0.0
        local angVelY = particle:getVar("angVelY") or 0.0
        local angVelZ = particle:getVar("angVelZ") or 0.0
        
        angVelX = angVelX * angularDampingFactor
        angVelY = angVelY * angularDampingFactor
        angVelZ = angVelZ * angularDampingFactor
        
        particle:setVar("angVelX", angVelX)
        particle:setVar("angVelY", angVelY)
        particle:setVar("angVelZ", angVelZ)
    end
end
-- end function update
