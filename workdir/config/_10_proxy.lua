
do_proxy = create_round_robin ("http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/")

map (main, "/proxy/*", 0, function (req)
  if (do_proxy == nil) then return nil end

  local path, query, method, version, host = parse_path (req)
  local app_path, sub_path = string.match (path, '^/(%w+)/?(.*)')
  return do_proxy (req, (sub_path or "") .. (query or ""))
end)


