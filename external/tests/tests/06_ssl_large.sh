
set -e

source $scripts_dir/files.sh

file_name=`basename $large_path`

export URL=https://$HOST:$PORTS/$file_name
export LOCALFILE=$file_name

sh $scripts_dir/request.sh
