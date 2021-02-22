
--redirect_log ("/tmp/log1.txt")
--redirect_log (NULL, "ffffffff")

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

function timeout_callback (dt)
end

access = create_log ("build/access.log")
access ("start server on %s\n", os.date ("%c"))

to_cache = {ico=true, txt=true, js=true, jpg=true, png=true, css=true}
to_deflate = {html=true, css=true, js=true, txt=true}
content_type = {html="text/html", jpg="image/jpeg", png="image/png", gif="image/gif", txt="text/plain", css="text/css", ico="image/vnd.microsoft.icon", js="text/javascript",
svg="image/svg+xml"}

local server = create_httpd (function (server, req)
  local path, query, method, version = parse_path (req)
  --local h = parse_headers (req)
  local ip, port = remote_address (req)
  local id = get_option (req, "id")
  set_option (req, "cache_key", path)
  access ("%x %s %s %s %s\n", id, ip, method, path, query)

  local root = "htdocs"
  local app_path, sub_path = string.match (path, '^/(%w+)/?(.*)')
  if (app_path == "proxy") then
    return proxy (req, "http://localhost:28005/" .. (sub_path or ""))
  elseif (path == "/hello") then
    return respond (req, 200, "Hello world")
  end
  local file_path, file_name, ext = sanitize_path (req, root, path, "index.html")
  if (to_deflate[ext]) then
  else
    set_option (req, "disable", "deflate")
  end
  if (app_path == "wordpress") then
    file_path, file_name, ext = sanitize_path (req, root, path, "index.php")
    local dir_path = string.match (sub_path, '^([%w-]+)')

    if (dir_path == "archives" or dir_path == "wp-json") then
      return cgi (req, "/usr/bin/php-cgi", root, root.."/wordpress/index.php")
    elseif (ext == "php") then
      return cgi (req, "/usr/bin/php-cgi", root, file_path)
    end
  end
  if (ext == "php") then
    return cgi (req, "/usr/bin/php-cgi", root, file_path)
  elseif (to_cache[ext]) then
    set_option (req, "cache", 604800)
  end
  if (content_type[ext]) then
    set_content_type (req, content_type[ext])
  end
  send_file (req, file_path)

end, function (server, req, status, err)
  printf ("error callback called %d err '%s'\n", status, err)
  respond (req, 404, "URL could not be found on this server")

end, function (server, req)
  local status = get_option (req, "status")
  local id = get_option (req, "id")
  access ("%x    status %d\n", id, status)
end)

--listen (server, "localhost", "8081", nil, "workdir/cert.pem", "workdir/key.pem")
listen (server, "localhost", "8080", "ipv4")
--listen (server, nil, "8080", "ipv4")
--listen (server, nil, "8080", "any")

set_server_option (server, "timeout", 15)
set_server_option (server, "hostname", "local")
--set_server_option (server, "disable", "keepalive")
--set_server_option (server, "disable", "deflate")



