
php_bin = "/usr/bin/php-cgi"

function timeout_callback (dt)
end

index_files = {"index.html"}
fpm_apps = {}

require "lib.lua"
require_dir ("config")

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

