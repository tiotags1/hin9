
set -e

export URL=http://$HOST:$PORT/tests
export LOCALFILE=tests/index.html
export CURL_FLAGS="-L"

sh $scripts_dir/request.sh

