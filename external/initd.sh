#!/sbin/openrc-run

NAME=$RC_SVCNAME
RUN_DIR=/var/run/
LOG_DIR=/var/log/$NAME
CWD_DIR=/var/www/localhost
PID_FILE=$RUN_DIR/$NAME.pid
CFG_FILE=/etc/hinsightd/$NAME.lua

RUN_FILE=/usr/bin/hinsightd
RUN_USER="root"

extra_commands="checkconfig reload"

command=$RUN_FILE
command_args="--config $CFG_FILE --logdir $LOG_DIR --cwd $CWD_DIR --pidfile $PID_FILE"
pidfile="$PID_FILE"
command_args_background="--daemonize"

depend() {
  use net
}

checkconfig() {
  $command $command_args --pretend > /dev/null
}

start_pre() {
  checkpath --directory --owner $RUN_USER:$RUN_USER --mode 0770 $LOG_DIR
  checkconfig || return 1
}

reload() {
  if ! service_started "${SVCNAME}" ; then
    eerror " * ERROR ${SVCNAME} isn't running"
    return 1
  fi

  checkconfig || return 1

  echo " * Reloading ${SVCNAME} ..."

  start-stop-daemon --quiet --signal USR1 --pidfile ${PID_FILE}
  eend $?
}


