#!/bin/sh

hinsightd

PIDS=default

while [ ! -z $PIDS ]; do
  echo "waiting for more '$PIDS'"
  PIDS=`ps -ef | grep -v grep | grep hinsightd | awk '{print $1}' | tr '\n' ' '`
  wait $PIDS
done

echo "done $PIDS"
