-- Particle Data Script
-- Attach this to individual particle nodes
-- Stores per-particle physics data (velocity and angular velocity)
-- The parent container's physics script reads/writes these values

function vars()
    return {
        -- Linear velocity (m/s)
        velX = 0.0,
        velY = 0.0,
        velZ = 0.0,
        
        -- Angular velocity (radians per second) - optional for visual spinning
        angVelX = 0.0,
        angVelY = 0.0,
        angVelZ = 0.0,
        
        -- Optional: Initialize with random velocity
        randomizeVelocity = false,
        randomVelRange = 2.0
    }
end

function update()
    -- One-time initialization: randomize velocity if requested
    if vars.randomizeVelocity then
        -- Generate random velocities
        vars.velX = (math.random() * 2.0 - 1.0) * vars.randomVelRange
        vars.velY = (math.random() * 2.0 - 1.0) * vars.randomVelRange
        vars.velZ = (math.random() * 2.0 - 1.0) * vars.randomVelRange
        
        -- Optional: Add some angular velocity for visual effect
        vars.angVelX = (math.random() * 2.0 - 1.0) * 3.14
        vars.angVelY = (math.random() * 2.0 - 1.0) * 3.14
        vars.angVelZ = (math.random() * 2.0 - 1.0) * 3.14
        
        -- Disable randomization after first run
        vars.randomizeVelocity = false
    end
    
    -- Optional: Apply angular velocity to rotation (visual spinning)
    if vars.angVelX ~= 0.0 or vars.angVelY ~= 0.0 or vars.angVelZ ~= 0.0 then
        local t3d = node:transform3D()
        if t3d then
            local rot = t3d:getRot()
            
            rot.x = rot.x + vars.angVelX * dTime
            rot.y = rot.y + vars.angVelY * dTime
            rot.z = rot.z + vars.angVelZ * dTime
            
            t3d:setRot(rot)
            
            -- Apply damping to angular velocity
            local angDamping = 0.98
            vars.angVelX = vars.angVelX * angDamping
            vars.angVelY = vars.angVelY * angDamping
            vars.angVelZ = vars.angVelZ * angDamping
        end
    end
end
