
requirements
------------

requires bash-like shell, coreutils, curl, apache bench, netcat

for php: requires a php-fpm listening on tcp port 9000

for rproxy: requires another http server listening on port 28081 with the same htdocs as this one and php enabled

to run all tests `sh run.sh` inside the test folder `external/tests`

to run a single test `sh run.sh tests/<name>.sh`

benchmarks results are saved in `build/test/logs/bench.txt`


