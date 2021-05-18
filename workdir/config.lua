
-- basic
--server_log = logdir.."debug.log"
--debug_level = "ffffffff"
access_log = logdir .. "access.log"
server_name = "localhost"

-- ssl
auto_ssl_script = "/app/scripts/ssl.sh"
auto_ssl_renew = 60*60*24*30

-- content
to_cache = {ico=true, txt=true, js=true, jpg=true, png=true, css=true}

to_deflate = {html=true, css=true, js=true, txt=true}

content_type = {html="text/html", jpg="image/jpeg", png="image/png", gif="image/gif", txt="text/plain", css="text/css", ico="image/vnd.microsoft.icon", js="text/javascript",
svg="image/svg+xml"}


