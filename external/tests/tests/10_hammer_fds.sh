
URL=http://$HOST:$PORT/

LOG_PATH=${LOGS_DIR}${name}_abs.log

rm -f "$LOG_PATH"

set -e

ab -k -c $BENCH_CON -n $BENCH_NUM $URL &>> $LOG_PATH &
ab -k -c $BENCH_CON -n $BENCH_NUM $URL &>> $LOG_PATH &
ab -k -c $BENCH_CON -n $BENCH_NUM $URL &>> $LOG_PATH &
ab -k -c $BENCH_CON -n $BENCH_NUM $URL &>> $LOG_PATH &

wait

RET="$(cat $LOG_PATH)"

export NO_BENCH=1

sh $TOOL_DIR/hammer.sh

