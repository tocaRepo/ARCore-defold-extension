-- Optional helper. Call from a game-object script, before the next arcore.poll.
-- The consumer owns its render script and sends the matching matrices itself.
local M = {}
local next_texture = 0

function M.new(mesh_url)
    return {mesh = msg.url(mesh_url)}
end

function M.upload(camera, frame)
    if not frame or not frame.fresh or not frame.complete then return false end
    if not camera.texture or camera.width ~= frame.width or camera.height ~= frame.height then
        next_texture = next_texture + 1
        local old = camera.texture
        camera.texture = resource.create_texture("/runtime/arcore-camera-" .. next_texture .. ".texturec", {
            type = graphics.TEXTURE_TYPE_2D, width = frame.width, height = frame.height,
            format = graphics.TEXTURE_FORMAT_RGB, num_mip_maps = 1,
        })
        camera.width, camera.height = frame.width, frame.height
        go.set(camera.mesh, "texture0", camera.texture)
        if old then resource.release(old) end
    end
    resource.set_texture(camera.texture, {
        type = graphics.TEXTURE_TYPE_2D, width = frame.width, height = frame.height,
        format = graphics.TEXTURE_FORMAT_RGB, num_mip_maps = 1,
    }, frame.pixels)
    go.set(camera.mesh, "uv0", frame.uv0)
    go.set(camera.mesh, "uv1", frame.uv1)
    go.set(camera.mesh, "uv2", frame.uv2)
    return true
end

function M.destroy(camera)
    if camera.texture then resource.release(camera.texture); camera.texture = nil end
end
return M
