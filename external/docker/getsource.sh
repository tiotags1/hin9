
cd ../..
tar --exclude='./workdir/ssl' --exclude='*.c1' -zcvf external/docker/src.tar.gz external/basic/ src/ docs/*.md docs/*.txt workdir/*.lua CMakeLists.txt


