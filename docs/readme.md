hin9
====

a linux webserver using liburing, ssl, dynamic deflate, http1.1 pipelining, 304 status

uses lua for configuration and scripting

server tries to mitigate DDoS effects by offering a very robust way of dropping clients and mitigating the effects of keeping too many connections open, should offer good performance for video serving, web file uploading


requirements
------------

linux kernel >5.6 (march 2020), liburing, liblua (5.1 ?), libz, openssl/libressl, ninja build, and the libs in the other repo


roadmap
-------

* caching reverse proxy
* fastcgi
* better daemon support

