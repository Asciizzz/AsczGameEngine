# Vec2/Vec3/Vec4/Quat Rework Summary

## Overview
This document summarizes the massive rework to make Vec2, Vec3, Vec4, and Quat dedicated userdata types in the Lua scripting system, similar to how LuaHandle is handled. This improves type safety and avoids confusion with regular Lua tables.

## What Changed

### 1. New Userdata Types
- **Vec2**, **Vec3**, **Vec4**, **Quat** are now full userdata types with metatables
- Each type has its own constructor function: `Vec2(x, y)`, `Vec3(x, y, z)`, `Vec4(x, y, z, w)`, `Quat(w, x, y, z)`
- Each type has a `__tostring` metamethod for debugging

### 2. API Changes (Breaking Changes!)

#### Transform3D Component
**OLD (table-based):**
```lua
local pos = transform:getPos()  -- Returns {x=1, y=2, z=3}
transform:setPos({x = 10, y = 20, z = 30})

local rot = transform:getRot()  -- Returns {x=0, y=0, z=0} (euler)
transform:setRot({x = 0, y = 1.57, z = 0})

local quat = transform:getQuat()  -- Returns {x=0, y=0, z=0, w=1}
transform:setQuat({x = 0, y = 0.707, z = 0, w = 0.707})

local scale = transform:getScl()  -- Returns {x=1, y=1, z=1}
transform:setScl({x = 2, y = 2, z = 2})
```

**NEW (Vec/Quat userdata):**
```lua
local pos = transform:getPos()  -- Returns Vec3 userdata
transform:setPos(Vec3(10, 20, 30))

local rot = transform:getRot()  -- Returns Vec3 userdata (euler)
transform:setRot(Vec3(0, 1.57, 0))

local quat = transform:getQuat()  -- Returns Quat userdata
transform:setQuat(Quat(0.707, 0, 0.707, 0))

local scale = transform:getScl()  -- Returns Vec3 userdata
transform:setScl(Vec3(2, 2, 2))
```

#### Skeleton3D / Bone Component
**OLD:**
```lua
local bone = skeleton:bone(0)
local pos = bone:getLocalPos()  -- Returns {x=1, y=2, z=3}
bone:setLocalPos({x = 5, y = 5, z = 5})

local rot = bone:getLocalRot()  -- Returns {x=0, y=0, z=0}
bone:setLocalRot({x = 0, y = 1.57, z = 0})

local quat = bone:getLocalQuat()  -- Returns {x=0, y=0, z=0, w=1}
bone:setLocalQuat({x = 0, y = 0.707, z = 0, w = 0.707})
```

**NEW:**
```lua
local bone = skeleton:bone(0)
local pos = bone:getLocalPos()  -- Returns Vec3 userdata
bone:setLocalPos(Vec3(5, 5, 5))

local rot = bone:getLocalRot()  -- Returns Vec3 userdata
bone:setLocalRot(Vec3(0, 1.57, 0))

local quat = bone:getLocalQuat()  -- Returns Quat userdata
bone:setLocalQuat(Quat(0.707, 0, 0.707, 0))
```

Same changes apply to:
- `bone:getBindPos()`, `bone:getBindRot()`, `bone:getBindQuat()`, `bone:getBindScl()`
- `bone:localPose()` now returns `{pos=Vec3, rot=Quat, scl=Vec3}` instead of nested tables
- `bone:bindPose()` now returns `{pos=Vec3, rot=Quat, scl=Vec3}` instead of nested tables

#### Quaternion Utility Functions
**OLD:**
```lua
local result = quat_slerp({x=0, y=0, z=0, w=1}, {x=0, y=0.707, z=0, w=0.707}, 0.5)
local q = quat_fromEuler({x=0, y=1.57, z=0})
local euler = quat_toEuler({x=0, y=0.707, z=0, w=0.707})
local q = quat_fromAxisAngle({x=0, y=1, z=0}, 1.57)
local q = quat_lookAt({x=0, y=0, z=-1}, {x=0, y=1, z=0})
```

**NEW:**
```lua
local result = quat_slerp(Quat(1, 0, 0, 0), Quat(0.707, 0, 0.707, 0), 0.5)  -- Returns Quat
local q = quat_fromEuler(Vec3(0, 1.57, 0))  -- Returns Quat
local euler = quat_toEuler(Quat(0.707, 0, 0.707, 0))  -- Returns Vec3
local q = quat_fromAxisAngle(Vec3(0, 1, 0), 1.57)  -- Returns Quat
local q = quat_lookAt(Vec3(0, 0, -1), Vec3(0, 1, 0))  -- Returns Quat
```

#### VARS System
**OLD:**
```lua
VARS = {
    speed = 5.0,
    position = {x = 0, y = 0, z = 0},  -- Interpreted as vec3
    color = {x = 1, y = 1, z = 1, w = 1}  -- Interpreted as vec4
}
```

**NEW:**
```lua
VARS = {
    speed = 5.0,
    position = Vec3(0, 0, 0),  -- Explicit Vec3
    color = Vec4(1, 1, 1, 1)  -- Explicit Vec4
}
```

