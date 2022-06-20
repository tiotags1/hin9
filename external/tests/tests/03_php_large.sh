
set -e

export URL_PATH=tests/large.php
export PHP_PATH=$DOCS_DIR/tests/large.php
export SUBTEST="normal ssl deflate gzip head no_keepalive hammer"

sh $TOOL_DIR/request.sh

