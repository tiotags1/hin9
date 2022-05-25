#!/bin/bash

cd "${0%/*}"
ROOT=`pwd`

cd ../..

export HOST=localhost
export PORT=8080
export PORTS=8081
export REMOTE=http://localhost:28081/

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

complete=0
total=0

export run_dir=`pwd`/build/tests/
export scripts_dir=$ROOT

mkdir -p $run_dir

export htdocs=`pwd`/htdocs/

work_dir=`pwd`/workdir/

cat $work_dir/config/_20_fcgi.lua $work_dir/config/_20_proxy.lua > $work_dir/config/20_test_temp.lua

rm -f $run_dir/bench.txt

build/hin9 > ${run_dir}server.log &
PID=$!

sleep 1

for file in $ROOT/tests/*.sh; do
  export name=`basename $file`
  export name="${name%.*}"
  export test_dir=$run_dir/
  ((total++))
  if sh $file > ${run_dir}$name.log 2>&1; then
    printf "$name\t ${GREEN}successful$NC\n"
    ((complete++))
  else
    printf "$name\t ${RED}failed$NC\n"
  fi
done

rm $work_dir/config/20_test_temp.lua

kill $PID
wait

echo "Successfully finished $complete/$total tests"

if [ $complete != $total ]; then
  exit 1
fi

