
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

local server = create_httpd (function (server, req)
  local path, query, method, version = parse_path (req)
  --print ("path is ", path, method, query, version)
  --local h = parse_headers (req)
  --access ("%s %s %s\n", method, path, query)
  --for i, k in pairs (h) do
  --  print ("header ", i, k)
  --end
  --set_option (req, "keepalive", false)
  --set_option (req, "status", 403)
  --set_option (req, "cache", -1)
  --set_option (req, "disable", "keepalive")
  local ext = GetFileExtension (path)
  if (path == "/proxy") then
    return proxy (req, "http://localhost:28005/")
  elseif (path == "/test.test") then
    return cgi (req, "/usr/local/bin/fcgi_test", nil)
  elseif (path == "/test1.test") then
    return cgi (req, "/usr/local/bin/fcgi_test", "htdocs/test.php")
  elseif (path == "/cat") then
    return cgi (req, "/bin/cat", nil)
  elseif (path == "/hello") then
    return respond (req, 200, "Hello world")
  end
  local file_path = sanitize_path (req, "htdocs", path)
  --print (string.format ("file_path for '%s' ext '%s' res '%s'", path, ext, file_path))
  if (ext == "php") then
    return cgi (req, "/usr/bin/php-cgi", file_path)
  elseif (to_cache[ext]) then
    set_option (req, "cache", 604800)
  end
  send_file (req, file_path, 0, -1, function (req) end)
end)
--listen (server, "127.0.0.1", "8081", "workdir/cert.pem", "workdir/key.pem")
listen (server, "127.0.0.1", "8080")

set_server_option (server, "disable", "keepalive")



