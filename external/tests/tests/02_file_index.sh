
set -e

export URL_PATH=index.html
export LOCAL_PATH=$DOCS_DIR/index.html
#export PHP_FILE=
#export LOCAL_DATA=
#export SUBTEST="normal ssl deflate gzip post head no_keepalive hammer"
export SUBTEST="normal ssl deflate gzip head no_keepalive hammer"

sh $TOOL_DIR/request.sh

