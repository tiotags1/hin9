
function create_log (path)
  local fp = io.open (path, "w")
  return function (...)
    fp:write (os.date ("%X "), string.format (...))
    fp:flush ()
  end
end

function printf (...)
  io.write (string.format (...))
end

local root = "htdocs"
local log_path = "build/access.log"

access = create_log (log_path)
access ("start server on %s\n", os.date ("%c"))

to_cache = {ico=true, txt=true, js=true, jpg=true, png=true, css=true}
to_deflate = {html=true, css=true, js=true, txt=true}
content_type = {html="text/html", jpg="image/jpeg", png="image/png", gif="image/gif", txt="text/plain", css="text/css", ico="image/vnd.microsoft.icon", js="text/javascript",
svg="image/svg+xml"}

local server = create_httpd (function (server, req)
  local path, query, method, version = parse_path (req)

  local ip, port = remote_address (req)
  local id = get_option (req, "id")
  access ("%x %s %s %s %s\n", id, ip, method, path, query)

  local file_path, file_name, ext = sanitize_path (req, root, path, "index.html")

  if (to_deflate[ext] == nil) then
    set_option (req, "disable", "deflate")
  end

  if (content_type[ext]) then
    set_content_type (req, content_type[ext])
  end

  if (ext == "php") then
    return cgi (req, "/usr/bin/php-cgi", root, file_path)
  elseif (to_cache[ext]) then
    set_option (req, "cache", 604800)
  end

  send_file (req, file_path)

end, nil, function (server, req)
  local status = get_option (req, "status")
  local id = get_option (req, "id")
  access ("%x    status %d\n", id or -1, status or 1)
end)

listen (server, "localhost", "8081", nil, "workdir/cert.pem", "workdir/key.pem")
listen (server, "localhost", "8080", nil)




