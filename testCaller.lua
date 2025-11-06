
function vars()
    return {
        otherLib = rHandle(),  -- Set this to otherLib's handle in the editor
    }
end

function update()
    -- #include
    local otherLib = FS:script(VARS.otherLib)

    if kState('k') then
        -- Will call the other script function when K is pressed
        if otherLib then otherLib:call("msg", "K key was pressed!") end
    end
end
