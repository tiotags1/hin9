
* added pl, py, rb, cgi file extensions to forbidden list
* added the module name in benchmarks
* hammer fds now targets the root directory

v0.9.15
commit 4242aacbaa2b6bf1f4eadc8706079e2eb60fcde6
* tweaks

commit 99ddc9677c51b428455e758830d24c1b653b4001
* fixed misaligned packet condensing
* fixed potential out of memory segfaults, pipe queued infinite read requests
* new tests, bandwidth tests, 'too many files open'
* improved handling of out of fds for new connections
* removed -Os from cmake

commit 3a4271b764aaabf7c0421ca32f00d9a441f0ec49
* fixed rproxy backend not properly sending the close signal to chunked connections
* moved rproxy php hammer to separate test, can't make other servers serve >1000 php connections

commit 0456315f5d4aa2243efc948fa46d70ca1542e2e8
* greatly improved performance, now we're faster than nginx ! (for some use cases)
* improved testing infrastructure, better error messages, better logs, etc
* added a bunch of new tests, especially benchmarks
* fixed rproxy not sending HEAD requests

commit 71558f0fa1e434f3c7dd8d059b7f85536e250d65
* fixed segfault from writing the fcgi request multiple times
* better debug messages
* fixed php head test

commit 085567277214334b264286e3aa64740e343be2f6
* added hin netcode library example programs

commit 0071fff132d2920050e97edd277402d465a9c367
* refactored hin netcode library
* changed hin netcode library api
* added hin\_error and hin\_debug functions

commit dd9fe168b76158bb69cc63bf9b2633f015749e8c
* fixed being able to upload lots of files to fcgi at the same time, misinterpretation of fcgi spec, fcgi uploads are sequential only, aka now you can receive more simultaneous uploads than fcgi sockets
* fixed HEAD request (on static files)
* fixed not cleaning http client flags
* fixed basic pattern library consuming random '*' characters
* fixed parsing of debug masks
* added more tests
* tests check if server crashed, better logging, etc
* tweaked debug messages
* redirect log to file is now truncated on start and line buffered
* chased an use-after-free bug and ended up fixed like 5 things and now the new commit diff is unbearable

commit f04ca19742645fcc5f72c02ac0befb87d5a0d246
* added more tests
* php files are now banned from being served by default

commit b32447d4ac5bf811769b65fdef187631747e9804
* new tests
* added ability to only run a single test
* POST test actually POSTs
* centralized number of requests sent to ab in the main test.sh
* httpd made headers case insensitive

commit 4e9471ad52a530d817ffe7c331b3ca088d1cb831
* added some tests
* made the default config a bit easier to modify
* simplified default reverse proxy config
* fixed default index.html

commit 53e209bd61859915c5526fe00d2ed099b5952205
* fixed not properly exiting if listen sockets are waiting for the socket to become not busy
* fixed compilation errors

v0.9.14
commit f5cfd6c267572641678d1206e4efa17d581f4942
* fixed init.d script

commit 4f2bcd4030eebfc536e5b246e88f1d9e5f6e86a1
* fixed crash when handling some http error status codes
* fixed https client not connecting to port 443 by default
* fixed https client use after free, didn't handle the case where you finish the buffer as soon as you issue the read operation
* fixed https client memleak

commit 7da2363557e1f725e5b9068ea282d00e31235efb
* fixed cache use-after-free
* cache location now obeys temp dir directive
* rewrote argument handling

commit ce920ba6724dc39965d33b2f19a3c1bf985a324f
* fixed parsing of percent-encoded paths, aka now you can have japanese file names and space in filenames (I forgot about this one)
* simplified and improved the core pattern matching library (this does most of the parsing and should fix a few bugs)
*  also added better error messages for malformatted format strings
* parse\_path function now pushes url decoded path (real fs filename)
* parse\_path function now pushes version as a number instead of a string
* fixed set\_path not taking into account the filename passed
* fixed old cgi compilation

commit 2e1fac03d6d33081c49efd714ee2bad135706057
* fixed command line downloader

commit 112f72b8c3a5487fe9c672adc45466054bcf3883
* rproxy readded POST method
* rproxy passing client headers to the proxied server (aka cookies and exotic http bugs)
* note previous commit introduced bug in command line downloader

