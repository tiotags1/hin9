#!/sbin/openrc-run

NAME=$RC_SVCNAME
RUN_DIR=/var/run/$NAME
LOG_DIR=/var/log/$NAME
CWD_DIR=/var/www/localhost
PID_FILE=$RUN_DIR/$NAME.pid
CFG_FILE=/etc/hinsightd/$NAME.lua

RUN_FILE=/usr/bin/hinsightd
RUN_USER="root"

#extra_commands="checkconfig"

command=$RUN_FILE
command_args="--config $CFG_FILE --logdir $LOG_DIR --cwd $CWD_DIR"
pidfile="$PID_FILE"
command_args_background="--daemonize"

depend() {
  need net
}

start_pre() {
  checkpath --directory --owner $RUN_USER:$RUN_USER --mode 0775 $RUN_DIR $LOG_DIR
}

#checkconfig() {
#}


