
set -e

export TARGET="tests/min.php"
export URL="http://$HOST:$PORT/$TARGET"

out_file_gz=$test_dir/$name.gz
out_file=$test_dir/$name.bin

curl -v -k $CURL_FLAGS --header "Accept-Encoding: deflate" -k "$URL" > $out_file_gz
A2=`php $htdocs/$TARGET`

A1=`cat $out_file_gz | zlib-flate -uncompress`

echo "received  '$A1'"
echo "should be '$A2'"

if [ "$A1" != "$A2" ]; then
  echo "Output doesn't match"
  exit 1
fi