commit 825acb05a331602ef2299a1dca6c3587972627d0
* fixed crash if proxy tries to reuse an already closed connection (example proxied server closes idle keepalive connection but the proxying server does not know that yet and tries to reuse an invalid connection)
* fixed use-after-free when timed out if the close request is sync
* removed deprecated callback system from vhost (replaced by the more flexible mappings)
* added an active flag to all active buffers (this note is mostly useful mostly for bug tracking)
* lua added debug mask toggling functions so you can enable/disable specific debug functions through lua

commit 2d2ba43b479cf45eb11996de2f9821ad32420b69
* fixed proxy connection reuse
* other fixes due to improved code quality
* removed old http client code

commit c83db77b2819e3b10225dda01a5ea07daf732628
* fixed proxy code
* fixed proxy chunked encoding
* proxy added headers sent from proxyed server
* proxy still doesn't support POST-ing, connection reuse
* unified handling of chunking, made code easier to maintain but possible introduced bugs
* removed old unused reverse proxy code
* log ssl cert location to make it easier to debug problems with paths
* rewrote the proxy example, previous one didn't handle query strings properly

v0.9.13
commit 8d7f35233498880fcacf47e90cc42d2de7411f6a
* pipe code should now handle unexpected aborts slightly better
* fix: server no longer crashes when fcgi closes the connection before expected

commit 20392374e35d90545b0fa1d42a968bca530b01ea v0.9.13
* fix: use-after-free in cache code
* switched timing code to new dlist implementation
* about a year has passed since I released this and I have to say it's still filled with bugs

commit 94e18291ee82b78d761ce003281900656013ecb5
* moved timing code to base library
* fix: segfault when POSTing large files

commit 8987a65169ef2e6be2ba11a9971ebcccd9d43baf
* fix: fcgi proper cleanup when fcgi socket failes to connect

commit ec6a4549875a1c3dc48520eb529f2867fa4599d4
* fix: fcgi post stalling forever
* fix: use after free when freeing the cache

commit 2f780d491cc501b00ccb45573acc7f3eb94af596
* fix: not redirecting to folder when an index file is found/served
* fix: fcgi total fail when headers were larger than READ\_SIZE
* dlist append and prepend now accept multiple buffers (like it was designed to)

commit 3de03e0aea8142a29d39d3a80b73c3e6ce3a5d4a
* switched to a standardized linked list, many new undiscovered bugs added
* pipe code cleanup

commit 1e53f3a1e6dcfab3ff54aa0c55343620cff44fe3
* fcgi connection reuse, fcgi performance doubled (at least one test went from ~3000 to ~6000 reqs/s)
* fcgi code cleanup
* fcgi now properly handles queued connections

commit 32d7a8dc5956681da4c102996920e9a18162265b
* fix: fcgi no longer crashes if you POST more than ~4000 bytes
* fix: fcgi should now be more resilient to unexpected fcgi socket abort
* fix: library now properly tracks if it was built with openssl or not
* fix: deflate object memleak, zlib was not being freed when client finished

commit b17d60786a5e724d15fa92e1d023e215a1d97fac
* library component is no longer dependent on httpd\_vhost\_t for ssl sni negotiation, library is now stand alone (?)
* fix: ssl object memleak, ssl was not freed inside server client cleanup
* code: renamed hin\_vhost\_t to httpd\_vhost\_t and the functions that used it

commit bcba1d4ff5b5219016d2c95388d0cefb70575082
* simplified socket listen code
* graceful restart no longer requires a properly formatted shared memory region (but introduced a 1024 fd limit)

commit 4ac99fbe1dd10436fa39f68c7e064362d5057741
* fixes to library compilation, removed redundant functions that made library fail to link

commit 2b2c6c48559bba9bb19611c375014747336ca70d
* fix: should now properly install library and headers
* simplified listen code, stop server code, etc

commit f244648f8e54586b40c77267547025fbddbb0fc7
* split code into library part and a server part
* rewrote parts of the base networking code to make it more generic
* split hin.h into multiple headers
* regression: rewriting base logic is hard and affects lots of other parts, so expect bugs

commit a1acab0d18546703c9149ca10c28fc163c92d590
* use ffcall for lua log format specifiers
* fixed some issues with the non-ffcall lua log function
* create\_log function can now create a nil log by just passing a nil path, you can force the previous behaviour by passing the 3rd parameter as true
* changed header\_date function to take a strftime format instead of a header name

commit cf388dbe6d7c0c2c3eea1ea170d7846afc6c285c
* refactored reverse proxy code
* regression: no more post support
* regression: no connection reuse
* regression: no more rproxy cache
* rproxy: hopefully better connection reuse
* fixed cache ignoring vhost
* fixed vhost map maching being restricted to the first vhost causing 500 errors for all other vhosts

