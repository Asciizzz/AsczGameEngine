-- Unified Handle System Demo Script
-- This script demonstrates the new unified Handle("type", index, version) system
-- 
-- OLD SYSTEM (deprecated):
--   nHandle(index, version) - for node handles
--   rHandle(index, version) - for registry/resource handles
--
-- NEW SYSTEM (unified):
--   Handle("node", index, version) - for scene nodes (remapped on scene load)
--   Handle("script", index, version) - for script resources
--   Handle("scene", index, version) - for scene resources
--   Handle("animation", index, version) - for animation handles
--   Handle("mesh", index, version) - for mesh resources (future)
--   Handle("texture", index, version) - for texture resources (future)

function vars()
    return {
        -- Node handles - these are scene-specific and get remapped when scenes are loaded
        targetNode = Handle("node", 0xFFFFFFFF, 0xFFFFFFFF),
        childNode = Handle("node", 0xFFFFFFFF, 0xFFFFFFFF),
        
        -- Registry/Resource handles - these are global and never remapped
        scriptHandle = Handle("script", 0xFFFFFFFF, 0xFFFFFFFF),
        sceneHandle = Handle("scene", 0xFFFFFFFF, 0xFFFFFFFF),
        
        -- Demo state
        demoEnabled = true,
        timer = 0.0
    }
end

function update()
    if not VARS.demoEnabled then
        return
    end
    
    VARS.timer = VARS.timer + DTIME
    
    -- ========== HANDLE VALIDATION ==========
    -- Check if handles are valid using the :valid() method
    if VARS.targetNode:valid() then
        print("targetNode is valid: " .. tostring(VARS.targetNode))
    end
    
    -- ========== HANDLE COMPARISON ==========
    -- Handles can be compared using == operator (via __eq metamethod)
    if VARS.targetNode == VARS.childNode then
        print("Both handles point to the same node!")
    end
    
    if VARS.targetNode ~= VARS.childNode then
        print("Handles point to different nodes")
    end
    
    -- ========== HANDLE INTROSPECTION ==========
    -- Get handle properties
    if VARS.targetNode:valid() then
        local handleType = VARS.targetNode:type()      -- Returns "node"
        local handleIndex = VARS.targetNode:index()    -- Returns the index
        local handleVersion = VARS.targetNode:version() -- Returns the version
        
        print(string.format("Handle type: %s, index: %d, version: %d", 
              handleType, handleIndex, handleVersion))
    end
    
    -- ========== NODE OPERATIONS ==========
    -- Use scene:node(handle) to get a Node object from a handle
    if VARS.targetNode:valid() then
        local node = SCENE:node(VARS.targetNode)
        if node then
            local t3d = node:transform3D()
            if t3d then
                local pos = t3d:getPos()
                print(string.format("Node position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z))
            end
        end
    end
    
    -- ========== HIERARCHY OPERATIONS ==========
    -- Get parent/children handles using NODE:parentHandle() and NODE:childrenHandles()
    local parentHandle = NODE:parentHandle()
    if parentHandle and parentHandle:valid() then
        print("Current node has a parent: " .. tostring(parentHandle))
        
        -- Parent handle is also a Handle("node", ...) object
        local parentNode = SCENE:node(parentHandle)
        if parentNode then
            print("Successfully accessed parent node!")
        end
    end
    
    local childrenHandles = NODE:childrenHandles()
    print(string.format("Current node has %d children", #childrenHandles))
    for i = 1, #childrenHandles do
        local childHandle = childrenHandles[i]
        print(string.format("Child %d: %s", i, tostring(childHandle)))
        
        -- Each child handle is a Handle("node", ...) object
        if childHandle:type() == "node" then
            print("  Type confirmed: node")
        end
    end
    
    -- ========== RESOURCE OPERATIONS ==========
    -- Access script resources from the filesystem registry
    if VARS.scriptHandle:valid() then
        -- FS:get(handle) returns the resource based on handle type
        local script = FS:get(VARS.scriptHandle)
        if script then
            -- Call functions on the script resource
            -- script:call("functionName", arg1, arg2, ...)
            script:call("someFunction", 42, "hello", {x = 1, y = 2, z = 3})
        end
    end
    
    -- ========== SCENE INSTANTIATION ==========
    -- Load a scene as a child of a node
    if VARS.sceneHandle:valid() and VARS.targetNode:valid() then
        -- SCENE:addScene(sceneHandle, parentNodeHandle)
        -- Both arguments are Handle objects
        SCENE:addScene(VARS.sceneHandle, VARS.targetNode)
        print("Scene instantiated!")
    end
    
    -- You can also use NODEHANDLE (current node's handle) directly
    if VARS.sceneHandle:valid() then
        SCENE:addScene(VARS.sceneHandle, NODEHANDLE)
        print("Scene instantiated as child of current node!")
    end
    
    -- ========== ANIMATION HANDLES ==========
    -- Animation handles are also unified
    local anim = NODE:anim3D()
    if anim then
        -- Get animation handle by name
        local idleHandle = anim:get("Idle_Loop")
        if idleHandle then
            print("Idle animation handle: " .. tostring(idleHandle))
            print("Type: " .. idleHandle:type())  -- Returns "animation"
            
            -- Get current animation
            local currentHandle = anim:current()
            
            -- Compare animation handles using ==
            if currentHandle == idleHandle then
                print("Currently playing idle animation")
            elseif currentHandle ~= idleHandle then
                print("Not playing idle animation, switching...")
                anim:play(idleHandle, true)
            end
        end
    end
    
    -- ========== CROSS-SCRIPT COMMUNICATION ==========
    -- You can pass handles between scripts
    local otherNode = SCENE:node(VARS.targetNode)
    if otherNode then
        local otherScript = otherNode:script()
        if otherScript then
            -- Get a handle variable from another script
            local otherNodeHandle = otherScript:getVar("someNodeHandle")
            if otherNodeHandle and otherNodeHandle:valid() then
                print("Got handle from another script: " .. tostring(otherNodeHandle))
                
                -- Set a handle variable in another script
                otherScript:setVar("myNodeHandle", NODEHANDLE)
            end
        end
    end
    
    -- Disable demo after first frame
    VARS.demoEnabled = false
end
