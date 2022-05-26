#!/bin/bash

# requires curl, apache bench, netcat, coreutils

cd "${0%/*}"
ROOT=`pwd`

cd ../..

export HOST=localhost
export PORT=8080
export PORTS=8081
export REMOTE=http://localhost:28081/
export BENCH_CON=1000
export BENCH_NUM=10000

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

complete=0
total=0

export run_dir=`pwd`/build/tests/
export scripts_dir=$ROOT/tools/

mkdir -p $run_dir

export htdocs=`pwd`/htdocs/

work_dir=`pwd`/workdir/

cat $work_dir/config/_20_fcgi.lua $work_dir/config/_20_proxy.lua > $work_dir/config/20_test_temp.lua

#rm -f $run_dir/bench.txt
date >> $run_dir/bench.txt

build/hin9 > ${run_dir}server.log &
PID=$!

sleep 1

run_test () {
  export name=`basename $file`
  export name="${name%.*}"
  export test_dir=$run_dir/
  ((total++))
  if sh $file &> ${run_dir}$name.log; then
    printf "${GREEN}success$NC\t$name\n"
    ((complete++))
  else
    printf "${RED}failed$NC\t$name\n"
  fi
}

if [ -n "$1" ]; then
  file=$ROOT/$1
  run_test
else
  for file in $ROOT/tests/*.sh; do
    run_test
  done
fi

rm $work_dir/config/20_test_temp.lua

kill -2 $PID
wait

echo "Successfully finished $complete/$total tests"

if [ $complete != $total ]; then
  exit 1
fi

