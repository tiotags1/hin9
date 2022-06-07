
set -e
#set -o xtrace

out_file=$test_dir/$name.bin

curl -v -k $CURL_FLAGS -o "$out_file" "$URL"
md51=`md5sum "$out_file" | awk '{ print $1 }'`
md52=`md5sum "$htdocs/$LOCALFILE" | awk '{ print $1 }'`

printf "$out_file: $md51\n$htdocs/$LOCALFILE: $md52\n"

if [ "$md51" != "$md52" ]; then
  echo "Output doesn't match"
  exit 1
fi
