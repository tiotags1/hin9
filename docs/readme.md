hin9
====

hinsightd is a http/1.1 webserver designed for use with linux io_uring

main features are http1.1 pipelining, reverse proxy, local file-based cache, cgi and fastcgi, ssl, dynamic deflate, per request debug information, graceful restart, customizable logging, customizable cache control headers, customizable everything

uses lua for everything related to configuration

fun features that can be implemented just in lua and have been left out as an excercise to the user
* rewrites
* per vhost logging
* zero or full logging and everything in between
* basic auth
* most kinds of load balancing

whenever possible coherency, ease of understanding and algorithm aesthetic are prioritized over speed, optimization or features


requirements
------------

* linux kernel >5.6 (march 2020), liburing, lua (5.1-5.4), libz
* optional: openssl/libressl
* cmake build system for compilation


install & run
-------------

`mkdir -p build && cd build && cmake .. && make && cd .. && build/hin9`

download mode
-------------

you can use the program as a http/1.1 downloader
* download and show to console: `build/hin9 -d _url_`
* download and save to file: `build/hin9 -do _url_ _path_`
* download multiple files: `build/hin9 -dodo _url1_ _path1_ _url2_ _path2_`
* multiple files 2: `build/hin9 -do _url1_ _path1_ -do _url2_ _out2_`
* download with progress bar: `build/hin9 -dopdo _url1_ _path1_ _url2_ _path2_`

simple server mode
------------------

can also be used just to serve the current directory

`cd htdocs && _build/hin9 --serve 8080_`

configuration
-------------

* cmake options: enable/disable ssl, cgi, fastcgi, reverse proxy code
* lots of knobs can be found in src/hin/conf.h

roadmap
-------

* static deflate caching

