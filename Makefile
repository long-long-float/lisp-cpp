lisp: lisp.cpp
	@g++ -W -Wall -std=c++11 lisp.cpp -o lisp

clean:
	@rm -f *.o lisp
