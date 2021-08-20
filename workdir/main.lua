
php_bin = "/usr/bin/php-cgi"

function timeout_callback (dt)
  if (auto_ssl_renew) then
    auto_ssl_time_callback (dt)
  end
end

function default_onRequest_handler (req)
  local path, query, method, version, host = parse_path (req)
  local ip, port = remote_address (req)
  local id = get_option (req, "id")
  access ("%x %s %s %s %s\n", id, ip, method, path, query)

  set_option (req, "cache_key", path, "?", query)

  local app_path, sub_path = string.match (path, '^/(%w+)/?(.*)')
  if (app_path == "proxy") then
    return proxy (req, "http://localhost:28005/" .. (sub_path or ""))
  elseif (app_path == "testing") then
    return cgi (req, php_bin, nil, "test.php")
  elseif (path == "/fcgi") then
    set_path (req, "/tests/min.php", "index.php")
    return fastcgi (req, fpm_socks.php)
  elseif (path == "/hello") then
    return respond (req, 200, "Hello world")
  end

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
    --return cgi (req, php_bin, nil, nil, path_info)
    return fastcgi (req, fpm_socks[ext])
  elseif (to_cache[ext]) then
    set_option (req, "cache", 604800)
  end

  send_file (req)
end

function default_onError_handler (req)
  printf ("error callback called %d err '%s'\n", status, err)
  respond (req, 404, "URL could not be found on this server\n")

  local status = get_option (req, "status")
  access ("%x    error %s\n", id or -1, err)
end

function default_onFinish_handler (req)
  local status = get_option (req, "status")
  local id = get_option (req, "id")
  access ("%x    status %d\n", id or -1, status or 1)
end

require "lib.lua"
require "default_config.lua"
local err = include "config.lua"
if (err) then
  printf ("%s\n", err)
end

redirect_log (server_log, debug_level)

fpm_socks = fpm_socks or {}
for ext, uri in pairs (fpm_apps) do
  fpm_socks[ext] = create_fcgi (uri)
end

if (access_log) then
  access = create_log (access_log)
else
  access = nil_log ()
end
access ("start server on %s\n", os.date ("%c"))

init ()

