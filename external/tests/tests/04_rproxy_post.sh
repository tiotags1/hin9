
set -e
set -o xtrace

exit 1

export URL_PATH=proxy/tests/post.php
export PHP_PATH=$DOCS_DIR/tests/post.php
export SUBTEST="normal ssl head deflate gzip no_keepalive hammer post"

sh $TOOL_DIR/request.sh

