
set -e

VALUE=helloworldhowareyou

A1=`curl -k --form VALUE="$VALUE" http://$HOST:$PORT/tests/post.php | grep VALUE`
A2="  <tr><td>VALUE</td><td>$VALUE</td></tr>"

echo "received  '$A1'"
echo "should be '$A2'"

if [ "$A1" != "$A2" ]; then
  echo "Output doesn't match"
  exit 1
fi

