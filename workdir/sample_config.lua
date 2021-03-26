
function printf (...)
  io.write (string.format (...))
end

access = create_log ("build/access.log")
access ("start server on %s\n", os.date ("%c"))

to_cache = {ico=true, txt=true, js=true, jpg=true, png=true, css=true}
to_deflate = {html=true, css=true, js=true, txt=true}
content_type = {html="text/html", jpg="image/jpeg", png="image/png", gif="image/gif", txt="text/plain", css="text/css", ico="image/vnd.microsoft.icon", js="text/javascript",
svg="image/svg+xml"}

local server = create_httpd (function (server, req)
  local path, query, method, version = parse_path (req)
  local ip, port = remote_address (req)
  access ("%x %s %s %s %s\n", id, ip, method, path, query)

  local id = get_option (req, "id")
  set_option (req, "cache_key", path, "?", query)

  local dir_path, file_name, ext, path_info = set_path (req, path, "index.php", "index.html")
  if (file_name == nil) then
    if (dir_path) then
      return respond (req, 403)
    else
      return respond (req, 404)
    end
  end

  if (to_deflate[ext] == nil) then
    set_option (req, "disable", "deflate")
  end
  if (content_type[ext]) then
    set_content_type (req, content_type[ext])
  end

  if (ext == "php") then
    return cgi (req, "/usr/bin/php-cgi", nil, nil, path_info)
  elseif (to_cache[ext]) then
    set_option (req, "cache", 604800)
  end

  send_file (req)

end, nil, function (server, req)
  local status = get_option (req, "status")
  local id = get_option (req, "id")
  access ("%x    status %d\n", id or -1, status or 1)
end)

listen (server, "localhost", "8081", nil, "workdir/cert.pem", "workdir/key.pem")
listen (server, "localhost", "8080")

set_server_option (server, "hostname", "localhost")
set_server_option (server, "cwd", "htdocs")



