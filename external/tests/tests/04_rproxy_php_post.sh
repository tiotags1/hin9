
set -e

VALUE=helloworldhowareyou

URL="http://$HOST:$PORT/proxy/tests/post.php"

A1=`curl -v -k --form VALUE="$VALUE" "$URL" | grep VALUE`
A2="  <tr><td>VALUE</td><td>$VALUE</td></tr>"

echo "received  '$A1'"
echo "should be '$A2'"

if [ "$A1" != "$A2" ]; then
  echo "Output doesn't match"
  exit 1
fi

