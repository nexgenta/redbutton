%{
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>

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
	bool include;		/* should we output this optional item or not */
};

/* a list of strings */
struct str_list
{
	struct str_list *next;
	char *name;
};

/* build up the separate parts of the output file in these buffers */
struct buf
{
	char *str;		/* the buffer */
	size_t len;		/* length of the string in the buffer */
	size_t nalloced;	/* number of bytes malloc'ed to the buffer */
};

/* global state */
struct
{
	struct item *items;		/* NULL => start a new identifier */
	bool and_items;			/* true => identifier must contain all items */
	struct buf lexer;		/* lex output file */
	struct str_list *tokens;	/* "%token" section of the yacc output file */
	struct buf grammar;		/* grammar section of the yacc output file */
	struct str_list *oneormores;	/* grammar section for Identifier+ rules */
} state;

int yyparse(void);
int yylex(void);

void usage(char *);
void fatal(char *);

void add_item(enum item_type, char *);

void output_def(char *);
void output_item(struct item *, bool);

void print_tokens(struct str_list *);
char *add_token(struct str_list **, char *);
char *unquote(char *);

void print_oneormores(struct str_list *);
void add_oneormore(struct str_list **, char *);

void buf_init(struct buf *);
void buf_append(struct buf *, char *);

/* input line we are currently parsing */
int yylineno = 1;

/* yacc functions we need to provide */
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
	/* empty */
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

/* here we go ... */
int
main(int argc, char *argv[])
{
	char *prog_name = argv[0];
	/* by default output the grammar */
	bool show_lexer = false;
	int arg;

	while((arg = getopt(argc, argv, "l")) != EOF)
	{
		switch(arg)
		{
		case 'l':
			show_lexer = true;
			break;

		default:
			usage(prog_name);
			break;
		}
	}

	if(optind != argc)
		usage(prog_name);

	state.items = NULL;
	buf_init(&state.lexer);
	state.tokens = NULL;
	buf_init(&state.grammar);
	state.oneormores = NULL;

	yyparse();

	if(show_lexer)
	{
		/* output lexer */
		printf("%s", state.lexer.str);
	}
	else
	{
		/* output grammar */
		print_tokens(state.tokens);
		printf("%%%%\n");
		printf("%s", state.grammar.str);
		print_oneormores(state.oneormores);
		printf("%%%%\n");
	}

	return EXIT_SUCCESS;
}

void
usage(char *prog_name)
{
	fprintf(stderr, "Syntax: %s [-l]\n", prog_name);

	exit(EXIT_FAILURE);
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
	struct item *new_item = malloc(sizeof(struct item));

	/* did our malloc or lex's strdup fail */
	if(new_item == NULL || name == NULL)
		fatal("Out of memory");

	if(state.items == NULL)
	{
		state.items = new_item;
	}
	else
	{
		/* find the end of the list */
		struct item *i = state.items;
		while(i->next)
			i = i->next;
		i->next = new_item;
	}

	new_item->next = NULL;
	new_item->name = name;		/* lex strdup's it for us */
	new_item->type = type;
	new_item->include = true;

	return;
}

