
# it fails even with a lower number so doesn't matter
BENCH_CON=4
BENCH_NUM=100

exit 1

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

export RET="$(ab -p $upload_file -T "multipart/form-data; boundary=$border" -k -c $BENCH_CON -n $BENCH_NUM $URL)"

sh $scripts_dir/hammer.sh
