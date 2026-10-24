-- Execute the actual sample script with engine adapters. These tests cover its
-- input/lifecycle policy, not Android hardware or Defold's renderer.
local starts, stops, released, hits = 0, 0, {}, 0
local frame = {tracking=false, fresh=false, status="permission_denied", detail="Camera denied", planes={}, anchors={}}
local width, height = 720, 1280
hash = function(v) return v end
msg = {post=function() end}
go = {}
window = {get_size=function() return width,height end}
for _, key in ipairs({"FOCUS_LOST","ICONIFIED","FOCUS_GAINED","DEICONIFIED","RESIZED"}) do window["WINDOW_EVENT_"..key] = key end
local listener
window.set_listener = function(fn) listener=fn end
arcore = {
    start=function() starts=starts+1; return true end,
    stop=function() stops=stops+1; return true end,
    poll=function() return frame, {message="buffer failed"} end,
    release_anchor=function(h) released[#released+1]=h; return true end,
    hit_test=function() hits=hits+1; return {} end,
}
package.preload["arcore_render.camera"] = function() return {new=function() return {} end, destroy=function() end} end
dofile("example/main/main.script")
local self={}
init(self);update(self,0.016)
assert(starts==1 and not self.tracking)
-- Recovery controls must survive an update while permission/tracking is absent.
on_input(self,"tap",{pressed=true,screen_x=600,screen_y=60})
update(self,0.016)
on_input(self,"tap",{released=true,screen_x=600,screen_y=60})
assert(starts==2 and stops==2)
self.handles={"first","second"}
on_input(self,"tap",{pressed=true,screen_x=100,screen_y=60})
update(self,0.016)
on_input(self,"tap",{released=true,screen_x=100,screen_y=60})
assert(#self.handles==1 and self.handles[1]=="first" and released[1]=="second")
-- A drag, resize or focus loss must not trigger a reset.
for _, action in ipairs({"drag","resize","focus"}) do
    on_input(self,"tap",{pressed=true,screen_x=600,screen_y=60})
    if action=="drag" then on_input(self,"tap",{screen_x=500,screen_y=60})
    elseif action=="resize" then listener(nil,window.WINDOW_EVENT_RESIZED)
    else listener(nil,window.WINDOW_EVENT_FOCUS_LOST) end
    on_input(self,"tap",{released=true,screen_x=600,screen_y=60})
    assert(starts==2)
    listener(nil,window.WINDOW_EVENT_FOCUS_GAINED)
end
-- A world gesture is canceled by tracking loss, unlike a recovery control.
frame.tracking=true;update(self,0.016)
on_input(self,"tap",{pressed=true,screen_x=360,screen_y=700})
frame.tracking=false;update(self,0.016)
on_input(self,"tap",{released=true,screen_x=360,screen_y=700})
assert(hits==0)
-- Poll errors gate world input but preserve a reset gesture.
on_input(self,"tap",{pressed=true,screen_x=600,screen_y=60})
frame=nil;update(self,0.016)
on_input(self,"tap",{released=true,screen_x=600,screen_y=60})
assert(starts==3 and not self.tracking)
final(self);assert(listener==nil)
print("PASS actual sample Lua: denied-permission reset, independent remove, drag/resize/focus cancellation, tracking/poll-error gates")