void
output_def(char *name)
{
	struct item *item;
	struct item *next;

	buf_append(&state.grammar, name);
	buf_append(&state.grammar, ":\n\t");

	/* output each item that makes up this identifier */
	for(item=state.items; item; item=item->next)
		output_item(item, true);

	buf_append(&state.grammar, ";\n\n");

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

void
output_item(struct item *item, bool recurse)
{
	char *tok_name;
	struct item *rest;

	switch(item->type)
	{
	case IT_LITERAL:
		tok_name = add_token(&state.tokens, item->name);
		buf_append(&state.grammar, tok_name);
		break;

	case IT_IDENTIFIER:
		buf_append(&state.grammar, item->name);
		break;

	case IT_OPTIONAL:
		if(recurse)
		{
			/*
			 * we are an optional item,
			 * so first output the remaining items
			 * this creates a rule which does not including us
			 */
			item->include = false;
			for(rest=item->next; rest; rest=rest->next)
				output_item(rest, true);
			/* or it with a rule which does contain us */
			if(item->next == NULL)
				buf_append(&state.grammar, "\n\t");
			buf_append(&state.grammar, "|\n\t");
			/*
			 * now output the items before us and output ourself,
			 * this constructs a rule including us
			 */
			item->include = true;
			for(rest=state.items; rest!=item; rest=rest->next)
				output_item(rest, false);
			buf_append(&state.grammar, item->name);
		}
		else if(item->include)
		{
			buf_append(&state.grammar, item->name);
		}
		break;

	case IT_ONEORMORE:
		/* add "OneOrMoreIdentifier" to the grammar */
		buf_append(&state.grammar, "OneOrMore");
		buf_append(&state.grammar, item->name);
		/* add the OneOrMoreIdentifier to our list */
		add_oneormore(&state.oneormores, item->name);
		break;

	default:
		fatal("Unexpected item type");
		break;
	}

	/* do we need all the items, or just one of them */
	if(state.and_items)
		buf_append(&state.grammar, item->next ? (item->include ? " " : "") : "\n\t");
	else
		buf_append(&state.grammar, item->next ? "\n\t|\n\t" : "\n\t");

	return;
}

void
print_tokens(struct str_list *t)
{
	while(t)
	{
		printf("%%token %s\n", t->name);
		t = t->next;
	}

	return;
}

char *
add_token(struct str_list **head, char *quoted)
{
	struct str_list *t = malloc(sizeof(struct str_list));
	struct str_list *list;

	if(t == NULL)
		fatal("Out of memory");

	t->next = NULL;
	t->name = unquote(quoted);

	/* check we haven't got it already */
	list = *head;
	while(list)
	{
		if(strcmp(list->name, t->name) == 0)
		{
			free(t->name);
			free(t);
			return list->name;
		}
		list = list->next;
	}

	/* add it to the end of the list */
	if(*head == NULL)
	{
		*head = t;
	}
	else
	{
		list = *head;
		while(list->next)
			list = list->next;
		list->next = t;
	}

	/* add it to the lex output file */
	buf_append(&state.lexer, quoted);
	buf_append(&state.lexer, "\treturn ");
	buf_append(&state.lexer, t->name);
	buf_append(&state.lexer, ";\n");

	return t->name;
}

char *
unquote(char *q)
{
	char *output;
	char *unq;

	/* check for special cases */
	if(strcmp(q, "\"}\"") == 0)
	{
		if((output = strdup("RBRACE")) == NULL)
			fatal("Out of memory");
		return output;
	}
	else if(strcmp(q, "\"(\"") == 0)
	{
		if((output = strdup("LPAREN")) == NULL)
			fatal("Out of memory");
		return output;
	}
	else if(strcmp(q, "\")\"") == 0)
	{
		if((output = strdup("RPAREN")) == NULL)
			fatal("Out of memory");
		return output;
	}

	/* max length it could be */
	if((output = malloc(strlen(q) + 1)) == NULL)
		fatal("Out of memory");

	/*
	 * remove any non-alphabetic chars (inc the start and end ")
	 * convert the remaining chars to uppercase
	 */
	unq = output;
	while(*q)
	{
		if(isalpha(*q))
		{
			*unq = toupper(*q);
			unq ++;
		}
		q ++;
	}

	/* terminate it */
	*unq = '\0';

	/* sanity check */
	if(output[0] == '\0')
		fatal("Invalid literal string");

	return output;
}

void
print_oneormores(struct str_list *list)
{
	while(list)
	{
		/* output the OneOrMoreIdentifier rule */
		printf("OneOrMore%s:\n", list->name);
		printf("\t%s\n", list->name);
		printf("\t|\n");
		printf("\tOneOrMore%s %s\n", list->name, list->name);
		printf("\t;\n\n");
		list = list->next;
	}

	return;
}

void
add_oneormore(struct str_list **head, char *name)
{
	struct str_list *list;

	/* check we haven't got it already */
	for(list=*head; list; list=list->next)
	{
		if(strcmp(list->name, name) == 0)
			return;
	}

	/* take a copy of the string */
	if((list = malloc(sizeof(struct str_list))) == NULL
	|| (list->name = strdup(name)) == NULL)
		fatal("Out of memory");

	/* add it to the head of the list */
	list->next = *head;
	*head = list;

	return;
}

#define INIT_BUF_SIZE	1024

void
buf_init(struct buf *b)
{
	b->len = 0;
	b->nalloced = INIT_BUF_SIZE;
	if((b->str = malloc(b->nalloced)) == NULL)
		fatal("Out of memory");

	return;
}

void
buf_append(struct buf *b, char *app_str)
{
	size_t app_len = strlen(app_str);

	/* +1 for the \0 terminator */
	while(b->nalloced < b->len + app_len + 1)
	{
		b->nalloced *= 2;
		if((b->str = realloc(b->str, b->nalloced)) == NULL)
			fatal("Out of memory");
	}

	memcpy(b->str + b->len, app_str, app_len);
	b->len += app_len;
	b->str[b->len] = '\0';

	return;
}

