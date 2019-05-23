.NOTPARALLEL:

all: build run

build:
	rm -f ./a.out
	g++ \
	-I/opt/sysrepo/include/ \
	-L/opt/libredblack/lib -L/opt/libyang/lib -L/opt/sysrepo/lib \
	-Wl,-rpath=/opt/libredblack/lib -Wl,-rpath=/opt/libyang/lib -Wl,-rpath=/opt/sysrepo/lib \
	-lsysrepo -lSysrepo-cpp -lyang -pthread -lpcre -lev -lprotobuf-c \
	-g3 -ggdb -O0 \
	main.cpp

run:
	sysrepoctl -u -m model || true
	sysrepoctl -i -g ./model.yang -o root:root -p 666
	./a.out

.PHONY: all build run
