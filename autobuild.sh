set -x
mkdir -p ../build
rm -rf ../build/*
cd ../build &&
	cmake ../chatserver-moduo-mysql-nginx-redis &&
	make
