
do_proxy = create_round_robin (server_remote, server_remote)

--[[
-- complicated ?
map (main, "/proxy/*", 0, function (req)
  if (do_proxy == nil) then return nil end

  local path, query, method, version, host = parse_path (req)
  local app_path, sub_path = string.match (path, '^/(%w+)/?(.*)')
  return do_proxy (req, (sub_path or "") .. (query or ""))
end)


local vhost = add_vhost {
host = {"proxy"},
htdocs = "htdocs",
}
]]

--local location = "" -- proxy everything from this host
local location = "/proxy" -- proxy only things inside this folder
local format = string.format ("^%s(/.*)", location)

map (nil, location .. "/*", 0, function (req)
  if (do_proxy == nil) then return nil end

  local path, query, method, version, host = parse_path (req)
  local sub_path = string.match (path, format)
  local url = sub_path or "/"
  if (query) then
    url = string.format ("%s?%s", url, query)
  end
  return do_proxy (req, url)
end)



