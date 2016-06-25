%{

#include "machine.h"
#include "sql_base.h"
#include "sql_statement.h"
using namespace ai::scci;
#include "sql.cpp.h"

%}

%Start A quote comments

W	[ \t\n]*
D	[0-9]
d	[1-9]
O	[0-7]
H	[0-9a-fA-F]

word	[$_a-zA-Z][$_a-zA-Z0-9]*

%{

static int getkeyword(const char *word);
static void count();

using namespace ai::scci;

namespace ai
{
namespace scci
{

int inquote = 0;
int incomments = 0;
string tminline;
int line_no = 0;
int column = 0;
string tmp_str;

}
}

%}

%%
	BEGIN A;

<A>"="		{ count(); return EQ; }
<A>"!="		{ count(); return NEQ; }
<A>"<>"		{ count(); return NEQ; }
<A>">="		{ count(); return GE; }
<A>"<="		{ count(); return LE; }
<A>">"		{ count(); return GT; }
<A>"<"		{ count(); return LT; }
<A>"+"		{ count(); return ADD; }
<A>"-"		{ count(); return SUB; }
<A>"*"		{ count(); return MUL; }
<A>"/"		{ count(); return DIV; }
<A>"%"		{ count(); return MOD; }
<A>"||"		{ count(); return STRCAT; }
<A>"("		{ count(); return LCURVE; }
<A>")"		{ count(); return RCURVE; }
<A>"["		{ count(); return LBRACKET; }
<A>"]"		{ count(); return RBRACKET; }
<A>"{"		{ count(); return LBRACE; }
<A>"}"		{ count(); return RBRACE; }
<A>":"		{ count(); return COLON; }
<A>";"		{ count(); return SEMICOLON; }
<A>","		{ count(); return COMMA; }
<A>"\."		{ count(); return DOT; }
<A>"0x"{H}+	{ count(); ::sscanf(yytext, "0x%lx", &yylval.lval); return CONSTANT; }
<A>"0X"{H}+	{ count(); ::sscanf(yytext, "0X%lx", &yylval.lval); return CONSTANT; }
<A>"0"{O}+	{ count(); ::sscanf(yytext, "0%lo", &yylval.lval); return CONSTANT; }
<A>0		{ count(); yylval.lval = 0; return CONSTANT; }
<A>[0-9]+ 	{ count(); yylval.lval = ::atol(yytext); return CONSTANT; }
<A>[0-9]*\.[0-9]+	{ count(); yylval.dval = ::atof(yytext); return DCONSTANT; }

<A>{word}	{
			int tok;

			count();
			if ((tok = getkeyword(yytext)) >= 0)
				return tok;
			int len = ::strlen(yytext);
			if ((yylval.sval = new char[len + 1]) == NULL) {
				cerr << (_("ERROR: Memory allocation failure")).str(SGLOCALE);
				::exit(1);
			}
			strcpy(yylval.sval, yytext);
			return IDENT;
		}

<A>[ \t\r]	{ count(); }

<A>'		{
			BEGIN quote;
			inquote = 1;
			tmp_str = "";
		}

<quote>''	{
			// echo an escaped quote
			count();
			tmp_str += yytext[0];
		}

<quote>'	{
			// end of quoted string
			BEGIN A;
			inquote = 0;
			count();
			if ((yylval.sval = new char[tmp_str.length() + 1]) == NULL) {
				cerr << (_("ERROR: Memory allocation failure")).str(SGLOCALE);
				::exit(1);
			}
			strcpy(yylval.sval, tmp_str.c_str());
			return STRING;
		}

<quote>.	{
			// echo any other single character
			count();
			tmp_str += yytext[0];
		}

<A>"/*+"	{
			BEGIN comments;
			incomments = 1;
			count();
			tmp_str = "/*+";
		}

<comments>"*/"	{
			// end of comments string
			BEGIN A;
			incomments = 0;
			count();
			tmp_str += "*/";
			if ((yylval.sval = new char[tmp_str.length() + 1]) == NULL) {
				cerr << (_("ERROR: Memory allocation failure")).str(SGLOCALE);
				::exit(1);
			}
			strcpy(yylval.sval, tmp_str.c_str());
			return COMMENTS;
		}

<comments>.	{
			// echo any other single character
			count();
			tmp_str += yytext[0];
		}

.		{
			count();
			return BADTOKEN;
		}

%%

int yywrap()
{
	return 1;
}

struct keyword_t
{
	const char *name;
	int tok;
};

keyword_t keywords[] = {
	{ "AND", AND_OP },
	{ "ASC", ASC },
	{ "BETWEEN", BETWEEN },
	{ "BY", BY },
	{ "CHAR", TYPE_CHAR },
	{ "CREATE", CREATE },
	{ "DATE", TYPE_DATE },
	{ "DATETIME", TYPE_DATETIME },
	{ "DELETE", DELETE },
	{ "DESC", DESC },
	{ "DISTINCT", DISTINCT },
	{ "DOUBLE", TYPE_DOUBLE },
	{ "DROP", DROP },
	{ "EXISTS", EXISTS },
	{ "FLOAT", TYPE_FLOAT },
	{ "FOR", FOR },
	{ "FROM", FROM },
	{ "GROUP", GROUP},
	{ "HAVING", HAVING },
	{ "IN", IN },
	{ "INSERT", INSERT },
	{ "INT", TYPE_INT },
	{ "INTERSECT", INTERSECT },
	{ "INTO", INTO },
	{ "IS", IS_OP },
	{ "LONG", TYPE_LONG },
	{ "NOT", NOT },
	{ "NULL", NULL_OP },
	{ "OR", OR_OP },
	{ "ORDER", ORDER },
	{ "SELECT", SELECT },
	{ "SET", SET },
	{ "SHORT", TYPE_SHORT },
	{ "TABLE", TABLE },
	{ "TIME", TYPE_TIME },
	{ "TRUNCATE", TRUNCATE },
	{ "UCHAR", TYPE_UCHAR },
	{ "UINT", TYPE_UINT },
	{ "ULONG", TYPE_ULONG },
	{ "UNION", UNION },
	{ "UPDATE", UPDATE },
	{ "USHORT", TYPE_USHORT },
	{ "VALUES", VALUES },
	{ "VCHAR", TYPE_VCHAR },
	{ "WHERE", WHERE },
	{ "WITH", WITH },
};

static int getkeyword(const char *word)
{
	int first = 0;
	int last;
	int middle;
	int comp;
	keyword_t *kw;

	kw = keywords;
	last = sizeof(keywords) / sizeof(keyword_t) - 1;

	do {	// binary search for keyword
		middle = (first + last) >> 1;
		comp = ::strcasecmp(word, kw[middle].name);
		if(comp == 0)
			return kw[middle].tok;

		if (comp < 0)
			last = middle - 1;
		if (comp > 0)
			first = middle + 1;
	} while (first <= last);

	return -1;
}

static void count()
{
	for (int i = 0; yytext[i] != '\0'; i++) {
		if (yytext[i] == '\n') {
			column = 0;
			::fprintf(yyout, "%s\n", tminline.c_str());
			tminline = "";
			line_no++;
		} else if (yytext[i] == '\t')
			column += 8 - (column % 8);
		else
			column++;
	}

	// ECHO
	tminline += yytext;
}

