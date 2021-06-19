hin9
====

hinsightd is a http/1.1 webserver designed for use with linux io_uring

main features are http1.1 pipelining, reverse proxy, local file-based cache, cgi, ssl, dynamic deflate, per request debug information, graceful restart, customizable logging, customizable cache control headers, customizable everything

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

* mkdir -p build && cd build && cmake .. && make && cd .. && build/hin9


roadmap
-------

* fastcgi
* static deflate caching


function reference
------------------

outdated

create\_httpd (request-callback, error-callback, finish-callback)
  * request-callback - is called when all the headers are gathered and the request is ready to be parsed
  * error-callback - is called when an error is encountered, atm only on 404 for static files
  * finish-callback - when the request is done
  * return a server object

listen (server-object, bind address, port, ipv6-specifier, ssl-data)
  * bind address/port in string format usable for getaddrinfo
  * ipv6 specifier, either "ipv4", "ipv6", "any", nil
  * ssl data is a path to a ssl cert and ssl key
  * returns a server socket object

redirect\_log (log path, debug mask)
  * log path - a file path for where to redirect stdout & stderr (optional)
  * debug mask - a string that represents a mask for what debug output to show (optional)
    * valid values are "0", "ffffffff"

set\_server\_option (server-object, option-name, option-value)

set\_server\_option (server, "enable", feature)

set\_server\_option (server, "disable", feature)
  * enable/disable a certain feature on the whole server

get\_server\_option (server-object, option-name)

get\_server\_option (server, "enable", feature)
  * returns if feature is enabled or disabled

set\_server\_option (server, "timeout", timeout)
  * number of seconds to keep clients after a request is done

set\_server\_option (server, "hostname", feature)
  * set server hostname

set\_option (request-object, option-name, option-value)

set\_option (request-object, "enable", feature)

set\_option (request-object, "disable", feature)
  * enable/disable a certain feature for a single request

set\_option (request-object, "status", http status number)
  * sets http status

set\_option (request-object, "cache\_key", cache uri)
  * identifies a cache object, uri is hashed and if caching is requested it will be used to identify cache object

set\_option (request-object, "cache", time in seconds/string)
  * set a cache options, if value is a:
    * positive number it sets cache-control as 'public, max-age=number, immutable'
    * negative number it sets cache-control as 'no-cache, no-store'
    * string it parses like a cache-control header (does not use string raw)

get\_option (request-object, option-name)

get\_option (request-object, "id", feature)
  * returns a request unique id (atm just the socket number)

get\_option (request-object, "status", feature)
  * returns http status

get\_option (request-object, "keepalive", feature)
  * returns if keepalive is enabled

get\_option (request-object, "enable", feature)
  * returns if feature is enabled

values for enable/disable
  * keepalive
  * range - range requests
  * modified\_since - sending modified headers
  * etag
  * cache - sending cache headers to clients
  * post
  * chunked - http1.1 chunked encoding
  * deflate
  * date - sending server date/time header
  * chunked\_upload - does anyone use this ?
  * local\_cache - server-side cache

parse\_path (request-object)
  * returns path, query string, method, http version

parse\_headers (request-object)
  * returns headers as a lua hashtable

send\_file (request-object, path, start offset, byte count)
  * sends file at path to client

proxy (request-object, uri)
  * proxies a request for uri to client
  * uri is in the form of "http(s)://host(:port)/request"

cgi (request-object, executable path, htdocs / root dir, file path)
  * executes cgi program and pipes the output to client

respond (request-object, http status code, optional body)
  * returns a raw message to client expl respond (req, 403, "don't look")

sanitize\_path (request-object, htdocs / root dir, path)
  * returns dir path relative to root dir, file name, file extension

remote\_address (request-object)
  * returns ip address, port number

add\_header (request-object, name, value)
  * adds header to request

shutdown (request-object)
  * closes connection without response

set\_content\_type (request-object, content type string)
  * sets content type returned




