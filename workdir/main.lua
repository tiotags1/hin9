
function create_log (path)
  local fp = io.open (path, "w")
  return function (...)
    fp:write (os.date ("%X "), string.format (...))
    fp:flush ()
  end
end

access = create_log ("build/access.log")
access ("start server on %s\n", os.date ("%c"))

function GetFileExtension (url)
  return url:match("^.+%.(.+)$")
end

to_cache = {ico=true, txt=true, js=true, jpg=true, png=true, css=true}
undeflate = {ico=true, jpg=true, png=true, bin=true, iso=true}

local server = create_httpd (function (server, req)
  local path, query, method, version = parse_path (req)
  --print ("path is ", path, method, query, version)
  --local h = parse_headers (req)
  --local ip, port = remote_address (req)
  --access ("%s %s %s %s\n", ip, method, path, query)
  --for i, k in pairs (h) do
  --  print ("header ", i, k)
  --end
  --set_option (req, "status", 403)
  --set_option (req, "cache", -1)
  --add_header (req, "Hello", "from server")
  local ext = GetFileExtension (path)
  local proxy_path = string.match (path, '^/proxy/(.*)')
  if (path == "/proxy" or proxy_path) then
    return proxy (req, "http://localhost:28005/" .. (proxy_path or ""))
  elseif (path == "/test.test") then
    return cgi (req, "/usr/local/bin/fcgi_test", nil)
  elseif (path == "/test1.test") then
    return cgi (req, "/usr/local/bin/fcgi_test", "htdocs/test.php")
  elseif (path == "/cat") then
    return cgi (req, "/bin/cat", nil)
  elseif (path == "/hello") then
    return respond (req, 200, "Hello world")
  end
  local root = "htdocs"
  local file_path, file_name = sanitize_path (req, root, path)
  if (undeflate[ext]) then
    set_option (req, "disable", "deflate")
  end
  if (ext == "php") then
    return cgi (req, "/usr/bin/php-cgi", root, file_path)
  elseif (to_cache[ext]) then
    set_option (req, "cache", 604800)
  end
  send_file (req, file_path, 0, -1)
end)

--[[
  listen
  1st param: server object
  2nd param: bind address
  3rd param: port number
  4th param: ipv4, ipv6, any/nil
  5th and 6th param: ssl certificate and ssl key
]]
--listen (server, "localhost", "8081", nil, "workdir/cert.pem", "workdir/key.pem")
listen (server, "localhost", "8080", "ipv4")
--listen (server, nil, "8080", "ipv4")
--listen (server, nil, "8080", "any")

set_server_option (server, "timeout", 15)
--set_server_option (server, "disable", "keepalive")
--set_server_option (server, "disable", "deflate")



