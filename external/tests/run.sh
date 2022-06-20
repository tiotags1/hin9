#!/bin/bash

# tests info in documents folder

CWD_DIR=`pwd`
SCRIPT_DIR="$( realpath ` dirname -- "$0" ` )"

cd ../..
export ROOT_DIR=`pwd`

export HOST=localhost
export PORT=8080
export PORTS=8081
export REMOTE=http://localhost:28081/
export BENCH_CON=1000
export BENCH_NUM=10000

export RUN_DIR=${ROOT_DIR}/build/tests/
export TOOL_DIR=${SCRIPT_DIR}/tools/
export DOCS_DIR=${ROOT_DIR}/htdocs/
export WORK_DIR=${ROOT_DIR}/workdir/
export LOGS_DIR=${RUN_DIR}logs/
export DOWNLOAD_DIR=${RUN_DIR}downloads/
export CURL_FLAGS="-v -k --fail"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

complete=0
total=0
server_crashed=0

mkdir -p $RUN_DIR/{config,downloads,logs} ${WORK_DIR}/ssl

cp $WORK_DIR/{main.lua,lib.lua} $RUN_DIR
cp $WORK_DIR/config/{00_defines.lua,10_localhost.lua,99_default.lua} $RUN_DIR/config/
cat $WORK_DIR/config/_20_*.lua > $RUN_DIR/config/20_test_temp.lua

if [ ! -f ${WORK_DIR}/ssl/key.pem ]; then
  openssl req -x509 -newkey rsa:2048 -keyout ${WORK_DIR}/ssl/key.pem -out ${WORK_DIR}/ssl/cert.pem -sha256 -days 365 -nodes -subj "/C=US/ST=Oregon/L=Portland/O=Company Name/OU=Org/CN=example.com"
fi

$TOOL_DIR/temp_files.sh &>> ${LOGS_DIR}server.log

build/hin9 --config ${RUN_DIR}main.lua --log ${LOGS_DIR}server.log --logdir ${LOGS_DIR} &
PID=$!

sleep 1

echo "testing $HOST:$PORT REMOTE $REMOTE with -c $BENCH_CON -n $BENCH_NUM on `date`" >> $LOGS_DIR/bench.txt

run_test () {
  export name=`basename $file`
  export name="${name%.*}"
  export test_dir=$RUN_DIR/
  ((total++))
  echo "Test $name started on `date`" &> ${LOGS_DIR}/$name.log
  sh $file &>> ${LOGS_DIR}$name.log
  exit_code=$?
  if ! kill -s 0 $PID &>> ${LOGS_DIR}/server.log; then
    exit_code=1
    echo "Server crashed" > ${LOGS_DIR}/$name.log
    if [ $server_crashed -eq 0 ]; then
      printf "Server ${RED}crashed${NC} !!!\n"
      server_crashed=1
    fi
  fi
  if [ $exit_code -eq 0 ]; then
    printf "${GREEN}success$NC\t$name\n"
    ((complete++))
  else
    printf "${RED}failed$NC\t$name (more info 'tail ${LOGS_DIR}$name.log')\n"
  fi
}

if [ -n "$1" ]; then
  for fn in "$@"
  do
    file=$CWD_DIR/$fn
    run_test
  done
else
  for file in $SCRIPT_DIR/tests/*.sh; do
    run_test
  done
fi

kill -2 $PID &>> ${LOGS_DIR}/server.log

echo "Successfully finished $complete/$total tests"

if [ $complete != $total ]; then
  exit $(($total-$complete))
fi

wait
