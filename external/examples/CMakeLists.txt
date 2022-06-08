cmake_minimum_required(VERSION 3.10)

# set the project name
project (hin9_examples)

add_definitions (-D_GNU_SOURCE -D_LARGEFILE_SOURCE -Wall -Werror=vla)

include_directories ("../basic/" "../../src/hin")

set (TARGET_LIBS ${TARGET_LIBS} uring z crypto ssl)

set (null_httpd_src
null_httpd/client.c
null_httpd/httpd.c
null_httpd/main.c
null_httpd/read.c
null_httpd/utils.c
)

add_executable (null_httpd ${null_httpd_src})
target_link_libraries (null_httpd hinsightd ${TARGET_LIBS})


