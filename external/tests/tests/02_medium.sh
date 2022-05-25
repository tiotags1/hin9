
set -e
set -o xtrace

file_name=medium_test.bin
file_path=$htdocs/$file_name

export URL=http://$HOST:$PORT/$file_name
export LOCALFILE=$file_name

dd if=/dev/urandom of=$file_path bs=1M count=5

sleep 2

sh $scripts_dir/request.sh

rm $file_path
