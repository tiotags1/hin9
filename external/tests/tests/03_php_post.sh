
set -e

export URL_PATH=tests/post.php
export PHP_PATH=$DOCS_DIR/tests/post.php
export SUBTEST="normal ssl head deflate gzip no_keepalive hammer post"

sh $TOOL_DIR/request.sh

