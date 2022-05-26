
set -e

source $scripts_dir/files.sh

dd if=/dev/urandom of=$tmp_path bs=1M count=1

rm -f $medium_path
for i in {1..15}
do
  cat $tmp_path >> $medium_path
done

rm -f $large_path
for i in {1..100}
do
  cat $tmp_path >> $large_path
done
