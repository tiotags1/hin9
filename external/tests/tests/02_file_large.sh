
set -e

export BENCH_CON=10
export BENCH_NUM=50

export URL_PATH=large.bin
export LOCAL_PATH=$DOCS_DIR/large.bin
export SUBTEST="normal ssl deflate gzip head hammer"

sh $TOOL_DIR/request.sh

