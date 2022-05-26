
set -e

echo "$RET"

total=`echo "$RET" | grep "Requests per second"`

set +e
non200=`echo "$RET" | grep "Non-2xx responses"`
if [ -n "$non200" ]; then
  echo "didn't get an ok $non200"
  exit 1
fi

echo "$name $total" >> $run_dir/bench.txt
