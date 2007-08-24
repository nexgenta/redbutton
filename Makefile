CC=gcc
CFLAGS=-Wall -O2

LEX=flex
YACC=bison
#LEX=lex
#YACC=yacc

DESTDIR=/usr/local

OBJS=	mhegc.o	\
	lex.parser.o	\
	parser.tab.o	\
	utils.o

TARDIR=`basename ${PWD}`

mhegc:	${OBJS}
	${CC} ${CFLAGS} -o mhegc ${OBJS} ${LIBS}

mhegc.o:	mhegc.c parser.tab.h

parser.tab.h:	parser.tab.c

ccc:	ccc.y ccc.l
	${LEX} -i -t ccc.l > lex.ccc.c
	${YACC} -b ccc -d ccc.y
	${CC} ${CFLAGS} -o ccc lex.ccc.c ccc.tab.c

parser.l:	parser.l.header grammar ccc
	cat parser.l.header > parser.l
	cat grammar | ./ccc -l >> parser.l
	cat parser.l.footer >> parser.l

parser.y:	parser.y.header parser.y.footer grammar ccc
	cat parser.y.header > parser.y
	cat grammar | ./ccc >> parser.y
	cat parser.y.footer >> parser.y

lex.parser.c:	parser.l
	${LEX} -i -t parser.l > lex.parser.c

parser.tab.c:	parser.y
	${YACC} -b parser -d parser.y

.c.o:
	${CC} ${CFLAGS} -c $<

install:	mhegc
	install -m 755 mhegc ${DESTDIR}/bin

clean:
	rm -f mhegc ccc lex.*.c *.tab.[ch] parser.l parser.y *.o core

tar:
	make clean
	(cd ..; tar zcvf ${TARDIR}.tar.gz --exclude .svn ${TARDIR})

