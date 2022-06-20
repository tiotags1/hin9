
set -e

export URL_PATH=tests/min.php
export PHP_PATH=$DOCS_DIR/tests/min.php
export SUBTEST="normal ssl head deflate gzip no_keepalive hammer"

sh $TOOL_DIR/request.sh

