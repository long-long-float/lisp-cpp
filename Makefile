CC = g++
CPPFLAGS = -W -Wall -std=c++11
LOADLIBS = -ldl

lisp: lisp.o object.o environment.o gc.o

clean:
	@rm -f *.o lisp
