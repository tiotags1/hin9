
-- logging
map (main, "*", 0, function (req)
  local path, query, method, version, host = parse_path (req)
  local ip, port = remote_address (req)
  local id = get_option (req, "id")
  access ("%x %s %s %s %s\n", id, ip, method, path, query)
end)
map (main, "*", 99, function (req)
  local status = get_option (req, "status")
  local id = get_option (req, "id")
  access ("%x    status %d\n", id or -1, status or 1)
end)

-- default map
map (main, "*", 0, function (req)
  local path, query, method, version, host = parse_path (req)

  set_option (req, "cache_key", path, "?", query)

  local dir_path, file_name, ext, path_info, location = set_path (req, path, "index.php", "index.html")
  if (location) then
    return redirect (req, location, 301)
  end
  if (file_name == nil) then
    if (dir_path) then
      return list_dir (req)
    else
      return respond (req, 404)
    end
  end

  if (to_compress[ext] == nil) then
    set_option (req, "disable", "compress")
  end
  if (content_type[ext]) then
    set_content_type (req, content_type[ext])
  end

  if (fpm_socks[ext]) then
    return fastcgi (req, fpm_socks[ext])
  elseif (to_cache[ext]) then
    set_option (req, "cache", 604800)
  end

  send_file (req)
  return true
end)

