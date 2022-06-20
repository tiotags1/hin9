
set -e

exit 1

export URL_PATH=proxy/tests/min.php
export PHP_PATH=$DOCS_DIR/tests/min.php
export SUBTEST="normal ssl head deflate gzip no_keepalive hammer post"

sh $TOOL_DIR/request.sh

