
set -e

export URL="http://$HOST:$PORT/"
export LOCAL_PATH="index.html"

out_file=$DOWNLOAD_DIR/$name.bin

$ROOT_DIR/build/hin9 -V -do $URL $out_file
md51=`md5sum $out_file | awk '{ print $1 }'`
md52=`md5sum $DOCS_DIR/$LOCAL_PATH | awk '{ print $1 }'`

printf "$out_file: $md51\n$htdocs/$LOCAL_PATH: $md52\n"

if [ "$md51" != "$md52" ]; then
  echo "Output doesn't match"
  exit 1
fi

echo "Completed test $name"
