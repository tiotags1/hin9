
set -e

export URL_PATH=large.bin
export LOCAL_PATH=$DOCS_DIR/large.bin
export SUBTEST="normal ssl deflate gzip head"

sh $TOOL_DIR/request.sh

