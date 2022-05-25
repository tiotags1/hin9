
set -e

export URL=https://$HOST:$PORTS/
export LOCALFILE=index.html

sh $scripts_dir/request.sh

