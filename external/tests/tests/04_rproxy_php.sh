
set -e
#set -o xtrace

export URL_PATH=proxy/tests/min.php
export PHP_PATH=$DOCS_DIR/tests/min.php
export SUBTEST="normal ssl deflate gzip"
# hammer doesn't make sense atm head no_keepalive hammer

sh $TOOL_DIR/request.sh

