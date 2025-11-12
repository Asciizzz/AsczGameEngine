-- ============================================
-- SKELETON3D & BONE API QUICK REFERENCE
-- ============================================

-- Get skeleton component from node
local skeleton = NODE:skeleton3D()  -- nil if not found

-- Skeleton methods
local count = skeleton:boneCount()
local bone = skeleton:bone(index)  -- nil if index out of range
skeleton:refreshAll()  -- Reset all bones to bind pose

-- ============================================
-- BONE LOCAL POSE (Modifiable)
-- ============================================

-- Position
local pos = bone:getLocalPos()       --> {x, y, z}
bone:setLocalPos({x=0, y=1, z=0})

-- Rotation (Euler angles in radians)
local rot = bone:getLocalRot()       --> {x, y, z}
bone:setLocalRot({x=0, y=1.57, z=0})

-- Rotation (Quaternion)
local quat = bone:getLocalQuat()     --> {x, y, z, w}
bone:setLocalQuat({x=0, y=0.707, z=0, w=0.707})

-- Scale
local scl = bone:getLocalScl()       --> {x, y, z}
bone:setLocalScl({x=1, y=1, z=1})

-- Full pose
local pose = bone:localPose()        --> {pos={...}, rot={...}, scl={...}}

-- ============================================
-- BONE BIND POSE (Read-Only)
-- ============================================

local bindPos = bone:getBindPos()    --> {x, y, z}
local bindRot = bone:getBindRot()    --> {x, y, z} (euler)
local bindQuat = bone:getBindQuat()  --> {x, y, z, w}
local bindScl = bone:getBindScl()    --> {x, y, z}
local bindPose = bone:bindPose()     --> {pos={...}, rot={...}, scl={...}}

-- ============================================
-- BONE HIERARCHY
-- ============================================

local parent = bone:parent()             -- Bone or nil
local parentIdx = bone:parentIndex()     -- integer or nil

local children = bone:children()         -- {Bone, Bone, ...}
local childIdxs = bone:childrenIndices() -- {0, 1, 2, ...}

-- ============================================
-- BONE INFO
-- ============================================

local idx = bone:index()    -- integer
local name = bone:name()    -- string

-- ============================================
-- COMMON PATTERNS
-- ============================================

-- Iterate all bones
for i = 0, skeleton:boneCount() - 1 do
    local bone = skeleton:bone(i)
    -- ...
end

-- Walk up parent chain
local current = bone
while current do
    print(current:name())
    current = current:parent()
end

-- Walk down children (recursive)
function visitBone(bone)
    print(bone:name())
    for _, child in ipairs(bone:children()) do
        visitBone(child)
    end
end

-- Rotate bone smoothly
local rot = bone:getLocalRot()
rot.y = rot.y + DELTATIME * 1.0
bone:setLocalRot(rot)

-- Quaternion interpolation
local current = bone:getLocalQuat()
local target = {x=0, y=1, z=0, w=0}
local result = quat_slerp(current, target, DELTATIME * 0.5)
bone:setLocalQuat(result)

-- Reset bone to bind pose
bone:setLocalPos(bone:getBindPos())
bone:setLocalQuat(bone:getBindQuat())
bone:setLocalScl(bone:getBindScl())
