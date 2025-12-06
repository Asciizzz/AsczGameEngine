VARS = {
    other = Handle()
}

function update()
    local node = SCENE:node(VARS.other)

    local rtScript = node:script()

    if rtScript then
        print("Other script variable a:", rtScript:getVar("a"))
    end

end