
do_proxy = create_round_robin ("http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/")

--[[
-- complicated ?
map (main, "/proxy/*", 0, function (req)
  if (do_proxy == nil) then return nil end

  local path, query, method, version, host = parse_path (req)
  local app_path, sub_path = string.match (path, '^/(%w+)/?(.*)')
  return do_proxy (req, (sub_path or "") .. (query or ""))
end)]]

local vhost = add_vhost {
host = {"proxy"},
htdocs = "htdocs",
}

map (vhost, "*", 0, function (req)
  if (do_proxy == nil) then return nil end

  local path, query, method, version, host = parse_path (req)
  return do_proxy (req, string.format ("%s?%s", path or "", query or ""))
end


