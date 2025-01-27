all: ergasia3
ergasia3: ergasia3.o
	gcc -o ergasia3 ergasia3.o

ergasia3.o: ergasia3.c dhlwseis.h
	gcc -c ergasia3.c -o ergasia3.o

clean:
	rm -f *.o ergasia3
