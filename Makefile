.NOTPARALLEL:

all: build run

build:
	rm -f ./a.out
	${CXX}                                                  \
	-I/opt/libyang/include -I/opt/sysrepo/include           \
	-L/opt/libyang/lib -L/opt/sysrepo/lib                   \
	-Wl,-rpath=/opt/libyang/lib -Wl,-rpath=/opt/sysrepo/lib \
	-lyang -lyang-cpp -lsysrepo -lsysrepo-cpp               \
	-lev -lpcre -lpthread -lprotobuf-c                      \
	-g3 -ggdb -O0                                           \
	./main.cpp

run:
	sysrepoctl -u model || true
	sysrepoctl -i ./model.yang
	./a.out

.PHONY: all build run
