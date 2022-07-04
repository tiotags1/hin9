
set -e
#set -o xtrace

if [ -n "$LOCAL_PATH" ]; then
  md5_orig=`md5sum "$LOCAL_PATH" | awk '{ print $1 }'`
elif [ -n "$PHP_PATH" ]; then
  md5_orig=`php "$PHP_PATH" | md5sum | awk '{ print $1 }'`
elif [ -n "$LOCAL_DATA" ]; then
  md5_orig=`printf "$LOCAL_DATA" | md5sum | awk '{ print $1 }'`
else
  echo "no file specified"
fi

check_output () {
  md5_new=`md5sum "$out_file" | awk '{ print $1 }'`
  printf "$LOCAL_PATH\norig: $md5_orig\n"
  echo "$URL_PATH"
  echo "new:  $md5_new"

  if [ "$md5_orig" != "$md5_new" ]; then
    echo "$name $module doesn't match"
    exit 1
  fi
  echo "Completed test $name $module"
}

check_hammer () {
  export RET
  export module
  sh $TOOL_DIR/hammer.sh
}

export module="normal"
if echo "$SUBTEST" | grep "$module" > /dev/null; then
  echo "Start test $name $module"
  out_file=$DOWNLOAD_DIR/${name}_${module}.bin
  curl $CURL_FLAGS -o "$out_file" "http://$HOST:$PORT/$URL_PATH"
  check_output
fi

export module="ssl"
if echo "$SUBTEST" | grep "$module" > /dev/null; then
  echo "Start test $name $module"
  out_file=$DOWNLOAD_DIR/${name}_${module}.bin
  curl $CURL_FLAGS -o "$out_file" "https://$HOST:$PORTS/$URL_PATH"
  check_output
fi

export module="deflate"
if echo "$SUBTEST" | grep "$module" > /dev/null; then
  echo "Start test $name $module"
  out_gz=$DOWNLOAD_DIR/${name}_${module}.gz
  out_file=$DOWNLOAD_DIR/${name}_${module}.bin
  RET="$(curl $CURL_FLAGS -o "$out_gz" --header "Accept-Encoding: deflate" "http://$HOST:$PORT/$URL_PATH")"
  set +e
  # TODO
  if echo "$RET" | grep "deflate" > /dev/null; then
    cat $out_gz | zlib-flate -uncompress > $out_file
    set -e
    check_output
  else
    set -e
  fi
fi

export module="post"
if echo "$SUBTEST" | grep "$module" > /dev/null; then
  echo "Start test $name $module"
  POSTVALUE="helloworldhowareyou"
  out_file=$DOWNLOAD_DIR/${name}_${module}.bin
  curl $CURL_FLAGS --form VALUE="$POSTVALUE" -o "$out_file" "http://$HOST:$PORT/$URL_PATH"
  A1=`grep $POSTVALUE $out_file`
  A2="  <tr><td>VALUE</td><td>$POSTVALUE</td></tr>"
  if [ "$A1" != "$A2" ]; then
    echo "Output doesn't match"
    exit 1
  fi
  echo "Completed test $name $module"
fi

export module="hammer"
if echo "$SUBTEST" | grep "$module" > /dev/null; then
  echo "Start test $name $module"
  RET="$(ab -k -c $BENCH_CON -n $BENCH_NUM "http://$HOST:$PORT/$URL_PATH")"
  check_hammer
  echo "Completed test $name $module"
fi

export module="no_keepalive"
if echo "$SUBTEST" | grep "$module" > /dev/null; then
  echo "Start test $name $module"
  RET="$(ab -c $BENCH_CON -n $BENCH_NUM "http://$HOST:$PORT/$URL_PATH")"
  check_hammer
  echo "Completed test $name $module"
fi

export module="head"
if echo "$SUBTEST" | grep "$module" > /dev/null; then
  echo "Start test $name $module"
  RET="$(ab -k -i -c $BENCH_CON -n $BENCH_NUM "http://$HOST:$PORT/$URL_PATH")"
  if [[ ! $( echo "$RET" | grep "HTML transferred:       0 bytes" ) ]]; then
    echo "Head requests sent bytes"
    exit 1
  fi
  check_hammer
  echo "Completed test $name $module"
fi


