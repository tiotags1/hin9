
* bugs
  * ssl renegotiation
  * cgi and proxy don't support deflate, keepalives or chunked encoding
  * if responses are larger than READ_SZ (4096 atm) it won't print overflowing headers


* reverse proxy
  * cache
  * cache gzipped files


* cgi support
  * fastcgi


* daemon support
  * graceful restart
  * daemon detach from console
  * pid file file locking
  * better access log functions


* sandboxing
  * syscall filtering
  * capabilities
  * namespaces

