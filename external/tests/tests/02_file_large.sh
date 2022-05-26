
set -e

source $scripts_dir/files.sh

file_name=`basename $large_path`

export URL=http://$HOST:$PORT/$file_name
export LOCALFILE=$file_name

sh $scripts_dir/request.sh

