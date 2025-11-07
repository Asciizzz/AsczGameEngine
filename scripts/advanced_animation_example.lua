-- Example: Multi-layer animation system with smooth transitions
-- This demonstrates Genshin Impact / Honkai Star Rail style animation blending

-- Setup animation system (called once at initialization)
function setupAnimationSystem(character)
    local anime3D = character.anime3D
    
    -- ========================================
    -- LAYER 1: Locomotion (Full Body)
    -- ========================================
    local locomotion = anime3D:addLayer("Locomotion", "Normal")
    
    -- Add locomotion states
    locomotion.states["Idle"] = {
        name = "Idle",
        clipHandle = anime3D:getHandle("Idle"),
        loop = true,
        timeScale = 1.0
    }
    
    locomotion.states["Walk"] = {
        name = "Walk",
        clipHandle = anime3D:getHandle("Walk"),
        loop = true,
        timeScale = 1.0
    }
    
    locomotion.states["Run"] = {
        name = "Run",
        clipHandle = anime3D:getHandle("Run"),
        loop = true,
        timeScale = 1.2
    }
    
    locomotion.states["Jump"] = {
        name = "Jump",
        clipHandle = anime3D:getHandle("Jump"),
        loop = false,
        interruptible = false,
        minDuration = 0.3  -- Must play at least 300ms
    }
    
    -- ========================================
    -- LAYER 2: Upper Body (Overrides arms)
    -- ========================================
    local upperBody = anime3D:addLayer("UpperBody", "Override")
    upperBody.weight = 0.0  -- Start disabled
    
    -- Create bone mask for upper body
    local upperMask = anime3D:addMask("UpperBodyMask", {
        "spine", "spine1", "spine2",
        "shoulder_L", "shoulder_R",
        "arm_L", "arm_R",
        "hand_L", "hand_R"
    })
    upperBody.maskName = "UpperBodyMask"
    
    upperBody.states["Aim"] = {
        name = "Aim",
        clipHandle = anime3D:getHandle("Aim"),
        loop = true,
        timeScale = 1.0
    }
    
    upperBody.states["Attack"] = {
        name = "Attack",
        clipHandle = anime3D:getHandle("Attack"),
        loop = false,
        interruptible = true
    }
    
    -- ========================================
    -- LAYER 3: Facial (Additive)
    -- ========================================
    local facial = anime3D:addLayer("Facial", "Additive")
    facial.weight = 1.0
    
    facial.states["Neutral"] = {
        name = "Neutral",
        clipHandle = anime3D:getHandle("FaceNeutral"),
        loop = true
    }
    
    facial.states["Smile"] = {
        name = "Smile",
        clipHandle = anime3D:getHandle("FaceSmile"),
        loop = true
    }
    
    -- Set initial states
    anime3D:transitionToImmediate("Locomotion", "Idle")
    anime3D:transitionToImmediate("Facial", "Neutral")
    
    return anime3D
end

-- Update function (called every frame)
function updateCharacterAnimation(character, input, deltaTime)
    local anime3D = character.anime3D
    local velocity = character.velocity
    local speed = velocity:length()
    
    -- ========================================
    -- LOCOMOTION LAYER
    -- ========================================
    
    -- Speed-based state transitions
    if character.isGrounded then
        if speed < 0.5 then
            anime3D:transitionTo("Locomotion", "Idle", 0.3)
        elseif speed < 4.0 then
            anime3D:transitionTo("Locomotion", "Walk", 0.25)
            
            -- Adjust walk speed based on actual movement speed
            local walkState = anime3D:getLayer("Locomotion"):getState("Walk")
            if walkState then
                -- Scale from 0.8x to 1.2x based on speed
                walkState.timeScale = 0.8 + (speed / 4.0) * 0.4
            end
        else
            anime3D:transitionTo("Locomotion", "Run", 0.2)
            
            -- Speed up run animation for fast movement
            local runState = anime3D:getLayer("Locomotion"):getState("Run")
            if runState then
                runState.timeScale = 1.0 + ((speed - 4.0) / 6.0) * 0.5
            end
        end
    else
        -- In air
        anime3D:transitionTo("Locomotion", "Jump", 0.15)
    end
    
    -- ========================================
    -- UPPER BODY LAYER (Aim/Attack)
    -- ========================================
    
    local upperBody = anime3D:getLayer("UpperBody")
    
    if input.aiming then
        -- Enable upper body layer
        upperBody.weight = lerpTo(upperBody.weight, 1.0, deltaTime * 5.0)
        anime3D:transitionTo("UpperBody", "Aim", 0.2)
        
        -- Trigger attack
        if input.attackPressed then
            anime3D:transitionTo("UpperBody", "Attack", 0.1)
        end
    else
        -- Disable upper body layer (blend back to locomotion)
        upperBody.weight = lerpTo(upperBody.weight, 0.0, deltaTime * 3.0)
    end
    
    -- ========================================
    -- FACIAL LAYER
    -- ========================================
    
    if character.emotion == "happy" then
        anime3D:transitionTo("Facial", "Smile", 0.5)
    else
        anime3D:transitionTo("Facial", "Neutral", 0.5)
    end
    
    -- ========================================
    -- UPDATE ANIMATION SYSTEM
    -- ========================================
    
    anime3D:update(character.scene, deltaTime)
end

-- Direct clip manipulation example
function scrubAnimation(character, clipName, time)
    local clip = character.anime3D:get(clipName)
    if clip then
        clip.time = time
        clip.playing = false  -- Pause for manual scrubbing
    end
end

-- Play animation with custom settings
function playAnimationCustom(character, clipName, speed, loop)
    local clip = character.anime3D:get(clipName)
    if clip then
        clip.speed = speed or 1.0
        clip.loop = loop or false
        clip:play(true)  -- Play from start
    end
end

-- Helper function for smooth interpolation
function lerpTo(current, target, alpha)
    return current + (target - current) * alpha
end

--[[
USAGE EXAMPLE:

-- In game initialization:
local player = createPlayer()
setupAnimationSystem(player)

-- In game loop:
function update(dt)
    local input = getPlayerInput()
    updateCharacterAnimation(player, input, dt)
end

-- For cutscenes or manual control:
scrubAnimation(player, "Walk", 1.5)  -- Scrub to 1.5 seconds
playAnimationCustom(player, "Victory", 0.8, false)  -- Play victory at 80% speed

]]
