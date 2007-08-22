%{
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define YYSTYPE char *

/* build up a list of items that define the current identifier */
enum item_type
{
	IT_LITERAL,	/* "LiteralString" */
	IT_IDENTIFIER,	/* NormalIdentifier */
	IT_OPTIONAL,	/* [OptionalIdentifier] */
	IT_ONEORMORE	/* OneOrMoreIdentifier+ */
};

struct item
{
	struct item *next;
	char *name;
	enum item_type type;
};

/* global state */
struct
{
	struct item *items;	/* NULL => start a new identifier */
	bool and_items;		/* true => identifier must contain all items */
} state;

/* input line we are currently parsing */
int yylineno = 1;

void
yyerror(const char *str)
{
	fprintf(stderr, "Error: %s at line %d\n", str, yylineno);

	return;
}

int
yywrap(void)
{
	return 1;
}

int
main(void)
{
	state.items = NULL;

	yyparse();

	return EXIT_SUCCESS;
}

void
fatal(char *str)
{
	yyerror(str);

	exit(EXIT_FAILURE);
}

void
add_item(enum item_type type, char *name)
{
	struct item *new_item;

	if(name == NULL)
		fatal("Out of memory");

	if(state.items == NULL)
	{
		if((state.items = malloc(sizeof(struct item))) == NULL)
			fatal("Out of memory");
		new_item = state.items;
	}
	else
	{
		/* find the end of the list */
		struct item *i = state.items;
		while(i->next)
			i = i->next;
		if((i->next = malloc(sizeof(struct item))) == NULL)
			fatal("Out of memory");
		new_item = i->next;
	}

	new_item->next = NULL;
	new_item->name = name;		/* lex strdup's it for us */
	new_item->type = type;

	return;
}

void
output_def(char *name)
{
	struct item *item;
	struct item *next;

printf("%s:\n", name);
for(item=state.items; item; item=item->next)
{
printf("\t%s",item->name);
if(item->type==IT_OPTIONAL) printf(" [optional]");
if(item->type==IT_ONEORMORE) printf(" oneormore+");
if(item->next && !state.and_items) printf (" |");
printf("\n");
}

	/* free the items */
	item = state.items;
	while(item)
	{
		next = item->next;
		free(item->name);
		free(item);
		item = next;
	}
	state.items = NULL;

	return;
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
		output_def($1);
	}
	;

definition:
	and_items
	{
		state.and_items = true;
	}
	|
	or_items
	{
		state.and_items = false;
	}
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
		add_item(IT_LITERAL, $1);
	}
	|
	IDENTIFIER
	{
		add_item(IT_IDENTIFIER, $1);
	}
	|
	LBRACKET IDENTIFIER RBRACKET
	{
		add_item(IT_OPTIONAL, $2);
	}
	|
	IDENTIFIER ONEORMORE
	{
		add_item(IT_ONEORMORE, $1);
	}
	;
%%
