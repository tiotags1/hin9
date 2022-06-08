#!/bin/bash

# tests info in documents folder

cd "${0%/*}"
ROOT=`pwd`

cd ../..

export HOST=localhost
export PORT=8080
export PORTS=8081
export REMOTE=http://localhost:28081/
export BENCH_CON=1000
export BENCH_NUM=10000
export ROOT_DIR=`pwd`

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

complete=0
total=0

export run_dir=`pwd`/build/tests/
export scripts_dir=$ROOT/tools/

mkdir -p $run_dir $run_dir/config

export htdocs=`pwd`/htdocs/

work_dir=`pwd`/workdir/

cp $work_dir/main.lua $work_dir/lib.lua $run_dir
cp $work_dir/config/00_defines.lua $work_dir/config/10_localhost.lua $work_dir/config/99_default.lua $run_dir/config/
cat $work_dir/config/_20_*.lua > $run_dir/config/20_test_temp.lua

date >> $run_dir/bench.txt

build/hin9 --config ${run_dir}main.lua --log ${run_dir}server.log &
PID=$!

sleep 1
server_crashed=0

run_test () {
  export name=`basename $file`
  export name="${name%.*}"
  export test_dir=$run_dir/
  ((total++))
  echo "Test $name started on `date`" &> ${run_dir}$name.log
  sh $file &>> ${run_dir}$name.log
  exit_code=$?
  if ! kill -s 0 $PID &> ${run_dir}server.log; then
    exit_code=1
    echo "Server crashed" > ${run_dir}$name.log
    if [ $server_crashed -eq 0 ]; then
      printf "Server ${RED}crashed${NC} !!!\n"
      server_crashed=1
    fi
  fi
  if [ $exit_code -eq 0 ]; then
    printf "${GREEN}success$NC\t$name\n"
    ((complete++))
  else
    printf "${RED}failed$NC\t$name\n"
  fi
}

if [ -n "$1" ]; then
  for fn in "$@"
  do
    file=$ROOT/$fn
    run_test
  done
else
  for file in $ROOT/tests/*.sh; do
    run_test
  done
fi

kill -2 $PID &> ${run_dir}server.log
wait

echo "Successfully finished $complete/$total tests"

if [ $complete != $total ]; then
  exit $(($total-$complete))
fi

