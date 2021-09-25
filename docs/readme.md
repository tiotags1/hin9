hin9
====

hinsightd is a http/1.1 webserver designed around the linux async api io_uring. It tries to solve complicated problems with simple solutions.

It has most of the features you'd expect out of more mature server but in a lighter format: http1.1 pipelining, reverse proxy, local file-based cache, cgi and fastcgi, ssl, dynamic deflate, per request debug information, graceful restart. And some exotic features like a http downloader.

The server core is written in c but the main decisions related to what each request does is handled by scripts interpreted by Lua. This offers a flexible way to write 'plugins', examples include: custom logging formats, per vhost logging, different load balancing strategies, http authentication, rewrites, timed callback actions, and other fun exercises.

Whenever possible code prioritizes coherency, ease of understanding and algorithm beauty over speed, optimization or features. Spaces for indenting and tabs for aligning. Code comments are rare.

requirements
------------

* linux kernel 5.6 (march 2020), liburing, lua (5.1-5.4), libz
* optional: openssl/libressl
* cmake build system for compilation


install & run
-------------

`mkdir -p build && cd build && cmake .. && make && cd .. && build/hin9`

download mode
-------------

you can also use the program as a http/1.1 downloader
* download and show to console: `build/hin9 -d _url_`
* download and save to file: `build/hin9 -do _url_ _path_`
* download multiple files: `build/hin9 -dodo _url1_ _path1_ _url2_ _path2_`
* multiple files 2: `build/hin9 -do _url1_ _path1_ -do _url2_ _out2_`
* download with progress bar: `build/hin9 -dopdo _url1_ _path1_ _url2_ _path2_`

simple server mode
------------------

Can also be used just to serve the current directory without any kind of config. Skipping config file also skipps any kind of mime type matching, http compression, logging, etc.

`cd htdocs && ../build/hin9 --serve 8080`

compilation options
-------------------

* cmake options: enable/disable ssl, cgi, fastcgi, reverse proxy code
* misc settings can be found in src/hin/conf.h


