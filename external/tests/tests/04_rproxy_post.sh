
set -e
#set -o xtrace

export URL_PATH=proxy/tests/post.php
export PHP_PATH=$DOCS_DIR/tests/post.php
export SUBTEST="normal ssl deflate gzip post"
# hammer doesn't make sense atm head no_keepalive hammer

sh $TOOL_DIR/request.sh

