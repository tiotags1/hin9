hin9
====

hinsightd is a HTTP/1.1 webserver designed to be light on resources but not light on features. It uses the new linux async API io\_uring for network and file access.

It has most of the features you'd expect out of more mature server: HTTP/1.1, ssl, reverse proxy, fastcgi and cgi, local caching for dynamic content, deflate compression, graceful restart. And some exotic features like being a simple command line http downloader.

The server core is written in C but the main decisions related to what each request does is handled by scripts interpreted by Lua. This offers a flexible way to write 'plug-ins', examples include: custom logging formats, per vhost logging, different load balancing strategies, http authentication, rewrites, timed callback actions, and other fun exercises.

Whenever possible code prioritizes coherency, ease of understanding and algorithm beauty over speed, optimization or features. Spaces for indenting and tabs for aligning. Code comments are rare.

more information [on the github pages site](https://tiotags1.github.io/tiotags1/)

security features
-----------------

To combat buffer overflows, server receive all network data in a single expanding buffer and process it in place using a single pattern matching library with proper bounds checking.

It only uses a single linux capability: `cap_net_bind_service` (bind to ports lower that 1024).

requirements
------------

* a recent linux kernel (>=5.6 - march 2020)
* liburing
* lua (5.1-5.4)
* libz
* optional: openssl/libressl, ffcall
* cmake build system for compilation


compile & run
-------------

`git clone https://github.com/tiotags1/hin9.git && cd hin9`

`mkdir -p build && cd build && cmake .. && make && cd .. && build/hin9`

download mode
-------------

you can also use the program as a HTTP/1.1 downloader
* download and print output: `build/hin9 -d _url_`
* download and save to file: `build/hin9 -do _url_ _path_`
* download multiple files: `build/hin9 -dodo _url1_ _path1_ _url2_ _path2_`
* multiple files 2: `build/hin9 -do _url1_ _path1_ -do _url2_ _out2_`
* download with progress bar: `build/hin9 -dopdop _url1_ _path1_ _url2_ _path2_`

simple server mode
------------------

You can also serve the current directory without a config file with the `--server port` command line argument. Doing so also bypasses any kind of MIME type matching, http compression, logging, etc

`cd htdocs && ../build/hin9 --serve 8080`

compilation options
-------------------

* cmake options: enable/disable ssl, cgi, fastcgi, reverse proxy code
* misc settings can be found in src/hin/conf.h


