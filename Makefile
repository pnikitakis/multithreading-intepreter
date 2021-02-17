all:
	gcc -g -Wall -o khash.o -c khash.c
	gcc -g -Wall -o hw4b hw4b.c khash.o -lpthread

clean:
	$(RM) hw4b
	$(RM) khash.o

