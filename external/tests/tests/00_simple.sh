
set -e

export URL=http://$HOST:$PORT/
export LOCALFILE=index.html

sh $scripts_dir/request.sh

