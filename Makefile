.PHONY=all run

CXX=g++
CFLAGS=-Wall
LDLIBS=-lpthread
SRCS_LINUX := $(wildcard server/*.cpp)
OBJ := $(SRCS_LINUX:%.cpp=%.o)
all: server

server: mostlyclean $(OBJ) 
	$(CXX) -o server/m.out $(LDFLAGS) $(OBJ) $(LDLIBS)

$(OBJ): %.o : %.cpp
	$(CXX) $(CFLAGS) $(IFLAGS) $(LDLIBS) -c $< -o $@

run: server
	cd `pwd`/server; ./m.out; cd ..

mostlyclean:
	$(rm server/*.out || true)
	
clean: mostlyclean
	rm server/*.o
