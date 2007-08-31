%{
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include "asn1type.h"

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

/* a list of strings */
struct str_list
{
	struct str_list *next;
	char *name;
};

/* build up the separate parts of the output files in these buffers */
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
	struct str_list *tokens;	/* tokens returned by the lexer */
	struct buf parse_fns;		/* parse_Xxx() C functions for the parser */
	struct buf parse_enum_fns;	/* parse_Xxx() C functions for enum values */
	struct buf is_fns;		/* is_Xxx() C functions for the parser */
	struct buf parse_hdr;		/* parse_Xxx() prototypes for the parser */
	struct buf parse_enum_hdr;	/* parse_Xxx() prototypes for enum values */
	struct buf is_hdr;		/* is_Xxx() prototypes for the parser */
} state;

/* header for files we generate */
#define STANDARD_HEADER	"/*\n * This file was automatically generated. Do not Edit!\n */\n\n"

int yyparse(void);
int yylex(void);

void usage(char *);
void fatal(char *);

void add_item(enum item_type, char *);

void output_def(char *);

char *add_token(struct str_list **, char *);
char *unquote(char *);

void buf_init(struct buf *);
void buf_append(struct buf *, char *, ...);

FILE *safe_fopen(char *, char *);
void file_append(FILE *, char *);

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
	char *lexer_name = NULL;
	char *parser_name = NULL;
	char *header_name = NULL;
	char *tokens_name = NULL;
	int arg;
	struct str_list *t;
	char header[PATH_MAX];
	char footer[PATH_MAX];

	while((arg = getopt(argc, argv, "l:p:h:t:")) != EOF)
	{
		switch(arg)
		{
		case 'l':
			lexer_name = optarg;
			break;

		case 'p':
			parser_name = optarg;
			break;

		case 'h':
			header_name = optarg;
			break;

		case 't':
			tokens_name = optarg;
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
	buf_init(&state.parse_fns);
	buf_init(&state.parse_enum_fns);
	buf_init(&state.is_fns);
	buf_init(&state.parse_hdr);
	buf_init(&state.parse_enum_hdr);
	buf_init(&state.is_hdr);

	yyparse();

	/* #define is_Xxx functions for the tokens */
	for(t=state.tokens; t; t=t->next)
		buf_append(&state.is_hdr, "#define is_%s(TOK)\t(TOK == %s)\n", t->name, t->name);

	/* output lexer */
	if(lexer_name != NULL)
	{
		FILE *lexer_file = safe_fopen(lexer_name, "w");
		fprintf(lexer_file, STANDARD_HEADER);
		/* output the header if there is one */
		snprintf(header, sizeof(header), "%s.header", lexer_name);
		file_append(lexer_file, header);
		/* output our stuff */
		fprintf(lexer_file, "%s", state.lexer.str);
		/* output the footer if there is one */
		snprintf(footer, sizeof(footer), "%s.footer", lexer_name);
		file_append(lexer_file, footer);
		fclose(lexer_file);
	}

	/* output parser C code */
	if(parser_name != NULL)
	{
		FILE *parser_file = safe_fopen(parser_name, "w");
		fprintf(parser_file, STANDARD_HEADER);
		/* output the header if there is one */
		snprintf(header, sizeof(header), "%s.header", parser_name);
		file_append(parser_file, header);
		/* output our stuff */
		fprintf(parser_file, "%s", state.parse_fns.str);
		fprintf(parser_file, "%s", state.parse_enum_fns.str);
		fprintf(parser_file, "%s", state.is_fns.str);
		/* output the footer if there is one */
		snprintf(footer, sizeof(footer), "%s.footer", parser_name);
		file_append(parser_file, footer);
		fclose(parser_file);
	}

	/* output parser header file */
	if(header_name != NULL)
	{
		FILE *header_file = safe_fopen(header_name, "w");
		fprintf(header_file, STANDARD_HEADER);
		/* output the header if there is one */
		snprintf(header, sizeof(header), "%s.header", header_name);
		file_append(header_file, header);
		/* output our stuff */
		fprintf(header_file, "%s", state.parse_hdr.str);
		fprintf(header_file, "%s", state.parse_enum_hdr.str);
		fprintf(header_file, "%s", state.is_hdr.str);
		/* output the footer if there is one */
		snprintf(footer, sizeof(footer), "%s.footer", header_name);
		file_append(header_file, footer);
		fclose(header_file);
	}

	/* output a header file defining the tokens for the lexer */
	if(tokens_name != NULL)
	{
		unsigned int tok_val;
		FILE *tokens_file = safe_fopen(tokens_name, "w");
		fprintf(tokens_file, STANDARD_HEADER);
		/* output the header if there is one */
		snprintf(header, sizeof(header), "%s.header", tokens_name);
		file_append(tokens_file, header);
		/* output our stuff */
		tok_val = 256;		// just needs to be larger than the last one in tokens.h.header
		for(t=state.tokens; t; t=t->next)
			fprintf(tokens_file, "#define %s\t%u\n", t->name, tok_val++);
		/* output the footer if there is one */
		snprintf(footer, sizeof(footer), "%s.footer", tokens_name);
		file_append(tokens_file, footer);
		fclose(tokens_file);
	}

	return EXIT_SUCCESS;
}

void
usage(char *prog_name)
{
	fprintf(stderr, "Syntax: %s [-l <lexer-file>] [-p <parser-c-file>] [-h <parser-h-file>] [-t <tokens-file>]\n", prog_name);

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

	/* find the end of the list */
	if(state.items == NULL)
	{
		state.items = new_item;
	}
	else
	{
		struct item *i = state.items;
		while(i->next)
			i = i->next;
		i->next = new_item;
	}

	new_item->next = NULL;
	new_item->name = name;		/* lex strdup's it for us */
	new_item->type = type;

	/* if it is a literal, make a token for it */
	if(new_item->type == IT_LITERAL)
		add_token(&state.tokens, new_item->name);

	return;
}

void
output_def(char *name)
{
	struct item *item;
	struct item *next;
	unsigned int nitems;

	/* prototype for the parse_Xxx function */
	buf_append(&state.parse_hdr, "void parse_%s(struct state *);\n", name);

	/* C code for the parse_Xxx functions */
	buf_append(&state.parse_fns, "void parse_%s(struct state *state)\n{\n", name);
	buf_append(&state.parse_fns, "\ttoken_t next;\n\n");
	buf_append(&state.parse_fns, "\tverbose(\"<%s>\\n\");\n\n", name);

	/* count how many items make it up */
	nitems = 0;
	/* skip literals at the start */
	item = state.items;
	while(item && item->type == IT_LITERAL)
		item = item->next;
	/* don't count literals at the end */
	while(item && item->type != IT_LITERAL)
	{
		nitems ++;
		item = item->next;
	}

	/* a single item (not including literals) */
	if(nitems == 1)
	{
		/* eat literals at the start */
		item = state.items;
		while(item && item->type == IT_LITERAL)
		{
			char *tok_name = unquote(item->name);
			buf_append(&state.parse_fns, "\t/* %s */\n", item->name);
			buf_append(&state.parse_fns, "\texpect_token(%s, %s);\n\n", tok_name, item->name);
			free(tok_name);
			item = item->next;
		}
		/* see if the next token is what we are expecting */
		buf_append(&state.parse_fns, "\tnext = peek_token();\n\n");
		if(item->type == IT_IDENTIFIER)
		{
			buf_append(&state.parse_fns, "\t/* %s */\n", item->name);
			buf_append(&state.parse_fns, "\tif(is_%s(next))\n", item->name);
			buf_append(&state.parse_fns, "\t\tparse_%s(state);\n", item->name);
			buf_append(&state.parse_fns, "\telse\n");
			buf_append(&state.parse_fns, "\t\tparse_error(\"Expecting %s\");\n", item->name);
		}
		else if(item->type == IT_OPTIONAL)
		{
			buf_append(&state.parse_fns, "\t/* [%s] */\n", item->name);
			buf_append(&state.parse_fns, "\tif(is_%s(next))\n", item->name);
			buf_append(&state.parse_fns, "\t\tparse_%s(state);\n", item->name);
		}
		else if(item->type == IT_ONEORMORE)
		{
			buf_append(&state.parse_fns, "\t/* %s+ */\n", item->name);
			buf_append(&state.parse_fns, "\twhile(is_%s(next))\n", item->name);
			buf_append(&state.parse_fns, "\t{\n");
			buf_append(&state.parse_fns, "\t\tparse_%s(state);\n", item->name);
			buf_append(&state.parse_fns, "\t\tnext = peek_token();\n");
			buf_append(&state.parse_fns, "\t}\n");
		}
		else
		{
			/* assert */
			fatal("nitems==1 but not Identifier/[Identifier]/Identifier+");
		}
		/* eat literals at the end */
		item = item->next;
		while(item)
		{
			char *tok_name = unquote(item->name);
			buf_append(&state.parse_fns, "\t/* %s */\n", item->name);
			buf_append(&state.parse_fns, "\texpect_token(%s, %s);\n\n", tok_name, item->name);
			free(tok_name);
			item = item->next;
		}
	}
	/* more than one item (not including literals) */
	else
	{
		/*
		 * do we need to pick one item, or do we need them all?
		 * ie are we building a CHOICE/ENUMERATED or a SET/SEQUENCE type
		 * does the order in which the items appear matter?
		 * ie are we building an ordered (SEQUENCE) or unordered (SET) type
		 */
/* TODO: could probably just check and_items rather than doing asn1type() now we know the grammar is consistent */
		switch(asn1type(name))
		{
		case ASN1TYPE_CHOICE:
		case ASN1TYPE_ENUMERATED:
			/* assert */
			if(state.and_items)
				fatal("CHOICE or ENUMERATED type, but and_items set");
			buf_append(&state.parse_fns, "\tnext = peek_token();\n\n");
			buf_append(&state.parse_fns, "\t/* CHOICE or ENUMERATED */\n");
			item = state.items;
			for(item=state.items; item; item=item->next)
			{
				/* is it the first */
				if(item == state.items)
					buf_append(&state.parse_fns, "\t");
				else
					buf_append(&state.parse_fns, "\telse ");
				if(item->type == IT_IDENTIFIER)
				{
					buf_append(&state.parse_fns, "if(is_%s(next))\n", item->name);
					buf_append(&state.parse_fns, "\t\tparse_%s(state);\n", item->name);
				}
				else if(item->type == IT_LITERAL)
				{
					char *tok_name = unquote(item->name);
					/* assert */
					if(asn1type(name) != ASN1TYPE_ENUMERATED)
						fatal("literal but not enum");
					buf_append(&state.parse_fns, "if(is_%s(next))\n", tok_name);
					buf_append(&state.parse_fns, "\t\tparse_%s(state);\n", tok_name);
					/* create a parse_Xxx function for the enum value */
					buf_append(&state.parse_enum_hdr, "void parse_%s(struct state *);\n", tok_name);
					buf_append(&state.parse_enum_fns, "void parse_%s(struct state *state)\n{\n", tok_name);
					buf_append(&state.parse_enum_fns, "\texpect_token(%s, %s);\n", tok_name, item->name);
					buf_append(&state.parse_enum_fns, "\n\tverbose(\"<ENUM value=%s/>\\n\");\n", tok_name);
					buf_append(&state.parse_enum_fns, "\n\treturn;\n}\n\n");
					free(tok_name);
				}
				else
				{
					/* assert */
					fatal("CHOICE/ENUMERATED but not Identifier or Literal");
				}
			}
			buf_append(&state.parse_fns, "\telse\n");
			buf_append(&state.parse_fns, "\t\tparse_error(\"Unexpected token\");\n");
			break;

		case ASN1TYPE_SET:
			/* assert */
			if(!state.and_items)
				fatal("SET but and_items not set");
			/* eat any literals at the start */
			item = state.items;
			while(item && item->type == IT_LITERAL)
			{
				char *tok_name = unquote(item->name);
				buf_append(&state.parse_fns, "\t/* %s */\n", item->name);
				buf_append(&state.parse_fns, "\texpect_token(%s, %s);\n\n", tok_name, item->name);
				free(tok_name);
				item = item->next;
			}
			/* keep parsing items until we get one that should not be in the SET */
			buf_append(&state.parse_fns, "\t/* SET */\n");
			buf_append(&state.parse_fns, "\twhile(true)\n\t{\n");
			buf_append(&state.parse_fns, "\t\tnext = peek_token();\n");
			while(item && item->type != IT_LITERAL)
			{
				/* assert */
				if(item->type != IT_IDENTIFIER && item->type != IT_OPTIONAL)
					fatal("SET but not Identifier or Optional");
				buf_append(&state.parse_fns, "\t\t/* %s */\n", item->name);
				buf_append(&state.parse_fns, "\t\tif(is_%s(next))\n\t\t{\n", item->name);
				buf_append(&state.parse_fns, "\t\t\tparse_%s(state);\n", item->name);
				buf_append(&state.parse_fns, "\t\t\tcontinue;\n\t\t}\n");
				item = item->next;
			}
			/* didn't match any items, must be the end of the SET */
			buf_append(&state.parse_fns, "\t\telse\n\t\t{\n");
			buf_append(&state.parse_fns, "\t\t\tbreak;\n\t\t}\n");
			buf_append(&state.parse_fns, "\t}\n");
			/* eat any trailing literals */
			while(item && item->type == IT_LITERAL)
			{
				char *tok_name = unquote(item->name);
				buf_append(&state.parse_fns, "\t/* %s */\n", item->name);
				buf_append(&state.parse_fns, "\texpect_token(%s, %s);\n\n", tok_name, item->name);
				free(tok_name);
				item = item->next;
			}
			break;

		case ASN1TYPE_SEQUENCE:
			/* assert */
			if(!state.and_items)
				fatal("SEQUENCE but and_items not set");
			buf_append(&state.parse_fns, "\t/* SEQUENCE */\n");
			item = state.items;
			for(item=state.items; item; item=item->next)
			{
				/* assert */
				if(item->type != IT_IDENTIFIER && item->type != IT_LITERAL && item->type != IT_OPTIONAL)
					fatal("SEQUENCE but not Identifier, Literal or Optional");
				/* eat literals, parse [optional] identifiers */
				buf_append(&state.parse_fns, "\n\t/* %s */\n", item->name);
				if(item->type == IT_LITERAL)
				{
					char *tok_name = unquote(item->name);
					buf_append(&state.parse_fns, "\texpect_token(%s, %s);\n", tok_name, item->name);
					free(tok_name);
				}
				else
				{
					buf_append(&state.parse_fns, "\tnext = peek_token();\n");
					buf_append(&state.parse_fns, "\tif(is_%s(next))\n", item->name);
					buf_append(&state.parse_fns, "\t\tparse_%s(state);\n", item->name);
					if(item->type != IT_OPTIONAL)
					{
						buf_append(&state.parse_fns, "\telse\n");
						buf_append(&state.parse_fns, "\t\tparse_error(\"Expecting %s\");\n", item->name);
					}
				}
			}
			break;

		default:
			/* assert */
			fatal("Illegal ASN1TYPE");
			break;
		}
	}
	buf_append(&state.parse_fns, "\n\tverbose(\"</%s>\\n\");\n", name);
	buf_append(&state.parse_fns, "\n\treturn;\n}\n\n");

	/*
	 * generate the is_Xxx(token_t) functions
	 * these functions should return true if the given token can be the first token for this type
	 * for unordered types (SET/CHOICE/ENUMERATED) check if any of the items match the token
	 * for ordered types (SEQUENCE) check if the first item matches the token
	 */

	/* prototype for the is_Xxx functions */
	buf_append(&state.is_hdr, "bool is_%s(token_t);\n", name);

	/* C code for the is_Xxx functions */
	buf_append(&state.is_fns, "bool is_%s(token_t tok)\n{\n", name);

	/* count the number of items */
	nitems = 0;
	for(item=state.items; item; item=item->next)
		nitems ++;

	/*
	 * for single items (or ones that start with a literal) the token must match the first item
	 * unless it's an ENUMERATED type,
	 * in which case all items are literals and the token can match any of them
	 */
	if(nitems == 1
	|| state.items->type == IT_LITERAL)
	{
		/* if it is an enum, check if any of the enum items match the token */
		bool is_enum = true;
		for(item=state.items; item && is_enum; item=item->next)
			is_enum = is_enum && (item->type == IT_LITERAL);
		item = state.items;
		if(is_enum)
		{
			/* assert */
			if(asn1type(name) != ASN1TYPE_ENUMERATED)
				fatal("is_enum but not ENUMERATED");
			buf_append(&state.is_fns, "\treturn ");
			while(item)
			{
				char *tok_name = unquote(item->name);
				buf_append(&state.is_fns, "is_%s(tok)", tok_name);
				free(tok_name);
				/* is it the last one */
				if(item->next)
					buf_append(&state.is_fns, "\n\t    || ");
				else
					buf_append(&state.is_fns, ";\n");
				item = item->next;
			}
		}
		/* not an enum, just check if the first item matches the token */
		else if(item->type == IT_LITERAL)
		{
			char *tok_name = unquote(item->name);
			buf_append(&state.is_fns, "\treturn is_%s(tok);\n", tok_name);
			free(tok_name);
		}
		else
		{
			buf_append(&state.is_fns, "\treturn is_%s(tok);\n", item->name);
		}
	}
	else
	{
		switch(asn1type(name))
		{
/* TODO: we have taken care of ENUMERATED above */
		case ASN1TYPE_ENUMERATED:
		case ASN1TYPE_CHOICE:
		case ASN1TYPE_SET:
			/* check if any of the items match the token */
			buf_append(&state.is_fns, "\treturn ");
			for(item=state.items; item; item=item->next)
			{
				/* assert */
				if(item->type != IT_IDENTIFIER && item->type != IT_OPTIONAL)
					fatal("is_fns: expecting Identifier or [Identifier]");
				buf_append(&state.is_fns, "is_%s(tok)", item->name);
				/* is it the last one */
				if(item->next)
					buf_append(&state.is_fns, "\n\t    || ");
				else
					buf_append(&state.is_fns, ";\n");
			}
			break;

		case ASN1TYPE_SEQUENCE:
			/* check if the first item matches the token */
			item = state.items;
			if(item->type == IT_LITERAL)
			{
				char *tok_name = unquote(item->name);
				buf_append(&state.is_fns, "\treturn (tok == %s);\n", tok_name);
				free(tok_name);
			}
			else if(item->type == IT_IDENTIFIER)
			{
				buf_append(&state.is_fns, "\treturn is_%s(tok);\n", item->name);
			}
			else
			{
				/* assert */
				fatal("SEQUENCE but first item not Literal or Identifier");
			}
			break;

		default:
			/* assert */
			fatal("Illegal ASN1TYPE");
			break;
		}
	}
	buf_append(&state.is_fns, "}\n\n");

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
	buf_append(&state.lexer, "%s\treturn %s;\n", quoted, t->name);

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
buf_append(struct buf *b, char *fmt, ...)
{
	va_list ap;
	char *app_str;
	size_t app_len;

	va_start(ap, fmt);
	if((app_len = vasprintf(&app_str, fmt, ap)) < 0)
		fatal("Out of memory or illegal format string");
	va_end(ap);

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

	free(app_str);

	return;
}

FILE *
safe_fopen(char *path, char *mode)
{
	FILE *f;

	if((f = fopen(path, mode)) == NULL)
	{
		fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	return f;
}

void
file_append(FILE *out, char *path)
{
	FILE *append = fopen(path, "r");
	char buf[BUFSIZ];
	size_t nread;

	/* don't care if the file does not exist */
	if(append == NULL)
	{
		if(errno == ENOENT)
			return;
		fprintf(stderr, "Unable to read %s: %s\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* we are using stdio, so copy in BUFSIZ blocks */
	while(!feof(append))
	{
		nread = fread(buf, 1, sizeof(buf), append);
		if(fwrite(buf, 1, nread, out) != nread)
		{
			fprintf(stderr, "Unable to append %s: %s", path, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	return;
}

