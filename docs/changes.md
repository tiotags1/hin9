
commit 6138add176d8a6d823b5c154302d586fe4e0316b
* added virtual fs code to cache directory structure in ram
* rewrote the way files are selected, now you need to use set_path
* filling cgi path\_info and path\_translated
* wrote a c logging api so you don't have to rely on lua's io library
* added a compile-time option for max header line sent
* went from 11k to 17k reqs/sec

commit 19362717a3354fc6d372d0f4b5b32ae2370e5df8
* added random seed to cache hashtable
* removed more warnings

commit e44ae8a24d2ca783ec5c9c73d58853e17487899e
* bugfixes to cgi introduced by previous commit

commit a09f51255515223a98a5db3b74ef545308f92af9
* servers now have a current working dir independent of program cwd
* headers function returns nested tables for duplicate http headers
* new command prompt arguments --help, --version, --config
* cgi ignore X-CGI- headers
* gmtime_r replaces gmtime

commit 5c81f09e5ec9fef2bc5b71eaa1868e9654ff6e7f
* slightly better cgi environment
* added so_reuseaddr by default
* can load config file from the command prompt
* bugfix: a space broke cgi return status
* bugfix: cache key not taking query string into account
* os signal fixes
* added a simpler sample config and other documentation changes

commit 641bc14b61b65eae6a9102df8c88b3ea4ad347df
* improved per request debug logging
* improved some error messages

commit a7bf9191fd3e01cb2bf1bfa935b39bf821b1148e
* bugfix: post errors causing crashes
* option to limit chunked upload for cgi
* chunked decode edge cases handling
* per request debug logging

commit 2f754a5743f26f6b6021565a209b36e4d9f7acfb
* bugfixed: graceful restart
* lua option to toggle debug output
* gitlab markdown doesn't know what newlines are

commit 95f4357027f24f959ecd5dd2564b616772db6ef7
* cache control improvements
* cgi cache fixes
* bugfix: don't cache items marked as private (or not marked as public)
* function reference in readme

commit 59f28b6ec0d78563cb527d543c96b4c0556b816d
* reverse proxy caching
* removed linger, why doesn't close peer reset with buffered data ?

commit ce785167140d68cc4eb4d65c8ca06542119cf836
* cache can generate an etag by hashing content
* bugfix: send client cache header if cgi sent it originally but it's being served from the local cache
* test: null server

commit 13f398fa1a593b6a2b8abf883f00911a17b920ea
* cache max size
* cache remove items when they expire
* cache handling errors in cache\_item creation
* fixes to time callback

commit 2d22f713c82c512f84f6506f3f92d330d895625e
* timer duration customization

