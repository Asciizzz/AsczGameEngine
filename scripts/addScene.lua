-- Add Scene Script (Updated for Unified Handle System)
-- Attach this to a node to load a scene from the resource registry
-- Press K key to instantiate the scene as a child of this node

function vars()
    return {
        -- Unified handle to the scene resource
        -- Use Handle("scene", index, version) to create a scene handle
        -- This points to a Scene in the shared resource registry
        sceneHandle = Handle("scene", 0xFFFFFFFF, 0xFFFFFFFF),
        kHold = false  -- Internal state to track key press
    }
end

function update()
    -- Press K to load the scene
    if kState("k") and not VARS.kHold then
        VARS.kHold = true  -- Mark key as held

        if not VARS.sceneHandle or not VARS.sceneHandle:valid() then
            print("addScene.lua: Error - sceneHandle is invalid, cannot load scene")
            return
        end

        -- Add the scene with this node as parent
        -- NODEHANDLE is automatically a Handle("node", ...) in the new system
        SCENE:addScene(VARS.sceneHandle, NODEHANDLE)
        
        print("addScene.lua: Scene instantiated (press K was triggered)")
    end

    if not kState("k") then
        VARS.kHold = false  -- Reset key hold state when K is released
    end
end
