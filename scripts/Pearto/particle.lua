-- Particle Data Script
-- Attach this to individual particle nodes
-- Stores per-particle physics data (velocity and angular velocity)
-- The parent container's physics script reads/writes these values

VARS = {
    -- Linear velocity (m/s)
    vel = Vec3(0.0, 0.0, 0.0),
    
    -- Angular velocity (radians per second) - optional for visual spinning
    angVel = Vec3(0.0, 0.0, 0.0),
    
    -- Optional: Initialize with random velocity
    randomizeVelocity = false,
    randomVelRange = 2.0
}

function update()
    -- One-time initialization: randomize velocity if requested
    if VARS.randomizeVelocity then
        -- Generate random velocities
        VARS.vel.x = (math.random() * 2.0 - 1.0) * VARS.randomVelRange
        VARS.vel.y = (math.random() * 2.0 - 1.0) * VARS.randomVelRange
        VARS.vel.z = (math.random() * 2.0 - 1.0) * VARS.randomVelRange
        
        -- Optional: Add some angular velocity for visual effect
        VARS.angVel.x = (math.random() * 2.0 - 1.0) * 3.14
        VARS.angVel.y = (math.random() * 2.0 - 1.0) * 3.14
        VARS.angVel.z = (math.random() * 2.0 - 1.0) * 3.14
        
        -- Disable randomization after first run
        VARS.randomizeVelocity = false
    end
    
    -- Optional: Apply angular velocity to rotation (visual spinning)
    -- Using quaternion rotation with axis-angle approach
    if VARS.angVel.x ~= 0.0 or VARS.angVel.y ~= 0.0 or VARS.angVel.z ~= 0.0 then
        local t3d = NODE:transform3D()
        if t3d then
            -- Get current rotation as quaternion (Vec4: x, y, z, w)
            local quat = t3d:getQuat()
            
            -- Calculate rotation amount for this frame
            local angleX = VARS.angVel.x * DELTATIME
            local angleY = VARS.angVel.y * DELTATIME
            local angleZ = VARS.angVel.z * DELTATIME
            
            -- Apply incremental rotations using rotX, rotY, rotZ
            -- These apply relative rotations in degrees
            if angleX ~= 0.0 then
                t3d:rotX(math.deg(angleX))
            end
            if angleY ~= 0.0 then
                t3d:rotY(math.deg(angleY))
            end
            if angleZ ~= 0.0 then
                t3d:rotZ(math.deg(angleZ))
            end
            
            -- Apply damping to angular velocity
            local angDamping = 0.98
            VARS.angVel.x = VARS.angVel.x * angDamping
            VARS.angVel.y = VARS.angVel.y * angDamping
            VARS.angVel.z = VARS.angVel.z * angDamping
        end
    end
end
