
set -e

file_name=large_test.bin
file_path=$htdocs/$file_name
tmp_path=$test_dir/temp.bin

export URL=http://$HOST:$PORT/$file_name
export LOCALFILE=$file_name

dd if=/dev/urandom of=$tmp_path bs=1M count=2

rm -f $file_path

for i in {1..100}
do
  cat $tmp_path >> $file_path
done

sleep 2

sh $scripts_dir/request.sh

rm $tmp_path
rm $file_path
