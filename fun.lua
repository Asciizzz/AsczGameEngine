VARS = {
    sceneHandle = Handle("scene")
}


function update()

    local node = SCENE:addScene(VARS.sceneHandle, NODE:handle())
    if node then
        local t3D = node:transform3D()
        if t3D then
            t3D:setPos({x=0, y=5, z=0})
        end
    end

end