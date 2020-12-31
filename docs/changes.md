

* graceful restart initial implementation
  * after 1-2 restarts liburing crashes in io_uring setup syscall claming no resources
  * doesn't wait for new server to finish setup before it issues close
* added ability to request an ipv6 or ipv4 socket
* flush httpd timeout so it closes when you close the server
