CC=gcc
CFLAGS=-Wall -O

OBJS=	rb-download.o	\
	list.o		\
	findmheg.o	\
	listen.o	\
	command.o	\
	assoc.o		\
	carousel.o	\
	module.o	\
	table.o		\
	dsmcc.o		\
	biop.o		\
	fs.o		\
	utils.o

LIBS=-lz

TARDIR=`basename ${PWD}`
DATE=`date +%Y%m%d`

rb-download:	${OBJS}
	${CC} ${CFLAGS} -o rb-download ${OBJS} ${LIBS}

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	rm -f rb-download *.o core

tar:
	make clean
	(cd ..; tar zcvf ${TARDIR}-${DATE}.tar.gz ${TARDIR})


