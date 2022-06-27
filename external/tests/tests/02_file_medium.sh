
set -e

export BENCH_CON=10
export BENCH_NUM=50

export URL_PATH=medium.bin
export LOCAL_PATH=$DOCS_DIR/medium.bin
export SUBTEST="normal ssl deflate gzip head hammer"

sh $TOOL_DIR/request.sh

