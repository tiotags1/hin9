
set -e

# urlencode from https://gist.github.com/cdown/1163649
function urlencode() {
if [[ $# != 1 ]]; then
echo "Usage: $0 string-to-urlencode"
return 1
fi
local data="$(curl -s -o /dev/null -w %{url_effective} --get --data-urlencode "$1" "")"
if [[ $? == 0 ]]; then
echo "${data##/?}"
fi
return 0
}

file_name="世界さんこんにちは.html"
temp=`urlencode "$file_name"`
echo "$temp"

export URL="http://$HOST:$PORT/tests/$temp"
export LOCALFILE="tests/$file_name"

sh $scripts_dir/request.sh

