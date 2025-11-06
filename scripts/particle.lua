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
    if VARS.randomizeVelocity then
        -- Generate random velocities
        VARS.velX = (math.random() * 2.0 - 1.0) * VARS.randomVelRange
        VARS.velY = (math.random() * 2.0 - 1.0) * VARS.randomVelRange
        VARS.velZ = (math.random() * 2.0 - 1.0) * VARS.randomVelRange
        
        -- Optional: Add some angular velocity for visual effect
        VARS.angVelX = (math.random() * 2.0 - 1.0) * 3.14
        VARS.angVelY = (math.random() * 2.0 - 1.0) * 3.14
        VARS.angVelZ = (math.random() * 2.0 - 1.0) * 3.14
        
        -- Disable randomization after first run
        VARS.randomizeVelocity = false
    end
    
    -- Optional: Apply angular velocity to rotation (visual spinning)
    if VARS.angVelX ~= 0.0 or VARS.angVelY ~= 0.0 or VARS.angVelZ ~= 0.0 then
        local t3d = NODE:transform3D()
        if t3d then
            local rot = t3d:getRot()
            
            rot.x = rot.x + VARS.angVelX * DTIME
            rot.y = rot.y + VARS.angVelY * DTIME
            rot.z = rot.z + VARS.angVelZ * DTIME
            
            t3d:setRot(rot)
            
            -- Apply damping to angular velocity
            local angDamping = 0.98
            VARS.angVelX = VARS.angVelX * angDamping
            VARS.angVelY = VARS.angVelY * angDamping
            VARS.angVelZ = VARS.angVelZ * angDamping
        end
    end
end
