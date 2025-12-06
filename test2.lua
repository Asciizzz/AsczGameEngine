VARS = {
    other = Handle()
}

function update()
    local node = SCENE:node(VARS.other)

    local rtScript = node:script()

    if rtScript then
        local otherVars = rtScript:vars()
        print("Other script variable a:", otherVars.a)
        
        -- You can also modify the other script's variables directly
        otherVars.a = otherVars.a + 1
    end

end