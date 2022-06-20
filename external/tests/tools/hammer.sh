
set -e

echo "$RET"

total=`echo "$RET" | grep "Requests per second"`

set +e
non200=`echo "$RET" | grep "Non-2xx responses"`
if [ -n "$non200" ]; then
  echo "got non 200 requests $non200"
  exit 1
fi
failed=`echo "$RET" | grep "Exceptions:"`
if [ -n "$failed" ]; then
  echo "got faild requests $failed"
  exit 1
fi

echo "${name} $total" >> $LOGS_DIR/bench.txt
