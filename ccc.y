%{
#include <stdio.h>
#include <string.h>

#define YYSTYPE char *

void
yyerror(const char *str)
{
	fprintf(stderr, "yyerror: %s\n", str);
}

int
yywrap(void)
{
	return 1;
}

int
main(void)
{
	yyparse();

	return 0;
}

%}

%token COMMENT
%token LITERAL
%token IDENTIFIER
%token DEFINEDAS
%token ALTERNATIVE
%token LBRACKET
%token RBRACKET
%token ONEORMORE
%token ENDCLAUSE
%token INVALID

%%
clauses:
	|
	clauses clause
	;

clause:
	COMMENT
	|
	IDENTIFIER DEFINEDAS definition ENDCLAUSE
	{
		printf("DEFINE:'%s'\n", $1);
	}
	;

definition:
	and_items
	|
	or_items
	;

and_items:
	item
	|
	and_items item
	;

or_items:
	item ALTERNATIVE item
	|
	or_items ALTERNATIVE item
	;

item:
	LITERAL
	{
		printf("LITERAL:'%s'\n", $1);
	}
	|
	IDENTIFIER
	{
		printf("IDENTIFIER:'%s'\n", $1);
	}
	|
	LBRACKET IDENTIFIER RBRACKET
	{
		printf("[IDENTIFIER]:'%s'\n", $2);
	}
	|
	IDENTIFIER ONEORMORE
	{
		printf("IDENTIFIER+:'%s'\n", $1);
	}
	;
%%
