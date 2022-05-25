
set -e

URL=http://$HOST:$PORT/proxy/

# broken inside test, works outside ?

RET="$(ab -k -c 1000 -n 1000 $URL)"

echo "$RET"

total=`echo "$RET" | grep "Requests per second"`

echo "Reverse proxy $total" >> $run_dir/bench.txt
