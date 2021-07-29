#!/sbin/openrc-run

HIN_NAME=${HIN_NAME:-$RC_SVCNAME}
HIN_RUN_DIR=${HIN_RUN_DIR:-/run/$HIN_NAME}
HIN_LOG_DIR=${HIN_LOG_DIR:-/var/log/$HIN_NAME}
HIN_TMP_DIR=${HIN_TMP_DIR:-/var/tmp/$HIN_NAME}
HIN_WORK_DIR=${HIN_WORK_DIR:-/var/www/localhost}
HIN_CFG_FILE=${HIN_CFG_FILE:-/etc/hinsightd/main.lua}
HIN_LOG_FILE=${HIN_LOG_FILE:-$HIN_LOG_DIR/hindsight.log}

extra_commands="checkconfig reload"

command=${HIN_BIN:-/usr/sbin/hinsightd}
pidfile="${HIN_PID_FILE:-$HIN_RUN_DIR/$HIN_NAME.pid}"
command_args="--log $HIN_LOG_FILE --config $HIN_CFG_FILE --logdir $HIN_LOG_DIR --tmpdir $HIN_TMP_DIR --cwd $HIN_WORK_DIR --pidfile $pidfile.1"
command_args_background="--daemonize"
command_user=${HIN_USER:-hinsightd}

depend() {
  use net
  after firewall
}

checkconfig() {
  start-stop-daemon --quiet --user $command_user --start --exec $command -- --check $command_args
}

start_pre() {
  checkpath --directory --owner $command_user:$command_user --mode 0770 $HIN_LOG_DIR $HIN_TMP_DIR $HIN_RUN_DIR
  checkconfig || return 1
  hinsightd_pid_helper $pidfile.1 $pidfile || return 1
}

reload() {
  if ! service_started "${HIN_NAME}" ; then
    eerror "ERROR ${HIN_NAME} isn't running"
    return 1
  fi

  hinsightd_pid_helper $pidfile.1 $pidfile || return 1

  checkconfig || return 1

  einfo "Reloading ${HIN_NAME} ..."

  start-stop-daemon --quiet --signal USR1 --pidfile $pidfile
  eend $?
}


