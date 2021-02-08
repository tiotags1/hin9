
* fixes to make wordpress and phpmyadmin work
* added redirect output to logfile function
* bugfix some of the broken pipe errors

commit b8fd799c8bd8e007db7253d4a3d20d24c59bdcc4
* removed limit size to response header size
* bugfix: improved and fixed error handling
* bugfix: restricted post on file handler
* bugfix: segfault for post error handling

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
