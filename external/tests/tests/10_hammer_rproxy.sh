
set -e

URL=http://$HOST:$PORT/proxy/

# broken inside test, works outside ?

export RET="$(ab -k -c $BENCH_CON -n $BENCH_NUM $URL)"

sh $scripts_dir/hammer.sh
