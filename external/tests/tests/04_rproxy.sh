
set -e

export URL_PATH=proxy/
export LOCAL_PATH=$DOCS_DIR/index.html
export SUBTEST="normal ssl head deflate gzip no_keepalive hammer"

sh $TOOL_DIR/request.sh

