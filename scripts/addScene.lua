-- Add Scene Script
-- Attach this to a node to load a scene from the resource registry
-- Press K key to instantiate the scene as a child of this node

function vars()
    return {
        -- Registry handle to the scene resource (use rHandle(index, version))
        -- This should point to a Scene in the shared resource registry
        sceneHandle = rHandle(0xFFFFFFFF, 0xFFFFFFFF),
        kHold = false  -- Internal state to track key press
    }
end

function update()
    -- Press K to load the scene
    if kState("k") and not VARS.kHold then
        VARS.kHold = true  -- Mark key as held

        if not VARS.sceneHandle then
            print("addScene.lua: Error - sceneHandle is nil, cannot load scene")
            return
        end

        -- Add the scene with this node as parent
        SCENE:addScene(VARS.sceneHandle, NODEHANDLE)
        
        print("addScene.lua: Scene instantiated (press K was triggered)")
    end

    if not kState("k") then
        VARS.kHold = false  -- Reset key hold state when K is released
    end
end
