require 'core/class'

local Transform = cetech.Transform
--local Camera = cetech.Camera
local Mat4f = cetech.Mat4f
local Vec3f = cetech.Vec3f
local Quatf = cetech.Quatf
local Log = cetech.Log

FPSCamera = class(FPSCamera)

function FPSCamera:init(world, camera_unit, fly_mode)
    self.world = world
    self.unit = camera_unit
    --  self.camera = Camera.get(world, camera_unit)
    self.transform = Transform.get(world, camera_unit)

    self.fly_mode = fly_mode or false
end

function FPSCamera:setFlyMode(enable)
    self.fly_mode = enable
end

function FPSCamera:update(dt, dx, dy, updown, leftright)
    local pos = Transform.get_position(self.world, self.transform)
    local rot = Transform.get_rotation(self.world, self.transform)

    local m_world = Transform.get_world_matrix(self.world, self.transform)

    local x_dir = m_world.x
    local z_dir = -m_world.z

    -- Position
    if not self.fly_mode then
        z_dir.y = 0.0
    end

    pos = pos + z_dir * updown * dt
    pos = pos + x_dir * leftright * dt

    -- Rotation
    local rotation_around_world_up = Quatf.from_axis_angle(Vec3f.unit_y(), dx * 1 * 1)
    local rotation_around_camera_right = Quatf.from_axis_angle(x_dir, -dy * 1 * 1)
    local rotation = rotation_around_world_up * rotation_around_camera_right

    Transform.set_position(self.world, self.transform, pos)
    Transform.set_rotation(self.world, self.transform, rotation * rot)
end