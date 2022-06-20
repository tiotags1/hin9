
set -e

export URL_PATH=tests/cache.php
export PHP_PATH=$DOCS_DIR/tests/cache.php
export SUBTEST="normal ssl head deflate gzip no_keepalive hammer"

sh $TOOL_DIR/request.sh

