
set -e

HOST=localhost:8080
HOSTS=localhost:8081

DOCS=tests/
HTDOCS=htdocs/$PATH

PROXY=

DIR=/tmp/

build/hin9 &

PID=$!
echo "Pid is $PID"

sleep 1

echo "GET request"
printf "GET /${DOCS}min.php HTTP/1.1\r\n\
Host: localhost:8080\r\n\
Connection: close\r\n\
\r\n" > ${DIR}cmd.txt
cat ${DIR}cmd.txt | nc localhost 8080
echo ""

echo "GET deflate request"
printf "GET /${DOCS}min.php HTTP/1.1\r\n\
Host: localhost:8080\r\n\
Accept-Encoding: deflate\r\n\
Connection: close\r\n\
\r\n" > ${DIR}cmd.txt
cat ${DIR}cmd.txt | nc localhost 8080
echo ""

echo "GET gzip request"
printf "GET /${DOCS}min.php HTTP/1.1\r\n\
Host: localhost:8080\r\n\
Accept-Encoding: gzip\r\n\
Connection: close\r\n\
\r\n" > ${DIR}cmd.txt
cat ${DIR}cmd.txt | nc localhost 8080
echo ""

echo "Keep-alive request"
printf "GET /${DOCS}min.php HTTP/1.1\r\n\
Host: localhost:8080\r\n\
\r\n\
GET /${DOCS}min.php HTTP/1.1\r\n\
Host: localhost:8080\r\n\
\r\n\
GET /${DOCS}min.php HTTP/1.1\r\n\
Host: localhost:8080\r\n\
Connection: close\r\n\
\r\n" > ${DIR}cmd.txt
cat ${DIR}cmd.txt | nc localhost 8080
echo ""

echo "POST 0-length request"
printf "POST /${DOCS}post.php HTTP/1.1\r\n\
Host: localhost:8080\r\n\
Content-Length: 0\r\n\
Connection: close\r\n\
\r\n" > ${DIR}cmd.txt
cat ${DIR}cmd.txt | nc localhost 8080
echo ""

echo "POST url encoded request"
printf "POST /${DOCS}post.php HTTP/1.1\r\n\
Host: localhost:8080\r\n\
Content-Type: application/x-www-form-urlencoded\r\n\
Content-Length: 27\r\n\
Connection: close\r\n\
\r\n\
field1=value1&field2=value2" > ${DIR}cmd.txt
cat ${DIR}cmd.txt | nc localhost 8080
echo ""

echo "Finished"

# fails: request without a hostname, chunked upload, post on raw, post on proxy
#


kill $PID
