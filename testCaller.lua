
function vars()
    return {
        otherLib = Handle("script"),  -- Set this to otherLib's handle in the editor

        h1 = Handle("node"),
        h2 = Handle("node"),

        t = 0
    }
end

function update()
    -- #include
    local otherLib = FS:get(VARS.otherLib)

    if kState('k') then
        VARS.t = VARS.t + 1
        -- Will call the other script function when K is pressed

        if h1 == h2 then
            print("Hello")
        else
            print("Fuck you")
        end
    end
end