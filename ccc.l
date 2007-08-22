%{
#include <string.h>
#include "y.tab.h"
%}
%%
\/\/[^\n\r\f]*		return COMMENT;
\"[^"]*\"		yylval = strdup(yytext); return LITERAL;
[a-z][a-z0-9]*		yylval = strdup(yytext); return IDENTIFIER;
"::="			return DEFINEDAS;
"|"			return ALTERNATIVE;
"["			return LBRACKET;
"]"			return RBRACKET;
"+"			return ONEORMORE;
"."			return ENDCLAUSE;
[\n\r\f]		/* ignore end of line */
[ \t]+			/* ignore whitespace */
.			return INVALID;
%%
