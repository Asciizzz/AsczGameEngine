-- mathLib.lua
-- A static script library with a simple add function

function vars()
    return {}
end

function update()
    -- Static scripts don't need update
end

-- Add two numbers
function add(a, b)
    print("mathLib.add called with a=" .. tostring(a) .. ", b=" .. tostring(b))
    local result = a + b
    print("mathLib.add returning " .. tostring(result))
    return result
end
