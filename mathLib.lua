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
    return a + b
end
