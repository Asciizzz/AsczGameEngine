-- GLOBALS are shared across ALL instances of this script
-- Perfect for constants, shared counters, or script linking
GLOBALS = {
    PI = 3.14159,
    instanceCounter = 0,
    sharedHandle = Handle(), -- Persistent handle shared by all instances
}

VARS = {
    handle = Handle(), -- Test handle to another node
    neckIndex = 0
}


function update()
    -- Example: Use GLOBALS - changes affect ALL script instances
    GLOBALS.instanceCounter = GLOBALS.instanceCounter + 1
    
    -- Use the shared constant
    -- local rotationSpeed = GLOBALS.PI * 10.0
    local trfm3D = NODE:Transform3D()
    if trfm3D then
        trfm3D:rotY(DELTATIME * 20.0)
    end

    -- end

    local skele3D = NODE:Skeleton3D()
    if skele3D then

        local kA_press = KSTATE("A")
        local kD_press = KSTATE("D")

        -- Spin the root
        local neckBone = skele3D:bone(VARS.neckIndex)
        if neckBone then
            -- neckBone:rotY(DELTATIME * 40.0 * rotY)

            if kA_press then
                neckBone:rotY(DELTATIME * 40.0)
            end
            if kD_press then
                neckBone:rotY(-DELTATIME * 40.0)
            end
        end
    end
end
