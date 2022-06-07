
set -e

URL=http://$HOST:$PORT/

export RET="$(ab -k -i -c $BENCH_CON -n $BENCH_NUM $URL)"

ret=`sh $scripts_dir/hammer.sh`

if [[ ! $( echo "$ret" | grep "HTML transferred:       0 bytes" ) ]]; then
  echo "Head requests sent bytes"
  exit 1
fi

echo "$ret"
