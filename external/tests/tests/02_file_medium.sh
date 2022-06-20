
set -e

export URL_PATH=medium.bin
export LOCAL_PATH=$DOCS_DIR/medium.bin
export SUBTEST="normal ssl deflate gzip head"

sh $TOOL_DIR/request.sh

