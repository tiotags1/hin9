
set -e

export URL=http://$HOST:$PORT/
export LOCALFILE=index.html

out_file_gz=$test_dir/$name.gz
out_file=$test_dir/$name.bin

curl -v -k $CURL_FLAGS --header "Accept-Encoding: deflate" -o "$out_file_gz" -k "$URL"
cat $out_file_gz | zlib-flate -uncompress > $out_file

md51=`md5sum "$out_file" | awk '{ print $1 }'`
md52=`md5sum "$htdocs/$LOCALFILE" | awk '{ print $1 }'`

printf "$out_file: $md51\n$htdocs/$LOCALFILE: $md52\n"

if [ "$md51" != "$md52" ]; then
  echo "Output doesn't match"
  exit 1
fi
