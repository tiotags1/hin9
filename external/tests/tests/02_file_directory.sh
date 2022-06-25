
set -e

export URL_PATH=tests
export LOCAL_PATH=$DOCS_DIR/tests/index.html
export SUBTEST="normal ssl deflate gzip"
export CURL_FLAGS="$CURL_FLAGS -L"

sh $TOOL_DIR/request.sh

export URL_PATH=tests/index.html
export LOCAL_PATH=$DOCS_DIR/tests/index.html
export SUBTEST="head hammer no_keepalive"

sh $TOOL_DIR/request.sh

