%{
#include <string.h>
#include "ccc.tab.h"
extern int yylineno;
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
[\n\r\f]		yylineno ++;
[ \t]+			/* ignore whitespace */
.			return INVALID;
%%
