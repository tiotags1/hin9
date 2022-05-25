
set -e

URL=http://$HOST:$PORT/

RET="$(ab -k -c 1000 -n 10000 $URL)"

echo "$RET"

total=`echo "$RET" | grep "Requests per second"`

echo "Static file $total" >> $run_dir/bench.txt
