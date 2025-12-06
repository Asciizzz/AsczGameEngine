-- Array Example Script
-- Demonstrates the new array/vector functionality

-- Define arrays in VARS table (persistent across frames)
VARS = {
    -- Scalar arrays (must use explicit constructors)
    numbers = Array:float(1.5, 2.5, 3.5),
    scores = Array:int(100, 200, 300),
    flags = Array:bool(true, false, true),
    
    -- Vector arrays
    positions = Array:vec3(Vec3(0, 0, 0), Vec3(1, 1, 1), Vec3(2, 2, 2)),
    colors = Array:vec4(Vec4(1, 0, 0, 1), Vec4(0, 1, 0, 1)),
    
    -- String and handle arrays
    names = Array:string("Alice", "Bob", "Charlie"),
    nodeHandles = Array:handle()
}

-- Define locals (temporary, reset each frame)
LOCALS = {
    counter = 0,
    tempArray = Array:float()
}

function update()
    -- Basic array operations using object-oriented syntax
    
    -- -- Push new element
    -- VARS.numbers:push(4.5)
    
    -- -- Get array size
    -- local size = VARS.numbers:size()
    -- print("Array size: " .. size)
    
    -- -- Pop last element
    -- local last = VARS.numbers:pop()
    -- print("Popped: " .. last)
    
    -- -- Insert at specific position
    -- VARS.scores:insert(2, 150)  -- Insert 150 at index 2
    
    -- -- Remove from specific position
    -- local removed = VARS.scores:remove(2)
    -- print("Removed: " .. removed)
    
    -- Iterate over array
    for i = 1, VARS.positions:size() do
        local pos = VARS.positions[i]
        print("Position " .. i .. ": " .. pos.x .. ", " .. pos.y .. ", " .. pos.z)
    end
    
    -- -- Modify array elements directly
    -- VARS.positions[1] = Vec3(5, 5, 5)
    
    -- -- Clear array
    -- if LOCALS.counter > 100 then
    --     LOCALS.tempArray:clear()
    -- end
    
    -- LOCALS.counter = LOCALS.counter + 1
end
