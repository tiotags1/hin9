
-- basic
--server_log = logdir.."debug.log"
--debug_level = "ffffffff"
access_log = logdir .. "access.log"
server_name = "localhost"

-- content
to_cache = {ico=true, txt=true, js=true, jpg=true, png=true, css=true}

to_deflate = {html=true, css=true, js=true, txt=true, php=true}

content_type = {html="text/html", jpg="image/jpeg", png="image/png", gif="image/gif", txt="text/plain", css="text/css", ico="image/vnd.microsoft.icon", js="text/javascript",
svg="image/svg+xml"}

-- ssl
-- default ssl cert is served when it doesn't match a vhost
cert = create_cert ("workdir/ssl/cert.pem", "workdir/ssl/key.pem")

if (cert) then
  -- if you find a cert just use it
  -- the server quits if you try to use the return of a failed create_cert
  ssl_enable = true
end


main = add_vhost {
--host = {"localhost"},
socket = {
  {bind=nil, port="8080", sock_type="ipv4"},
  {bind=nil, port="8081", sock_type="ipv4", ssl=true},
},
cert = cert,
htdocs="htdocs",
--onRequest = default_request_handler,
--onError = default_error_handler,
--onFinish = default_finish_handler,
--hsts=600,
--hsts_flags="subdomains preload no_redirect no_header",
}



