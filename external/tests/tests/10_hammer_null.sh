
set -e

URL=http://$HOST:$PORT/helloworld

export RET="$(ab -k -c $BENCH_CON -n $BENCH_NUM $URL)"

sh $scripts_dir/hammer.sh
