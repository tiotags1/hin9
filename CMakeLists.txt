cmake_minimum_required(VERSION 3.10)

# set the project name
project (hin9)

find_package (Lua REQUIRED)
include_directories (${LUA_INCLUDE_DIR})

add_definitions (-D_GNU_SOURCE -D_LARGEFILE_SOURCE -O2 -fno-omit-frame-pointer -Wall -Werror=vla)

include_directories ("src/hin/" "external/basic/" "external/basic/vfs/")

option (USE_OPENSSL "Use openssl" ON)

set (TARGET_LIBS ${TARGET_LIBS} ${LUA_LIBRARIES} uring z)

if (USE_OPENSSL)
  add_definitions (-DHIN_USE_OPENSSL)
  set (TARGET_LIBS ${TARGET_LIBS} crypto ssl)
ENDIF()

set (external_src
external/basic/vfs/basic_vfs.c
external/basic/vfs/dir.c
external/basic/vfs/inotify.c
external/basic/vfs/stat.c
external/basic/vfs/path.c
external/basic/vfs/compat.c
external/basic/basic_endianness.c
external/basic/basic_hashtable.c
external/basic/basic_pattern.c
external/basic/basic_timer.c
)

set (hin_src
src/http/cgi/fcgi.c
src/http/cgi/fcgi_read.c
src/http/cgi/fcgi_write.c
src/http/cgi/fcgi_util.c
src/http/cgi/cgi_parse.c
src/http/cgi/httpd_cgi.c
src/http/client/dl_head.c
src/http/client/dl_req.c
src/http/client/dl_client.c
src/http/utils/http_utils.c
src/http/utils/http_misc.c
src/http/utils/filters.c
src/http/server/httpd_read.c
src/http/server/httpd_req.c
src/http/server/httpd_head.c
src/http/server/httpd_proxy.c
src/http/server/httpd.c
src/http/server/httpd_file.c
src/http/cache/cache.c
src/system/console.c
src/system/ssl_init.c
src/system/signal.c
src/system/daemon.c
src/system/restart.c
src/system/main.c
src/system/epoll.c
src/system/vhost.c
src/system/timing.c
src/netcode/ssl.c
src/netcode/connect.c
src/netcode/uring.c
src/netcode/client.c
src/netcode/listen.c
src/netcode/buffer.c
src/netcode/pipe.c
src/lua/lua.c
src/lua/lua_config.c
src/lua/lua_opt.c
src/lua/lua_utils.c
src/lua/lua_vfs.c
src/lua/lua_req.c
src/lua/lua_os.c
)

add_executable (hin9 ${hin_src} ${external_src})
target_link_libraries (hin9 ${TARGET_LIBS})



