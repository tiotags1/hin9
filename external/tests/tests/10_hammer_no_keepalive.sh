
set -e

URL=http://$HOST:$PORT/

export RET="$(ab -c $BENCH_CON -n $BENCH_NUM $URL)"

sh $scripts_dir/hammer.sh
