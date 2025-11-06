
function vars()
    return {
        otherLib = Handle("script"),  -- Set this to otherLib's handle in the editor

        h1 = Handle("node"),
        h2 = Handle("node")
    }
end

function update()
    -- #include
    local otherLib = FS:get(VARS.otherLib)

    if kState('k') then
        -- Will call the other script function when K is pressed
        if otherLib then otherLib:call("compare", VARS.h1, VARS.h2) end
    end
end
