-- Example script demonstrating the skeleton3D binding
-- Shows how to manipulate bone transforms programmatically

VARS = {
    targetBoneIndex = 0,
    rotationSpeed = 1.0,
    oscillateAmplitude = 0.5,
    time = 0.0  -- Time accumulator
}

function update()
    -- Accumulate time
    VARS.time = VARS.time + DELTATIME
    
    local skeleton = NODE:skeleton3D()
    
    if skeleton == nil then
        print("No skeleton component found on this node")
        return
    end
    
    local boneCount = skeleton:boneCount()
    print("Skeleton has " .. boneCount .. " bones")
    
    -- Get a specific bone
    local bone = skeleton:bone(VARS.targetBoneIndex)
    
    if bone == nil then
        print("Bone index " .. VARS.targetBoneIndex .. " not found")
        return
    end
    
    print("Working with bone: " .. bone:name() .. " (index " .. bone:index() .. ")")
    
    -- Example 1: Read local pose
    local localPos = bone:getLocalPos()
    print("Local position: x=" .. localPos.x .. ", y=" .. localPos.y .. ", z=" .. localPos.z)
    
    -- Example 2: Modify rotation over time (calculate absolute rotation from time)
    local bindRot = bone:getBindRot()  -- Start from bind pose
    local localRot = {
        x = bindRot.x,
        y = bindRot.y + VARS.rotationSpeed * VARS.time,
        z = bindRot.z
    }
    bone:setLocalRot(localRot)
    
    -- Example 3: Oscillate position
    local bindPos = bone:getBindPos()
    localPos.x = bindPos.x + math.sin(VARS.time * 2.0) * VARS.oscillateAmplitude
    bone:setLocalPos(localPos)
    
    -- Example 4: Get full pose data
    local pose = bone:localPose()
    print("Full local pose - pos: (" .. pose.pos.x .. ", " .. pose.pos.y .. ", " .. pose.pos.z .. ")")
    print("                  rot: (" .. pose.rot.x .. ", " .. pose.rot.y .. ", " .. pose.rot.z .. ", " .. pose.rot.w .. ")")
    print("                  scl: (" .. pose.scl.x .. ", " .. pose.scl.y .. ", " .. pose.scl.z .. ")")
    
    -- Example 5: Access bind pose (read-only)
    local bindPos = bone:getBindPos()
    print("Bind position: x=" .. bindPos.x .. ", y=" .. bindPos.y .. ", z=" .. bindPos.z)
    
    -- Example 6: Hierarchy navigation
    local parent = bone:parent()
    if parent then
        print("Parent bone: " .. parent:name() .. " (index " .. parent:index() .. ")")
    else
        print("This is a root bone (no parent)")
    end
    
    local children = bone:children()
    print("Number of children: " .. #children)
    for i, child in ipairs(children) do
        print("  Child " .. i .. ": " .. child:name() .. " (index " .. child:index() .. ")")
    end
    
    -- Example 7: Get children indices directly
    local childIndices = bone:childrenIndices()
    for i, idx in ipairs(childIndices) do
        local childBone = skeleton:bone(idx)
        if childBone then
            print("  Child index " .. idx .. ": " .. childBone:name())
        end
    end
    
    -- Example 8: Reset all bones to bind pose
    -- skeleton:refreshAll()  -- Uncomment to reset
end

-- Example of procedural animation:
-- You could implement inverse kinematics, spring physics, or any custom animation system
-- by directly manipulating bone transforms in this script
