
set -e

export URL=http://$HOST:$PORT/proxy/
export LOCALFILE=index.html

sh $scripts_dir/request.sh