commit 8fcbc9927f2eb72c0fae28ac2bbb9db4f1dd15dc
* better support for common log format (added response size field and moved date)
* create\_log will no longer print date unless you pass true as a parameter
* documentation changes
* fixed previous commit not exiting properly when you use fcgi

v0.9.12
commit 0b14a77d6d56789b38697954dfcd426015d4c365
* fixed downloader exiting before it even began downloading
* changed default log format to be closer to apache format

commit 633297f6a25da604dd11f384b6026a7bcf98aede
* readme changes
* better proxy example
* fixed graceful reload

commit db14c5e01de0629234320b63b1ebea3328550e35
* progress bar show speed
* progress bar reduce update frequency
* progress bar human readable file sizes
* config allow easier adding of index files
* warn if started as root
* fix config error if fcgi isn't compiled

commit c77203bace58f08a2332db89d80462465261bcd4
* command line argument to start a no-config file server in current directory `--serve _port_`
* fastcgi enabled by default
* fixed logrotate script
* fixed compilation errors
* fixed the tabs vs spaces debate forever, use spaces for indenting and tabs for aligning

commit 6cdda6cf849642d343e469da552f2dadad347575
* config can now map urls to specific lua callbacks
* config can include all the lua files in a dir
* config files reorganized for new include system, old system is deprecated and will be removed in the future
* added list\_dir1, map and get\_vhost function to lua
* vhost add doesn't require an explicit parent anymore, uses default parent, default parent is the first vhost that listens to sockets
* fixed and improved error messages
* fixed possible lua stack overflow
* fixed issuing https redirect for vhosts that didn't have a callback

commit 5dcff74a40f57f76a4de13621156ddb8e985856c
* httpd made fcgi optional
* fixed compilation errors

commit 29738af7c8c43481fa79c6fca472373692fd1be5
* added download mode, where it downloads files and then exits
* made old cgi and reverse proxy code optional for extra small server
* httpd added verbose error messages and a config to control it
* fixed crashes if uring is used before program initialization
* testing -Os vs -O2

commit f04d7703f6cdbd2e33f8a7289d80a01dba5e970f
* removed some of the gcc -Wextra -pedantic warnings
* fixed debug level argument parsing

v0.9.11
commit d5942e9253436a8f3c60f414b04f274779ec0ff0
* httpd toggle to force requiring hostname like the http spec requires
* httpd post-ing created a tmp file that wasn't being used
* httpd new tests using raw netcat requests
* httpd more error logging switched to new system
* httpd added round robin proxying
* now creates directory automatically for log files, and option to disable that

commit 27d649787d27992afffc2b5e53cb7f84b53a55aa
* httpd added handling of multiple parameters in the connection header
* httpd ability to disable POST serverwide
* httpd added a more unified error handling
* bugfix a remote denial of service bug found in the handling of chunked encoding could lead to the crash of the affected service (error handling used 'exit' instead of properly handling the error)
* bugfix you can now post 0 length content, why would you do that I don't know but I've seen wordpress do it

commit 133f98d11c56a92a76161f339608c15047b4f858
* redirect folder '/folder' to '/folder/', added trailing '/' for URIs to folders
* option to control if server sends redirects for URIs to folders with missing trailing '/'
* added redirect function to lua
* console now parses lua directly without adding 'lua <command>'
* added app_state function to allow restarting and reloading from lua
* bugfix order of returns for set\_path function
* bugfix http url matches a few more (] is still missing due to match_pattern bugs)

v0.9.10
commit 47f242e485906c795ab77f00b57bdbd8509e1a4d
* checks rlimits MEMLOCK at runtime so you don't have to warn at install time
* also increased RLIMIT_NOFILE to something more reasonable and customizable

commit ca8a1e3ec9c636a350ff16a4ce441610b93e52a4
* bugfix header function didn't write the last character for a buffer of a very specific size, aka vsnprintf needs a buffer of size n+1

v0.9.9
commit d24aec6c4a7a5239af563d03ba6f0a1e3200b38c
* added gzip compression
* refactored fcgi lifecycle to make it easier in the future to add reusing sockets
* bugfix compressing php after php already compressed it
* bugfix fcgi not properly closing

commit 5a9959f4e6c254b8e9b5db728e2313663397b96d
* directory listing and option to enable it (disabled by default)
* files that give EPERM should now return 403 instead of 404
* directories that give EPERM should be handled better
* added test for download speed and php fcgi post
* bugfix httpd respond text handles now handles text larger than 4096 bytes

