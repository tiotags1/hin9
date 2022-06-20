
set -e

export URL_PATH=helloworld
export LOCAL_DATA="Hello world\n"
export SUBTEST="normal ssl head no_keepalive hammer"

sh $TOOL_DIR/request.sh

