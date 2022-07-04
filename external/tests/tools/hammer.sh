
set -e

echo "$RET"

set +e
non200=`echo "$RET" | grep "Non-2xx responses"`
if [ -n "$non200" ]; then
  echo "$name $module got non 200 requests $non200"
  exit 1
fi
failed=`echo "$RET" | grep "Exceptions:"`
if [ -n "$failed" ]; then
  echo "$name $module got faild requests $failed"
  exit 1
fi
failed=`echo "$RET" | grep "apr_pollset_poll"`
if [ -n "$failed" ]; then
  echo "$name $module got timedout requests $failed"
  exit 1
fi
set -e

total=`echo "$RET" | grep "Requests per second"`
if [ -z "$NO_BENCH" ]; then
  echo "${name} ${module} $total" >> $LOGS_DIR/bench.txt
fi