commit 555cd10c97ca8aa41ec8e9b3262bb5c8a47ed010
* better command line processing including short versions
* added better configuration handling for multiple fcgi groups
* moved missing ssl sni warn message to info level
* fixed crash when trying to serve from folders you don't have access to

commit 0c1fbdc264750aff0d9ec9dae899e93dcd5aaad9
* pid file should now only be writable at server start/restart (via pid_helper)
* should now properly abort if logfiles can't be opened

v0.9.8
commit 98446fe03d7da3364e5bf613b24b03a8d05d2a45
* added php compression by default
* bugfix fcgi crash due to improper buffer length handling
* bugfix use after free in cleanup
* bugfix memory leaks in vfs
* bugfix memory leak in deflate compression

commit e8a2edd159e2e58fe826fa025f1cd9a46ceef19e
* refactored fcgi
* run lua scripts from the console
* minor readme revision
* fixed set debug mask for running vhosts (redirect log might actually work)
* fixed epoll with files (epoll doesn't support files so just use sync)
* fixed musl system calls for other architectures besides x86

v0.9.7
commit 915945d97133c84af4f2ac63b7bbe96fabeceb6b
* fcgi uploads
* moved some headers around
* set default log level to only log errors and configuration
* bugfix fcgi not being able to send large responses

commit 1b8b8cd4a27e5de72791fafd06efe4b346ff7910
* fcgi fpm, 1 fpm connection per httpd client, no uploads, possible crashes, not enabled by default
* merged fcgi and cgi parsing, removed hin worker, replaced with hin fcgi worker
* exposed fcgi creation api to lua, you can point fcgi to whatever tcp server you want
* fixed some virtual pipe handling
* fixed fcgi caching

commit e6d10dd24021a390a51f604bd11c8fe9372f9625
* added a simple hsts api, aka a simple way to redirect to https
* added null logging to cheat on benchmarks (logging is too slow and fills the hdd)
* bugfix: hin pipe fixed free before close
* bugfix: segfault on exit when you don't use access logging

commit eb51beab1893f0e3544540dabd7abdbdf2f28dc4
* refactoring dynamic buffer

commit 9d7ae4ea5b5d601b67795c32e2e3ba2cadfbeb34
* better bugfix to proxy crash on double free, cleaning state
* moved proxy into it's own folder

commit a7c98aabab76cb3c423ec7a928cbef0edefb1f48
* bugfix proxy crashing due to double free (incomplete)
* fcgi switched to pipes (bad atm)
* changed some function prototypes to take a size parameter

commit 46ad0263fd23433434e16c76a9c0e637f0385210
* docker changed to using binary packages, reducing size by a lot
* bugfix timers not being removed properly caused the server to keep them forever

commit fcf50de3ee618191d64e1450578c831ee88e9a83
* removed old ebuild and just added an initd script

v0.9.6
commit b82656f1d4e8b306a2d6c905ae41e6955cd75bab
* fcgi fpm connections initial
* fcgi doesn't output headers or output properly
* renamed endianess to endianness

commit 73221373cc9253b91302e137544b7e376617efe5
* apache bench tests
* cache switched to use the new timing api

commit 90ef43094295760d20128956853394d596813455
* removed debug message when redirecting to console
* removed a few compiler warnings

v0.9.5
commit f5671c1c7a2190967314b355be95eb7a04a860b4
* cache_test.php to test if caching still works
* made connect slightly more general purpose
* added compile time option to start despite ssl vhosts without certificates
* --help and --version no longer exit with error code
* moved statx musl compatibility layer to basic_vfs
* forced in large file support mode for 32bit
* bugfix segfault on exit
* bugfix 32bit segfaults, should now work on 32bit

commit 0c5d5ce4e84d58ab15d9ff5d08beb2a216bd2904
* more error handling, oh my we were forgetting about some errors

commit 631df5f233190ab8033503ef32b0e78f4d01b85d
* added an initial 'proper' vhost api
  seems like the idea of handling vhosts purely in lua is bad considering things
  like ssl sni and proper cwd handling
* fixes to the new timing api

commit a6194916bb20f7bff5d7a42af9982b8dd00e013d
* rewrote the httpd timeout code to use lists (faster server at high load)
  resons for slow progress: bugs, more bugs, the existential dread of commiting
  a poor api that would later cause 50x the work for simple fixes and bugs
* better lua include/require error messages
* timing code simplification, removed perfect frame timing

v0.9.4
commit 8c55b6c18dffdb77148ea75cd6427a20103bb804
* changed logging a bit

commit 514c8f767e091bb2409237bf86ef2f9752ad0d51
* moved around some configuration values
* added quiet, loglevel and debugmask command line parameter
* added timestamp for each received connection

commit de4907df6d5d47d130f3c8e89f65685938ee0f59
* added a simple ssl sni implementation (serving multiple ssl sites on a single ip)
* added hostname tracking to request
* added separate function to load certificates

commit 05e4e1a44b1feae4f01086a21d6f5ed1d6ec2436
* fixed another ssl segfault bug

commit c0d01c0f3fbf57deafbf720599159ff99e5df540
* fixed embarassingly simple ssl keepalive bug
* certificates renewing improvements
* added ip address to debug logs

commit bd30c060b2f808e2929658cfdc5e0e8d3f381bd9
* lua include file
* bugfix logs now properly keep previous contents on server startup
* docker script for retreiving a letsencrypt ssl certificate (using bacme script)

commit 1e53bdf997b3d182612b5275178eb8fdbb1b8bb1
* os.execute replacement function now has a callback
* added more epoll support
* fixed compilation for lua5.1, again

commit cad1fae8bf84f04a6989238d774b085fd56908be
* switched signalfd on by default
* switched console on by default
* epollfd framework changes needed for above
* fancy lua os.execute replacement, more changes incoming, lua when are you adding this

commit ab0235295d83ac21242b84c87d5773352c0f183a
* server can now read new files added to htdocs without restart
* added inotify to basic_vfs
* added an epoll framework
* fixed serving of . files
* fixed sending chunked for 0 length files
* docker optimization

commit e239ca9e285e3065bcc04ee736a20cae1363977f
* docker container project

commit e370b38d734e881104e27da50b6cf3ae29c8f55e
* musl compatibility, weird way to force posix compliance musl

v0.9.3
commit efb5dd88e60588c712dfc90f042d3c3e75fbd45d
* fixed redirect log (again)
* compile option to disable greeting

v0.9.2
commit 4bb511fa407c3481af90286b23c357aaf1b1d231
* documentation changes

v0.9.1
commit f23205a0310b25e3c2d9dcd7a21c56ca7cfa8743
* abort program if you can't redirect log to file
* logdir, cwd and tmpdir forced to have trailing /
* abort program if fail to bind any of the requested sockets

commit 54787f932072c955da27d2146832fc0898d9d1e9
* switched from homegrown ninja files to cmake

commit 05bcf5207cd60e069b3dc5a3377cdc4d23b6239a
* fixed lua not being properly #include

v0.9.0
commit 0e1e88ddb00d2786730d400dd06b5e735f9ddd62
* fixed proxying of chunked encoded connections
* I thought I fixed this

commit f930a5587707797c2df8bdec3e821f40470cd37c
* bugfix something bad about cgi
* bugfix cgi hang if subprocess crashes before headers
* bugfix cgi not accepting a direct script path

commit 73acbf22e54ccea22f05c1209ca2a33bb37beec0
* ebuild changes
* more compile time options

commit 96e5fadd690547f26b0cadac1b7770b7b10fc044
* use linux capabilities
* set user to nobody

commit 8afb936fe7d7274663d75824cd9bdf5ecd498bec
* fixed init.d script

commit a61575b3a2b74b5324a757cfe03b39145c135542
* bugfix graceful restart should work now
* added restart to ebuild
* minor fixes

commit e425349e03940bdc607d048c7b35061f836d75ef
* command line option to check config (--pretend)
* initial graceful restart code
* cleanup listen code

commit 27c2c68780ab850321768bc3c6dce8a6a4cd8f9b
* gentoo ebuild and openrc init.d file
* option to set logdir path and starting cwd
* option to daemonize
* option to create pid file

commit 0ab4b80fcbbcd3ddf0dc932210a19e5838509058
* bugfix segfault due to high server load (insufficient sqe's v2)

commit 49d237d36b543dbb66c8865fbf902e8f325c7f66
* wordpress finally works
* cgi proper http headers transforming '-' to '_' <- this was fun
* cgi fixed script_name for directory indices

commit 55df0083b7e705acb540eada8d98d006dc90b8f3
* bugfix segfaults due to not enough sqe's

commit 0e199d7abaa7afdee0375690328d94455bb6d42b
* bugfixes to logging (still inserting random invalid characters)

commit 1fef5dbbf5d1402303ca0bb34fa6d45b4cfa9977
* separated http into multiple directories

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
