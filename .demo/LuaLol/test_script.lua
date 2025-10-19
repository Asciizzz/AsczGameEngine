-- test_script.lua
-- This demonstrates Lua-C++ communication for your engine

print("Lua: Script loaded!")

-- This would be like GetExposedParameters() in your engine
function GetExposedParameters()
    return {
        {name = "playerName", type = "string", default = "DefaultPlayer"},
        {name = "startX", type = "float", default = 0.0},
        {name = "startY", type = "float", default = 0.0},
        {name = "startZ", type = "float", default = 0.0},
        {name = "playerHandle", type = "Handle", default = -1}
    }
end

-- Initialize function (called once)
function Initialize()
    print("Lua: Initializing script for player: " .. playerName)
    
    -- Use the handle passed from C++
    if playerHandle >= 0 then
        print("Lua: Got valid player handle: " .. playerHandle)
        
        -- Set initial position using C++ function
        local success = SetPosition(playerHandle, startX, startY, startZ)
        if success then
            print("Lua: Set initial position to (" .. startX .. ", " .. startY .. ", " .. startZ .. ")")
        else
            print("Lua: Failed to set position!")
        end
    else
        print("Lua: Invalid player handle!")
    end
end

-- Update function (would be called every frame)
function Update()
    print("Lua: Update called")
    
    if playerHandle >= 0 then
        -- Get current position
        local x, y, z = GetPosition(playerHandle)
        if x then
            print("Lua: Current position: (" .. x .. ", " .. y .. ", " .. z .. ")")
            
            -- Move the object
            local newX = x + 5.0
            local newY = y + 2.0
            local newZ = z + 1.0
            
            SetPosition(playerHandle, newX, newY, newZ)
            print("Lua: Moved to (" .. newX .. ", " .. newY .. ", " .. newZ .. ")")
        else
            print("Lua: Failed to get position!")
        end
    end
end

-- Example of calling C++ function
function ShowStatus()
    print("Lua: Showing status...")
    PrintMessage("Hello from Lua! Player: " .. playerName)
    
    -- Create a new object from Lua
    local newObjHandle = CreateObject("LuaCreatedObject")
    print("Lua: Created new object with handle: " .. newObjHandle)
    
    -- Position the new object
    SetPosition(newObjHandle, 100, 200, 300)
    print("Lua: Positioned new object at (100, 200, 300)")
end

-- Example of parameter validation
function ValidateParameters()
    if not playerName or playerName == "" then
        print("Lua: Warning - playerName is empty!")
        return false
    end
    
    if playerHandle < 0 then
        print("Lua: Warning - invalid playerHandle!")
        return false
    end
    
    return true
end

print("Lua: Script functions defined!")