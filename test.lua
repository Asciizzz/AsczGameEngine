VARS = {
    handle = Handle(), -- Test handle to another node
    hp = 0
}


function update()
    -- Get own transform
    local myT3d = NODE:Transform3D()
    if not myT3d then return end

    VARS.hp = VARS.hp + 1
    
    -- Example: Get another node from scene by handle
    local otherNode = SCENE:node(VARS.handle)
    if otherNode then
        -- Access the other node's components
        local otherTransform = otherNode:Transform3D()
        local otherSkeleton = otherNode:Skeleton3D()
        
        print("Hello")
        -- Do something with the other node's components
        -- if otherTransform then
        --     local pos = otherTransform:getPos()
        -- end
    end
end
