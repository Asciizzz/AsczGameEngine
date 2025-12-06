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
    enabled = true               -- Enable/disable simulation
}

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
            -- Store both the node and its script vars accessor
            table.insert(particles, {
                node = child,
                vars = script:vars()
            })
        end
    end
    
    if #particles == 0 then
        return
    end
    
    -- Calculate substep time
    local substepDt = DELTATIME / VARS.substeps
    
    -- Run multiple substeps for stability
    for step = 1, VARS.substeps do
        -- ========== PHASE 1: Apply Forces & Update Velocities ==========
        for i = 1, #particles do
            local p = particles[i]
            local vel = p.vars.vel
            
            if vel then
                -- Apply gravity
                vel.y = vel.y + VARS.gravity.y * substepDt
            end
        end
        
        -- ========== PHASE 2: Update Positions ==========
        for i = 1, #particles do
            local p = particles[i]
            local t3d = p.node:transform3D()
            local vel = p.vars.vel
            
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
            local vel = p.vars.vel
            
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
                end
            end
        end
        
        -- ========== PHASE 4: Particle-Particle Collisions ==========
        for i = 1, #particles do
            for j = i + 1, #particles do
                local p1 = particles[i]
                local p2 = particles[j]
                
                local t3d1 = p1.node:transform3D()
                local t3d2 = p2.node:transform3D()
                local vel1 = p1.vars.vel
                local vel2 = p2.vars.vel
                
                if not (t3d1 and t3d2 and vel1 and vel2) then
                    goto continue_collision
                end
                
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
                    
                    -- Relative velocity
                    local dvx = vel2.x - vel1.x
                    local dvy = vel2.y - vel1.y
                    local dvz = vel2.z - vel1.z
                    
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
                        
                        vel1.x = vel1.x - impX
                        vel1.y = vel1.y - impY
                        vel1.z = vel1.z - impZ
                        
                        vel2.x = vel2.x + impX
                        vel2.y = vel2.y + impY
                        vel2.z = vel2.z + impZ
                        
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
                            
                            -- Get angular velocities
                            local angVel1 = p1.vars.angVel
                            local angVel2 = p2.vars.angVel
                            
                            if angVel1 and angVel2 then
                                -- Apply angular velocity to both particles (opposite spins)
                                angVel1.x = angVel1.x + (ny * tangentialZ - nz * tangentialY) * spinScale
                                angVel1.y = angVel1.y + (nz * tangentialX - nx * tangentialZ) * spinScale
                                angVel1.z = angVel1.z + (nx * tangentialY - ny * tangentialX) * spinScale
                                
                                angVel2.x = angVel2.x - (ny * tangentialZ - nz * tangentialY) * spinScale
                                angVel2.y = angVel2.y - (nz * tangentialX - nx * tangentialZ) * spinScale
                                angVel2.z = angVel2.z - (nx * tangentialY - ny * tangentialX) * spinScale
                            end
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
        local p = particles[i]
        local vel = p.vars.vel
        local angVel = p.vars.angVel
        
        -- Linear velocity damping
        if vel then
            vel.x = vel.x * dampingFactor
            vel.y = vel.y * dampingFactor
            vel.z = vel.z * dampingFactor
        end
        
        -- Angular velocity damping
        if angVel then
            angVel.x = angVel.x * angularDampingFactor
            angVel.y = angVel.y * angularDampingFactor
            angVel.z = angVel.z * angularDampingFactor
        end
    end
end
