
-- ssl
-- default ssl cert is served when it doesn't match a vhost
cert = create_cert ("workdir/ssl/cert.pem", "workdir/ssl/key.pem")

main = add_vhost {
host = {"localhost"},
socket = {
  {bind=nil, port="8080", sock_type="ipv4"},
  {bind=nil, port="8081", sock_type="ipv4", ssl=true},
},
cert = cert,
htdocs="htdocs",
--hsts=600,
--hsts_flags="subdomains preload no_redirect no_header",
}