commit d3ab74f4662de8c99a67dca65c6aea4120eb648c
* cgi output cache (cache doesn't check a max or free items)
* option to disable local cache
* pipe count output bytes

commit 9ef4d28f233236083c921b8e5aa9a2ed3a0dea16
* documentation changes
* fixing clang errors

commit 7422660e2a454aa34646ac216e693639da78f0cc
* callback handler for 404 errors (only static files atm)
* option to disable continuous upload
* bugfix: finish callback shouldn't crash if you access headers or path

commit 6df538b3466abeb2bb6d4645101f7aa7fe3e7e5f
* http HEAD requests
* bugfix: proxy errors broke keepalives

commit 56382bc29585826c98fa097d4486725f27028e94
* lua callback for request finish, good for fancy logging (use of function involving headers and sending new requests are undefined, will likely crash)
* added lua get_option for http status and request id
* added http/1.1 continuous upload, removed restriction to only content-length uploads (not tested, don't have anything that does that)
* added timeout callback, good for cron jobs ?
* bugfix: POST http method forbidden on error messages

commit eae3c284c332bd5f7ca553cb4369d6741e83043c
* sanitize_path you can set the index file to something different than index.html
* bugfix: path parsing was missing ,;
* bugfix: cgi http status was broken at one point
* bugfix: responses with large headers are no longer broken
* bugfix: timer callback corrupted the heap after the first second a program was running

commit fe3b92aeb7247ed22da8956bd095778cfba12e5f
* fixes to make wordpress and phpmyadmin work
* added redirect output to logfile function
* bugfix some of the broken pipe errors

commit b8fd799c8bd8e007db7253d4a3d20d24c59bdcc4
* removed limit size to response header size
* bugfix: improved and fixed error handling
* bugfix: restricted post on file handler
* bugfix: segfault for post error handling

commit 5d8c6b6f3ee9182df60b4e6a44342ac1bb4698ce
* proxy post fixes

commit ac66658427e2b8861f43be271f68dfb9149f3b18
* 50% chance ssl is fixed, it might buffer what it needs or it might buffer something else
* ui conditional openssl compilation, you can now remove openssl as a dependency !
* ui hide timer callbacks in debug
* ui no longer needs to initialize a client openssl context
* refactor unified connection reuse for proxy and downloads
* bugfix ssl initialization errors didn't abort http connection
* bugfix deflating of large files was broken due to order of operations

commit 88b63c6b4bb8e02454461759cd6dbaeb859e7823
* proxy support for http/1.1 chunked encodings
* backend ssl connections
* backend connection reuse
* config: deflate max size, worker prefork
* config: proper serverwide hostname
* refactored: pipe code, can now use 0 to signify EOF
* refactored: client connection code
* refactored: match_string
* bugfix: ssl renegotiation
* bugfix: shutdown only once
* bugfix: reuse backend connection not removing closed connections
* bug: ssl is broken, use is undefined

commit 6b2066a4d372bd5bc566572c9c04eae73e151a19
* proxy POST requests
* bugfix: missing '-' in request path parsing

commit bb3f09200bd4dd947324faee02204e9d2503fae7
* bugfix memory leak for a ssl buffer closed before operation finishes

commit 5b3853d48cc8d1139e4140f00e7c53248f9216cf
* set\_content\_type function
* bugfixes: add\_header, sanitize\_path

commit 814646024da2cf87f75d9f4055a676257e26f649
* style changes and probably 0.0001% performance

commit e20418ca053d857a74fae35ba64a4193c3584640
* proxy reuse backend connections
* bugfix deflate connections for large files
* bugfix crash for cgi deflate connection
* speak the daemon's true name

commit d8413ac553746055b6141c79c747bc13fb58353c
* connection 'shutdown' function that closes connection and returns nothing
* bugfix direct responses included deflate http headers erroneously
* bugfix double free when you return no response
* compile time configuration for 'main.lua' path

commit e07fd6cb1d87ce14d7e0f790c338341b80b0be5c
* enabled keepalive/pipelining for file, cgi and proxy via chunked encoding
* enabled deflate for file and proxy
* cgi deflate has a bug
* changed the way hin\_client\_t and httpd\_client\_t interact
* changed async connect function parameters
* configuration to disable x-powered-by

commit 9edb1d56262107c1ccfafaa32c8ee447658b9702
* proxy deletes previous headers and adds only default server headers
* message when graceful restart fails

commit 0fdc1fe55d649408c36e5273466aae3cae8099b3
* httpd common headers (date, server name, etc) so cgi and others can send date and servername
* compile time configuration for a few variable
* refactoring lines buffer (still buggy)
* removed shutdown call and replaced it with async close (buggy atm)
* replaced async statx with normal statx (+1000 req/sec woo)
* better handling of child death

commit c2cbe0ca558fdfcba321a3757c8e6a1c6df8d75b
* cgi proper header parsing
* cgi docroot
* cgi servername given from http host
* cgi deflate doesn't work yet
* better lines buffer
* minor refactoring, moved functions to httpd misc and httpd read

commit abf9a79884f23dfea1425036248f00178105441d
* graceful restart initial implementation
  * after 1-2 restarts liburing crashes in io_uring setup syscall claiming no resources
  * doesn't wait for new server to finish setup before it issues close
* added ability to request an ipv6 or ipv4 socket
* flush httpd timeout so it closes when you close the server
