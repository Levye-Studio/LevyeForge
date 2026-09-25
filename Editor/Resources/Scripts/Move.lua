-- Transform movement for an entity without a dynamic rigid body.
local speed = 3
function on_update(dt)
    local x, y, z = entity:get_position()
    if input.is_key_down(Key.W) then z = z - speed * dt end
    if input.is_key_down(Key.S) then z = z + speed * dt end
    if input.is_key_down(Key.A) then x = x - speed * dt end
    if input.is_key_down(Key.D) then x = x + speed * dt end
    entity:set_position(x, y, z)
end
