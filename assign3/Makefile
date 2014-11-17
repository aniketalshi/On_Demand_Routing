
CC = gcc

LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a

FLAGS = -g -O2

CFLAGS = -I/home/courses/cse533/Stevens/unpv13e/lib

all: odr_and client_and
	
#${CC} -o prhwaddrs prhwaddrs.o get_hw_addrs.o ${LIBS}


utils.o: utils.c
	${CC} ${CFLAGS} -c utils.c

odr_and: odr_and.o utils.o
	${CC} ${FLAGS} -o odr_and utils.o odr_and.o ${LIBS}
odr_and.o: odr_and.c
	${CC} ${CFLAGS} -c odr_and.c

client_and: client_and.o utils.o
	${CC} ${FLAGS} -o client_and utils.o client_and.o ${LIBS}
client_and.o: client_and.c
	${CC} ${CFLAGS} -c client_and.c

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${FLAGS} -c get_hw_addrs.c

prhwaddrs.o: prhwaddrs.c
	${CC} ${FLAGS} -c prhwaddrs.c


clean:
	rm odr_and odr_and.o client_and client_and.o

