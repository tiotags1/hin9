
set -e

TARGET="tests/min.php"

A1=`curl -k --form VALUE="$VALUE" http://$HOST:$PORT/$TARGET`
A2=`php $htdocs/$TARGET`

echo "received  '$A1'"
echo "should be '$A2'"

if [ "$A1" != "$A2" ]; then
  echo "Output doesn't match"
  exit 1
fi
