
php_bin = "/usr/bin/php-cgi"

function timeout_callback (dt)
  current_date = os.date('%d/%m/%Y:%T %z')
end
current_date = os.date('%d/%m/%Y:%T %z')

index_files = {"index.html"}
fpm_apps = {}

require "lib.lua"

-- files in this directory get loaded in alphabetical order, file starting with '_' are ignored
require_dir ("config")

redirect_log (server_log, debug_level)

fpm_socks = fpm_socks or {}
for ext, fpm in pairs (fpm_apps) do
  if (type (fpm) == "table") then
    fpm_socks[ext] = create_fcgi (fpm.uri, fpm.min, fpm.max)
  else
    fpm_socks[ext] = create_fcgi (fpm)
  end
end

access = create_log (access_log)
access ("start server on %s\n", os.date ("%F %T %z"))

init ()

