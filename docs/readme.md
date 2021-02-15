hin9
====

hinsightd a linux webserver using liburing, ssl, dynamic deflate, http1.1 pipelining, 304 status, simple reverse proxy functions, simple (and inefficient) cgi support

uses lua for configuration and scripting, setting a file to be cached is as simple as "set\_option (req, "cache", num-seconds)" or adding a custom header "add\_header (req, "Hello", "from server")"

server tries to mitigate DDoS effects by offering a very robust way of dropping clients and mitigating the effects of keeping too many connections open, should offer good performance for video serving, web file uploading

whenever possible coherency, ease of understanding and algorithm aesthetic are prioritized over speed, optimization or features


requirements
------------

linux kernel >5.6 (march 2020), liburing, lua (5.1 ?), libz
optional: openssl/libressl
ninja build system for compilation


install & run
-------------

cd build && ninja && cd ..
build/hin9


roadmap
-------

* caching reverse proxy
* fastcgi
* better daemon support

