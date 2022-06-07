
BENCH_CON=8
BENCH_NUM=10

set -e

source $scripts_dir/files.sh

URL=http://$HOST:$PORT/tests/post.php

upload_file=$test_dir/upload.bin
border=1234567890

printf "\-\-$border\r\n\
Content-Disposition: form-data; name=\"upload\"; filename=\"test.bin\"\r\n\
Content-type: application/octet-stream\r\n\
Content-Transfer-Encoding: base64\r\n\r\n" > $upload_file

base64 $tmp_path >> $upload_file

printf "\r\n--$border--" >> $upload_file

# warning http/1.0 and keepalive when targeting php chunked doesn't make sense

export RET="$(ab -p $upload_file -T "multipart/form-data; boundary=$border" -c $BENCH_CON -n $BENCH_NUM $URL)"

sh $scripts_dir/hammer.sh
