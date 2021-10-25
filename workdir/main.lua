
php_bin = "/usr/bin/php-cgi"

function timeout_callback (dt)
  current_date = os.date('%d/%m/%Y:%T %z')
end
current_date = os.date('%d/%m/%Y:%T %z')

index_files = {"index.html"}
fpm_apps = {}

require "lib.lua"
require_dir ("config")

redirect_log (server_log, debug_level)

fpm_socks = fpm_socks or {}
for ext, uri in pairs (fpm_apps) do
  fpm_socks[ext] = create_fcgi (uri)
end

access = create_log (access_log)
access ("start server on %s\n", os.date ("%F %T %z"))

init ()

