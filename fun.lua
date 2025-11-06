

function update()

    local t3D = NODE:transform3D()

    if t3D then
        local pos = t3D:getPos()

        pos.x = math.random(-5.0, 5.0)
        pos.y = math.random(0.0, 5.0)
        pos.z = math.random(-5.0, 5.0)

        t3D:setPos(pos)
    end

end