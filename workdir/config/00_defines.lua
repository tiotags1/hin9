
-- basic
--server_log = logdir.."debug.log"
--debug_level = "ffffffff"
access_log = logdir .. "access.log"
server_name = "localhost"

server_http_port = "8080"
server_https_port = "8081"
server_bind_host = "localhost"
server_htdocs = "htdocs"
server_remote = "http://localhost:28081/"

-- content
to_cache = {ico=true, txt=true, js=true, jpg=true, png=true, css=true}

to_compress = {html=true, css=true, js=true, txt=true, php=true}

content_type = {html="text/html", jpg="image/jpeg", png="image/png", gif="image/gif", txt="text/plain", css="text/css", ico="image/vnd.microsoft.icon", js="text/javascript", svg="image/svg+xml",
bin="application/octet-stream",
apng="image/apng", avif="image/avif", webp="image/webp"}

forbidden_files = {php=true, pl=true, cgi=true, py=true, rb=true}

-- if no index file is present try to list files inside the directory
--set_server_option (main, "directory_listing", true)

-- disable http compression, keepalives globally
--set_server_option (main, "disable", "compress")
--set_server_option (main, "disable", "keepalive")

-- disable redirecting http://localhost:8080/directory to http://localhost:8080/directory/
--set_server_option (main, "directory_no_redirect", true)

-- try to create log directory if missing
set_global_option ("create_directory", true)

-- print internal errors inside http response, not recommended
--set_global_option ("verbose_errors", true)