#### Script Component Methods
**OLD:**
```lua
local otherScript = node:script()
local pos = otherScript:getVar("position")  -- Returns {x=1, y=2, z=3}
otherScript:setVar("position", {x = 10, y = 20, z = 30})
```

**NEW:**
```lua
local otherScript = node:script()
local pos = otherScript:getVar("position")  -- Returns Vec3 userdata
otherScript:setVar("position", Vec3(10, 20, 30))
```

#### StaticScript:call() Method
Arguments and return values that were tables with x/y/z/w components are now Vec/Quat userdata.

**OLD:**
```lua
local staticScript = FS:get(scriptHandle)
local result = staticScript:call("calculatePosition", {x=1, y=2, z=3}, 5.0)
-- result might be {x=10, y=20, z=30}
```

**NEW:**
```lua
local staticScript = FS:get(scriptHandle)
local result = staticScript:call("calculatePosition", Vec3(1, 2, 3), 5.0)
-- result is now Vec3 userdata if the static script returns a Vec3
```

### 3. Implementation Details

#### C++ Side Changes
1. **New helper functions in tinyScript_bind.hpp:**
   - `pushVec2(lua_State* L, const glm::vec2& vec)`
   - `pushVec3(lua_State* L, const glm::vec3& vec)`
   - `pushVec4(lua_State* L, const glm::vec4& vec)`
   - `pushQuat(lua_State* L, const glm::quat& quat)`
   - `getVec2(lua_State* L, int index)`
   - `getVec3(lua_State* L, int index)`
   - `getVec4(lua_State* L, int index)`
   - `getQuat(lua_State* L, int index)`
   - `lua_Vec2(lua_State* L)` - Constructor
   - `lua_Vec3(lua_State* L)` - Constructor
   - `lua_Vec4(lua_State* L)` - Constructor
   - `lua_Quat(lua_State* L)` - Constructor
   - `vec2_tostring(lua_State* L)` - __tostring metamethod
   - `vec3_tostring(lua_State* L)` - __tostring metamethod
   - `vec4_tostring(lua_State* L)` - __tostring metamethod
   - `quat_tostring(lua_State* L)` - __tostring metamethod

2. **Updated in tinyScript.cpp:**
   - `cacheDefaultVars()`: Now checks for Vec2/Vec3/Vec4 userdata instead of tables
   - `update()`: Pushes Vec2/Vec3/Vec4 as userdata instead of tables
   - `update()`: Pulls Vec2/Vec3/Vec4 from userdata instead of tables

3. **Registration:**
   - Vec2, Vec3, Vec4, Quat metatables registered in `registerNodeBindings()`
   - Global constructor functions registered: `Vec2()`, `Vec3()`, `Vec4()`, `Quat()`

### 4. Benefits of This Rework

1. **Type Safety**: No more confusion between arrays and vectors
   - `{1, 2, 3}` is now a plain array
   - `Vec3(1, 2, 3)` is explicitly a 3D vector

2. **Better Error Messages**: Type mismatches now show clear errors like "setPos expects Vec3"

3. **Performance**: Slightly better performance as vectors are stored as compact userdata instead of tables

4. **Clearer API**: The API is now more explicit about what types are expected

5. **Future-Proof**: Easy to add methods to Vec types later (e.g., `vec:length()`, `vec:normalize()`)

### 5. Migration Guide for Existing Scripts

To migrate existing Lua scripts:

1. Replace all table constructors for positions/rotations/scales with Vec constructors:
   ```lua
   -- OLD
   transform:setPos({x = 10, y = 20, z = 30})
   
   -- NEW
   transform:setPos(Vec3(10, 20, 30))
   ```

2. Update VARS declarations:
   ```lua
   -- OLD
   VARS = {
       position = {x = 0, y = 0, z = 0}
   }
   
   -- NEW
   VARS = {
       position = Vec3(0, 0, 0)
   }
   ```

3. Update quaternion function calls:
   ```lua
   -- OLD
   local q = quat_fromEuler({x = 0, y = 1.57, z = 0})
   
   -- NEW
   local q = quat_fromEuler(Vec3(0, 1.57, 0))
   ```

### 6. Debugging Tips

Use `print()` or `tostring()` to inspect Vec/Quat values:
```lua
local pos = Vec3(1, 2, 3)
print(pos)  -- Output: "Vec3(1.000, 2.000, 3.000)"

local q = Quat(1, 0, 0, 0)
print(q)    -- Output: "Quat(1.000, 0.000, 0.000, 0.000)"
```

### 7. What Remains the Same

- Float, int, bool, string variables still work exactly as before
- Handle types still work the same way
- All component methods that don't involve vectors/quaternions are unchanged
- Node hierarchy methods are unchanged
- Scene methods are unchanged

## Testing Recommendations

1. Test all existing Lua scripts and update them according to the migration guide
2. Test VARS system with Vec2/Vec3/Vec4 types
3. Test Transform3D getters and setters
4. Test Skeleton3D bone manipulation
5. Test quaternion utility functions
6. Test script:getVar() and script:setVar() with Vec types
7. Test staticScript:call() with Vec/Quat arguments and return values

## Notes

- The rework is complete and comprehensive
- All vec-related functionality now uses the new system consistently
- The old table-based approach is no longer supported
- This is a **breaking change** that requires updating all existing scripts
