
-- ssl
-- default ssl cert is served when it doesn't match a vhost
cert = create_cert ("workdir/ssl/cert.pem", "workdir/ssl/key.pem")

main = add_vhost {
host = {"localhost"},
socket = {
  {bind=server_bind_host, port=server_http_port, sock_type="ipv4"},
  {bind=server_bind_host, port=server_https_port, sock_type="ipv4", ssl=true},
},
cert = cert,
htdocs = server_htdocs,
--hsts=600,
--hsts_flags="subdomains preload no_redirect no_header",
}


