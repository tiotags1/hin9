
set -e

TARGET="tests/min.php"

export URL="http://$HOST:$PORT/proxy/$TARGET"

A1=`curl -v -k "$URL"`
A2=`php $htdocs/$TARGET`

echo "received  '$A1'"
echo "should be '$A2'"

if [ "$A1" != "$A2" ]; then
  echo "Output doesn't match"
  exit 1
fi

