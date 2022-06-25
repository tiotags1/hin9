
#ifndef HIN_INTERNAL_H
#define HIN_INTERNAL_H

int hin_server_start_accept (hin_server_t * server);

int hin_ssl_request_write (hin_buffer_t * buffer);
int hin_ssl_request_read (hin_buffer_t * buffer);
int hin_epoll_request_read (hin_buffer_t * buf);
int hin_epoll_request_write (hin_buffer_t * buf);

int hin_server_reaccept ();
int hin_epoll_check ();

#endif

