
cd ../..

tar --exclude='./workdir/ssl' --exclude='*.c1' -zcvf external/docker/conf.tar.gz workdir/*.lua htdocs/index.html htdocs/tests/*

tar --exclude='./workdir/ssl' --exclude='*.c1' -zcvf external/docker/src.tar.gz docs/*.md docs/*.txt external/basic/ src/ CMakeLists.txt

