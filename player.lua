VARS = {
    handle = Handle(), -- Test
    hp = 0
}


function update()
    -- Get own transform
    local myT3d = NODE:transform3D()
    if not myT3d then return end

    VARS.hp = VARS.hp + 1
end
