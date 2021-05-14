
-- log stuff
--redirect_log (nil, "ffffffff")
--redirect_log (server_log)

dofile "lib.lua"
dofile "config.lua"

php_bin = "/usr/bin/php-cgi"

function timeout_callback (dt)
  if (auto_ssl_renew) then
    auto_ssl_time_callback (dt)
  end
end

access = create_log (access_log)
access ("start server on %s\n", os.date ("%c"))

local server = create_httpd (function (server, req)
  local path, query, method, version = parse_path (req)
  local ip, port = remote_address (req)
  local id = get_option (req, "id")
  access ("%x %s %s %s %s\n", id, ip, method, path, query)

  set_option (req, "cache_key", path, "?", query)

  local app_path, sub_path = string.match (path, '^/(%w+)/?(.*)')
  if (app_path == "proxy") then
    return proxy (req, "http://localhost:28005/" .. (sub_path or ""))
  elseif (app_path == "testing") then
    return cgi (req, php_bin, nil, "test.php")
  elseif (path == "/hello") then
    return respond (req, 200, "Hello world")
  elseif (path == "/fcgi") then
    return cgi (req, "./build/fcgi_test", nil, "index.html")
  end

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
    return cgi (req, php_bin, nil, nil, path_info)
  elseif (to_cache[ext]) then
    set_option (req, "cache", 604800)
  end

  set_option (req, "debug", "0")
  send_file (req)

end, function (server, req, status, err)
  printf ("error callback called %d err '%s'\n", status, err)
  respond (req, 404, "URL could not be found on this server\n")

  local status = get_option (req, "status")
  access ("%x    error %s\n", id or -1, err)

end, function (server, req)
  local status = get_option (req, "status")
  local id = get_option (req, "id")
  access ("%x    status %d\n", id or -1, status or 1)
end)

ssl_sock = listen (server, nil, "8081", nil, "workdir/ssl/cert.pem", "workdir/ssl/key.pem")
listen (server, nil, "8080", "ipv4")

set_server_option (server, "timeout", 15)
set_server_option (server, "hostname", server_name)
set_server_option (server, "cwd", "htdocs")

if (ssl_sock == nil) then
  auto_ssl_do_renew ()
end

