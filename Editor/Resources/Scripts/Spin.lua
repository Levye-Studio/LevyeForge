-- Add a Lua Script component, load this file, then press Play.
local speed = math.rad(45)

function on_update(dt)
    local x, y, z = entity:get_rotation()
    entity:set_rotation(x, y + speed * dt, z)
end
