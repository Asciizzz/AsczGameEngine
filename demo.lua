function vars() 
    return {
        a = nHandle(),
        b = rHandle()
    }

end


function update()
    if kState("k") then
        if vars.a == vars.b then
            print("Handles are equal!")
        else
            print("Handles are NOT equal!")
        end
    end
end