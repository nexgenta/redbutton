CC=gcc
CFLAGS=-Wall -O2

# for vasprintf
DEFS=-D_GNU_SOURCE

LEX=flex
YACC=bison
#LEX=lex
#YACC=yacc

DESTDIR=/usr/local

OBJS=	mhegc.o	\
	lex.parser.o	\
	parser.o	\
	der_encode.o	\
	asn1tag.o	\
	utils.o

TARDIR=`basename ${PWD}`

mhegc:	parser.h ${OBJS}
	${CC} ${CFLAGS} ${DEFS} -o mhegc ${OBJS} ${LIBS}

ccc:	ccc.y ccc.l asn1type.o
	${LEX} -i -t ccc.l > lex.ccc.c
	${YACC} -b ccc -d ccc.y
	${CC} ${CFLAGS} ${DEFS} -o ccc lex.ccc.c ccc.tab.c asn1type.o

lex.parser.c parser.c parser.h:	parser.l.* parser.c.* parser.h.* tokens.h.* grammar asn1tag.h ccc
	cat grammar | ./ccc -l parser.l -p parser.c -h parser.h -t tokens.h
	${LEX} -i -t parser.l > lex.parser.c

.c.o:
	${CC} ${CFLAGS} ${DEFS} -c $<

berdecode:	berdecode.c
	${CC} ${CFLAGS} ${DEFS} -o berdecode berdecode.c

install:	mhegc
	install -m 755 mhegc ${DESTDIR}/bin

clean:
	rm -f mhegc ccc lex.ccc.c ccc.tab.[ch] lex.parser.c parser.[lch] tokens.h *.o berdecode core

tar:
	make clean
	(cd ..; tar zcvf ${TARDIR}.tar.gz --exclude .svn ${TARDIR})

