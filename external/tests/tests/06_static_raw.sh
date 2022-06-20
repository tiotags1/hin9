
set -e

do_req () {
  RES=$(echo "$REQ" | nc $HOST $PORT)
  echo "$RES"
  echo "$RES" | grep "HTTP/1.1 200 OK"
}

REQ=$(printf "GET / HTTP/1.1\r\n\
Host: $HOST:$PORT\r\n\
Connection: close\r\n\
\r\n")
do_req
echo "GET request OK"

#REQ=$(printf "GET / HTTP/1.1\r\n\
#Connection: close\r\n\
#\r\n")
#do_req
#echo "GET HTTP/1.1 no host ok"

#echo "GET deflate request"
#echo "GET gzip request"
#echo "GET keep-alive request"
#echo "HEAD request"
#echo "GET 304 request"
