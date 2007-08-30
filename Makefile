CC=gcc
CFLAGS=-Wall -O2

LEX=flex
YACC=bison
#LEX=lex
#YACC=yacc

DESTDIR=/usr/local

OBJS=	mhegc.o	\
	lex.parser.o	\
	parser.o	\
	utils.o

TARDIR=`basename ${PWD}`

mhegc:	parser.h ${OBJS}
	${CC} ${CFLAGS} -o mhegc ${OBJS} ${LIBS}

ccc:	ccc.y ccc.l asn1type.o
	${LEX} -i -t ccc.l > lex.ccc.c
	${YACC} -b ccc -d ccc.y
	${CC} ${CFLAGS} -o ccc lex.ccc.c ccc.tab.c asn1type.o

lex.parser.o parser.o parser.h:	parser.l.* parser.c.* parser.h.* tokens.h.* grammar ccc
	cat grammar | ./ccc -l parser.l -p parser.c -h parser.h -t tokens.h
	${LEX} -i -t parser.l > lex.parser.c
	${CC} ${CFLAGS} -c lex.parser.c
	${CC} ${CFLAGS} -c parser.c

.c.o:
	${CC} ${CFLAGS} -c $<

install:	mhegc
	install -m 755 mhegc ${DESTDIR}/bin

clean:
	rm -f mhegc ccc lex.ccc.c ccc.tab.[ch] lex.parser.c parser.[lch] tokens.h *.o core

tar:
	make clean
	(cd ..; tar zcvf ${TARDIR}.tar.gz --exclude .svn ${TARDIR})

