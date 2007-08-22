CC=gcc
CFLAGS=-Wall -O2

LEX=lex
YACC=yacc

DESTDIR=/usr/local

OBJS=	mhegc.o	\
	token.o	\
	utils.o

TARDIR=`basename ${PWD}`

ccc:	ccc.y ccc.l
	${LEX} -i ccc.l
	${YACC} -d ccc.y
	${CC} ${CFLAGS} -o ccc lex.yy.c y.tab.c

mhegc:	${OBJS}
	${CC} ${CFLAGS} -o mhegc ${OBJS} ${LIBS}

.c.o:
	${CC} ${CFLAGS} -c $<

install:	mhegc
	install -m 755 mhegc ${DESTDIR}/bin

clean:
	rm -f mhegc ccc *.o core

tar:
	make clean
	(cd ..; tar zcvf ${TARDIR}.tar.gz --exclude .svn ${TARDIR})

