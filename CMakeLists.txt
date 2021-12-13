cmake_minimum_required(VERSION 3.10)

# set the project name
project (hin9)

find_package (Lua REQUIRED)
include_directories (${LUA_INCLUDE_DIR})

add_definitions (-D_GNU_SOURCE -D_LARGEFILE_SOURCE -Os -Wall -Werror=vla)

include_directories ("src/hin/" "src/hin/http/" "external/basic/" "external/basic/vfs/")

option (USE_OPENSSL "Use openssl" ON)
option (USE_FCGI "Compile fastcgi support" ON)
option (USE_CGI "Compile cgi support" OFF)
option (USE_RPROXY "Compile reverse proxy support" ON)
option (USE_FFCALL "Use ffcall library for lua log function (more format specifiers)" OFF)

set (TARGET_LIBS ${TARGET_LIBS} ${LUA_LIBRARIES} uring z)

if (USE_OPENSSL)
  add_definitions (-DHIN_USE_OPENSSL)
  set (TARGET_LIBS ${TARGET_LIBS} crypto ssl)
ENDIF()

if (USE_CGI)
  add_definitions (-DHIN_USE_CGI)
  set (other_src ${other_src} src/http/cgi/httpd_cgi.c)
ENDIF()

if (USE_FCGI)
  add_definitions (-DHIN_USE_FCGI)
  set (other_src ${other_src}
src/http/cgi/fcgi.c
src/http/cgi/fcgi_read.c
src/http/cgi/fcgi_write.c
src/http/cgi/fcgi_post.c
src/http/cgi/fcgi_socket.c
src/http/cgi/fcgi_util.c
src/http/cgi/fcgi_worker.c)
ENDIF()

if (USE_RPROXY)
  add_definitions (-DHIN_USE_RPROXY)
  set (other_src ${other_src} src/http/proxy/rproxy.c)
ENDIF()

if (USE_FFCALL)
  add_definitions (-DHIN_USE_FFCALL)
  set (TARGET_LIBS ${TARGET_LIBS} ffcall)
ENDIF()

set (external_src
external/basic/vfs/basic_vfs.c
external/basic/vfs/dir.c
external/basic/vfs/inotify.c
external/basic/vfs/stat.c
external/basic/vfs/path.c
external/basic/vfs/compat.c
external/basic/basic_args.c
external/basic/basic_endianness.c
external/basic/basic_hashtable.c
external/basic/basic_lists.c
external/basic/basic_pattern.c
external/basic/basic_timer.c
)

set (hin_src
src/netcode/header.c
src/netcode/hin.c
src/netcode/ssl.c
src/netcode/ssl_init.c
src/netcode/connect.c
src/netcode/epoll.c
src/netcode/uring.c
src/netcode/client.c
src/netcode/listen.c
src/netcode/buffer.c
src/netcode/pipe.c
)

set (hin_exe
src/http/cgi/cgi_parse.c
src/http/client/dl_client.c
src/http/client/download.c
src/http/client/headers.c
src/http/client/data.c
src/http/client/utils.c
src/http/utils/http_utils.c
src/http/utils/http_misc.c
src/http/utils/filters.c
src/http/server/httpd_read.c
src/http/server/httpd_req.c
src/http/server/httpd_head.c
src/http/server/httpd.c
src/http/server/httpd_file.c
src/http/server/vhost.c
src/http/cache/cache.c
src/system/console.c
src/system/signal.c
src/system/daemon.c
src/system/helper.c
src/system/restart.c
src/system/main.c
src/system/timing.c
src/lua/lua.c
src/lua/lua_config.c
src/lua/lua_opt.c
src/lua/lua_utils.c
src/lua/lua_vfs.c
src/lua/lua_req.c
src/lua/lua_os.c
src/lua/lua_vhost.c
src/lua/lua_map.c
)

configure_file(src/hin/system/config_int.h.in config_int.h @ONLY)

add_library (hinsightd ${hin_src} ${external_src})

add_executable (hin9 ${hin_exe} ${other_src})
target_link_libraries (hin9 hinsightd ${TARGET_LIBS})

add_executable (hin9_pid_helper external/tools/pid_helper.c)

install(PROGRAMS $<TARGET_FILE:hin9> DESTINATION sbin RENAME hinsightd)
install(PROGRAMS $<TARGET_FILE:hin9_pid_helper> DESTINATION bin RENAME hinsightd_pid_helper)
install(TARGETS hinsightd LIBRARY ARCHIVE INCLUDES)
install(DIRECTORY src/hin DESTINATION include)
install(FILES ${CMAKE_BINARY_DIR}/config_int.h DESTINATION include/hin/system)

