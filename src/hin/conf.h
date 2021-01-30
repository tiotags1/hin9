
#ifndef HIN_CONF_H
#define HIN_CONF_H

#define HIN_CONF_PATH "workdir/main.lua"

#define HIN_URING_QUEUE_DEPTH 256
#define HIN_URING_DONT_FORK 0

#define HIN_HTTPD_TIMEOUT 10
#define HIN_HTTPD_MAX_HEADER_SIZE 65000
#define HIN_HTTPD_MAX_POST_SIZE 4121440
#define HIN_HTTPD_MAX_DEFLATE_SIZE 0
#define HIN_HTTPD_POST_DIRECTORY "/tmp"
#define HIN_HTTPD_SERVER_NAME "hinsightd 0.9"
#define HIN_HTTPD_PROXY_CONNECTION_REUSE 1

#define HIN_HTTPD_WORKER_NUM 1
#define HIN_HTTPD_WORKER_MAX_QUEUE 16
#define HIN_HTTPD_WORKER_PREFORKED 0
#define HIN_HTTPD_DISABLE_POWERED_BY 0

#define HIN_HTTPD_ASYNC_OPEN 1
#define HIN_HTTPD_ASYNC_STATX 0

#endif

