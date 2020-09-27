all: feeder

main.o: feeder.c
	gcc -c feeder.c

clean:
	rm -f *.o main
