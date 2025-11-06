-- testCaller.lua
-- Test script that calls add function from mathLib when 'k' is pressed

function vars()
    return {
        mathLibHandle = rHandle(),  -- Set this to mathLib's handle in the editor
        counter = 0.0
    }
end

function update()
    if kState('k') then
        -- Get the mathLib script
        local mathLib = FS:script(VARS.mathLibHandle)
        
        if mathLib then
            -- Call add function: counter + 1
            VARS.counter = mathLib:call("add", VARS.counter, 1.0)
            print("Counter: " .. VARS.counter)
        else
            print("Error: Could not load mathLib script!")
        end
    end
end
