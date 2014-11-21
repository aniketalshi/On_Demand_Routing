
CC = gcc

#LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a
LIBS =  ./Stevens/unpv13e/libunp.a

FLAGS = -g 

#CFLAGS = -I/home/courses/cse533/Stevens/unpv13e/lib
CFLAGS = -I./Stevens/unpv13e/lib

all: odr_and client_and server_and
	
odr_and: odr_and.o utils.o odr_functions.o
	${CC} ${FLAGS} -o odr_and utils.o odr_and.o odr_functions.o ${LIBS}
odr_and.o: odr_and.c
	${CC} ${CFLAGS} -c odr_and.c

client_and: client_and.o utils.o
	${CC} ${FLAGS} -o client_and utils.o client_and.o ${LIBS}
client_and.o: client_and.c
	${CC} ${CFLAGS} -c client_and.c

server_and: server_and.o utils.o
	${CC} ${FLAGS} -o server_and utils.o server_and.o ${LIBS}
server_and.o: server_and.c
	${CC} ${CFLAGS} -c server_and.c

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${FLAGS} -c get_hw_addrs.c

prhwaddrs.o: prhwaddrs.c
	${CC} ${FLAGS} -c prhwaddrs.c

utils.o: utils.c
	${CC} ${CFLAGS} -c utils.c

odr_functions.o: odr_functions.c 
	${CC} ${CFLAGS} -c odr_functions.c

clean:
	rm utils.o odr_and odr_and.o odr_functions.o \
	client_and client_and.o server_and server_and.o

