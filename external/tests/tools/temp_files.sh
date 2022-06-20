
set -e

tmp_path="$RUN_DIR/temp.bin"
medium_path="$DOCS_DIR/medium.bin"
large_path="$DOCS_DIR/large.bin"

if [ ! -f "$tmp_path" ]; then
  dd if=/dev/urandom of=$tmp_path bs=1M count=5
fi

if [ ! -f "$medium_path" ]; then
  for i in {1..3}
  do
    cat $tmp_path >> $medium_path
  done
fi

if [ ! -f "$large_path" ]; then
  for i in {1..100}
  do
    cat $tmp_path >> $large_path
  done
fi


