#ifndef lint
static char const 
yyrcsid[] = "$FreeBSD: src/usr.bin/yacc/skeleton.c,v 1.28 2000/01/17 02:04:06 bde Exp $";
#endif
#include <stdlib.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING() (yyerrflag!=0)
static int yygrowstack();
#define YYPREFIX "yy"
#line 2 "src/sql.y"

#include <string>
#include <vector>
#include <stack>
#include <boost/algorithm/string.hpp>
#include "machine.h"
#include "sql_base.h"
#include "sql_select.h"
#include "sql_insert.h"
#include "sql_update.h"
#include "sql_delete.h"
#include "sql_create.h"
#include "sql_drop.h"
#include "sql_truncate.h"

using namespace ai::scci;

namespace ai
{
namespace scci
{

extern int errcnt;
extern int inquote;
extern int incomments;
extern string tminline;
extern int column;

}
}

extern int yylex();

#line 68 "src/sql.y"
typedef union
{
	long lval;
	double dval;
	char *sval;
	simple_item_t *simple_val;
	composite_item_t *composite_val;
	sql_func_t *fval;
	arg_list_t *aval;
	field_desc_t *desc_val;
	relation_item_t *rela_val;
	sql_statement *sql_val;
} YYSTYPE;
#line 65 "src/sql.cpp"
#define YYERRCODE 256
#define SELECT 257
#define FROM 258
#define WHERE 259
#define GROUP 260
#define BY 261
#define ORDER 262
#define HAVING 263
#define EXISTS 264
#define IN 265
#define INTERSECT 266
#define UNION 267
#define WITH 268
#define INSERT 269
#define INTO 270
#define VALUES 271
#define FOR 272
#define UPDATE 273
#define SET 274
#define DELETE 275
#define ASC 276
#define DESC 277
#define DISTINCT 278
#define BETWEEN 279
#define CREATE 280
#define TABLE 281
#define DROP 282
#define TRUNCATE 283
#define COMMA 284
#define COLON 285
#define SEMICOLON 286
#define DOT 287
#define LCURVE 288
#define RCURVE 289
#define LBRACKET 290
#define RBRACKET 291
#define LBRACE 292
#define RBRACE 293
#define IS_OP 294
#define NULL_OP 295
#define TYPE_CHAR 296
#define TYPE_UCHAR 297
#define TYPE_SHORT 298
#define TYPE_USHORT 299
#define TYPE_INT 300
#define TYPE_UINT 301
#define TYPE_LONG 302
#define TYPE_VCHAR 303
#define TYPE_ULONG 304
#define TYPE_FLOAT 305
#define TYPE_DOUBLE 306
#define TYPE_DATE 307
#define TYPE_TIME 308
#define TYPE_DATETIME 309
#define CONSTANT 310
#define BADTOKEN 311
#define DCONSTANT 312
#define IDENT 313
#define STRING 314
#define COMMENTS 315
#define OR_OP 316
#define AND_OP 317
#define NOT 318
#define GT 319
#define LT 320
#define GE 321
#define LE 322
#define EQ 323
#define NEQ 324
#define ADD 325
#define SUB 326
#define STRCAT 327
#define MUL 328
#define DIV 329
#define MOD 330
#define UMINUS 331
const short yylhs[] = {                                        -1,
    0,    0,    0,   43,   43,   43,   43,   43,   43,   43,
   36,    4,    4,    8,    8,   21,   21,   20,   20,   20,
   20,   20,   20,   23,   23,   23,   23,   23,   23,   23,
   23,   23,   23,   28,   28,   28,   28,   24,   24,   11,
   11,   30,   30,   31,   31,   31,   31,   32,   32,   32,
   32,   32,   32,   32,   32,   32,   32,   22,   22,   22,
   22,   22,   22,   22,   22,   22,   22,   22,   22,   12,
   12,   13,   13,   14,   14,   15,   15,   26,   26,   26,
   33,   33,   34,   34,   34,   34,   34,   35,    6,    9,
    9,    7,   10,   10,   10,    3,    3,    3,    3,    3,
    3,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,    2,    2,    2,   37,   37,   37,   37,   18,   18,
   16,   16,    5,    5,   38,   17,   17,   25,   39,   39,
    1,    1,   40,   19,   19,   27,   29,   29,   29,   41,
   42,
};
const short yylen[] = {                                         2,
    1,    2,    3,    1,    1,    1,    1,    1,    1,    1,
   11,    1,    0,    1,    3,    2,    3,    1,    3,    1,
    1,    1,    1,    1,    1,    3,    3,    3,    3,    3,
    3,    2,    3,    6,    6,    3,    0,    1,    2,    1,
    3,    2,    0,    1,    3,    3,    2,    3,    5,    6,
    5,    4,    5,    6,    3,    3,    4,    1,    1,    3,
    3,    3,    3,    3,    3,    3,    2,    3,    3,    3,
    0,    1,    3,    3,    0,    1,    3,    1,    2,    2,
    2,    0,    1,    3,    3,    2,    3,    3,    4,    1,
    3,    4,    1,    3,    0,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,   11,    8,    8,    5,    1,    3,
    1,    3,    2,    0,    6,    1,    3,    3,    5,    4,
    1,    0,    7,    1,    3,    2,    4,    4,    1,    3,
    3,
};
const short yydefred[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    4,    5,
    6,    7,    8,    9,   10,    0,  131,    0,    0,    0,
    0,    0,    0,    0,    0,   12,    0,    0,    0,    0,
    0,    0,    0,  140,  141,    3,    0,   23,   20,   21,
    0,   22,    0,   25,    0,   24,   14,    0,    0,   39,
    0,    0,    0,  130,    0,    0,    0,    0,   32,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   16,
    0,    0,  118,    0,    0,  126,  129,    0,    0,    0,
    0,    0,    0,   59,   58,    0,    0,   44,    0,   33,
   19,    0,    0,    0,   40,   15,    0,  103,  104,  105,
  106,  107,  108,    0,  109,  110,  111,  112,  113,  114,
    0,   17,    0,    0,    0,   29,   30,   31,    0,  119,
    0,    0,    0,  125,    0,    0,    0,    0,    0,    0,
   47,    0,   67,    0,    0,    0,    0,   96,   97,   98,
   99,  100,  101,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  134,    0,   89,    0,    0,    0,
    0,   36,    0,    0,    0,    0,    0,  127,    0,   60,
   68,   55,   69,    0,    0,    0,    0,    0,   56,    0,
    0,    0,    0,    0,   64,   65,   66,    0,    0,   46,
    0,    0,  139,  136,    0,  133,    0,   41,    0,    0,
    0,    0,    0,  116,  120,    0,  117,   52,    0,   92,
    0,    0,    0,   57,    0,    0,    0,  135,    0,    0,
    0,    0,    0,    0,    0,    0,   49,   53,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   34,   35,
    0,   50,   54,  137,  138,    0,    0,    0,   76,    0,
    0,    0,    0,   83,    0,   11,  115,    0,    0,   79,
   80,    0,    0,   86,    0,    0,    0,  123,   77,   87,
    0,    0,   85,
};
const short yydgoto[] = {                                       8,
   18,  111,  150,   27,  256,   44,   84,   45,   92,  174,
   94,  200,  234,  221,  247,  163,   75,  121,  154,   85,
   47,   86,   48,   30,   76,  249,  155,   70,  194,   54,
   87,   88,  238,  253,  254,  129,   10,   11,   12,   13,
   14,   15,   16,
};
const short yysindex[] = {                                    -53,
 -307, -307, -307, -307, -307, -267, -217,    0,    0,    0,
    0,    0,    0,    0,    0, -257,    0, -221, -235, -275,
 -248, -214, -275, -275,  -53,    0,  515, -275, -227, -184,
 -275, -152, -275,    0,    0,    0,  515,    0,    0,    0,
 -191,    0,  515,    0, -252,    0,    0, -224, -240,    0,
 -204, -152, -100,    0, -173,  -89, -192,  515,    0, -275,
  515,  573, -167,  515,  515,  515,  515,  515,  515,    0,
 -146, -142,    0, -170, -212,    0,    0, -129, -130, -177,
 -149, -100,  510,    0,    0,  456, -154,    0, -126,    0,
    0, -261,  105, -210,    0,    0, -106,    0,    0,    0,
    0,    0,    0,  -99,    0,    0,    0,    0,    0,    0,
 -103,    0, -246, -246, -246,    0,    0,    0,  510,    0,
 -190,  510, -204,    0,  -64, -167,  419, -161,  -88,  510,
    0, -134,    0,  -86,  510, -245,  -60,    0,    0,    0,
    0,    0,    0,  510,  510,  510,  510,  510,  510,  510,
 -100, -100,  587, -157,    0,  515,    0, -275,  -51,  -95,
  -91,    0, -139,  397,  -85, -178,  397,    0,  -30,    0,
    0,    0,    0, -112,  397,  -58, -134,  442,    0,  -70,
  -26,  -54,  -54,  -54,    0,    0,    0,  397,  -82,    0,
  -23,   -2,    0,    0, -126,    0,  105,    0,   33,   39,
   24,   41,  510,    0,    0,   12,    0,    0,  510,    0,
  -81,   23,  510,    0, -134,    6,   42,    0,  515,  112,
  102,   82,   98,  397,  510,  397,    0,    0,  397,   67,
  119,  122,  135,  110,  105,  515,  478,  153,    0,    0,
  104,    0,    0,    0,    0,  515,  143, -160,    0, -166,
  478,  578, -143,    0,  155,    0,    0,  105,  515,    0,
    0,  -72, -159,    0,  510,  478,  478,    0,    0,    0,
  397,  146,    0,
};
const short yyrindex[] = {                                      0,
 -200,  204,  167, -243,  206,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  490,    0,  542,    0,    0,
    0,    0,    0,    0,  497,    0,    0,    0,  377,    0,
    0,    3,    0,    0,    0,    0,    0,    0,    0,    0,
   92,    0,    0,    0,    0,    0,    0, -247,    0,    0,
    0,    3,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0, -247,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    3,    0,    0,    0,    0,    0,
    1,    0,    0,    0,    0,    0,  232,    0,    0,    0,
    0,    0,  106,  296,    0,    0,  205,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  277,  288,  333,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   19,    0,    0,    0,  166,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  415,    0,
    0,    0,    0,  169,    0,    0,   75,    0,    0,    0,
    0,    0,    0,    0,  175,    0,  166,    0,    0,    0,
    0,  120,  189,  207,    0,    0,    0,  140,  100,    0,
  176,    0,    0,    0,    0,    0,  192,    0,    0,  124,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  166,    0,    0,    0,    0,    0,
   21,    0,    0,  193,    0,  200,    0,    0,  358,    0,
    0,    0,    0,  423,  366,    0,    0,   13,    0,    0,
    0,    0,    0,    0,    0,    0,  199,   20,    0,    0,
    0,    0,   25,    0,    0,    0,    0,  407,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   81,   85,    0,
};
const short yygindex[] = {                                    474,
  240,  347, -236,    0,    0,    0,    0,    0,    0, -155,
    0,    0,    0,    0,    0,  278,    0,    0,    0,  -25,
  441,  584,  -13,   38,  394,  260,  325,  -63,    0,  -48,
  -75,    0,    0,  -69,    0,    9,    0,    0,    0,    0,
    0,    0,    0,
};
#define YYTABLESIZE 908
const short yytable[] = {                                     112,
   18,   46,   43,   77,  128,   60,  131,   17,    9,   31,
   37,   46,  124,   23,  132,  265,    1,   46,   37,   78,
   82,  211,  156,   56,   81,  265,  124,  157,   25,   59,
   71,   61,   46,    9,   28,   46,   37,   29,   46,   46,
   46,   46,   46,   46,   93,  159,   53,   72,   53,  179,
  113,  114,  115,  116,  117,  118,   26,   73,   32,  230,
   34,   35,  170,   24,   29,   49,   33,   62,   52,  132,
   55,  123,  180,  158,  128,  189,  190,  132,    1,    1,
   88,   67,   68,   69,   84,   50,   78,  132,   63,   51,
    1,   18,  206,  165,  132,   57,   58,   95,  166,   45,
   64,   65,   66,   67,   68,   69,   53,   79,   74,  132,
   80,  132,  132,  132,   89,  260,  261,   38,   79,   61,
   91,  250,    1,   75,   62,  132,  195,  172,   38,  270,
   46,  196,   39,  169,   40,   81,   42,   57,  130,   48,
   82,  119,  197,   39,  203,   40,   81,   42,   83,  204,
   79,  251,  122,  132,  151,  152,  266,  267,  125,   83,
   38,  151,  152,   78,   64,   65,   66,   67,   68,   69,
  120,  209,  266,  267,  207,   39,  210,   40,   81,   42,
  263,  264,  126,  160,   79,  212,  153,   80,   62,  162,
  161,   83,    1,   46,   38,  198,  272,  273,   74,   90,
  173,  177,  209,    1,  181,  235,   63,  227,  199,   39,
   46,   40,   81,   42,  201,    2,  171,   82,  202,    3,
   46,    4,  248,  231,  214,   83,    5,  205,    6,    7,
  171,   42,  258,   46,  152,   64,   65,   66,   67,   68,
   69,   19,   20,   21,   22,  248,  138,  139,  140,  141,
  142,  143,  144,  145,  146,  147,  148,  149,  208,   18,
   18,  215,   18,   18,  216,   18,  144,  145,  146,  147,
  148,  149,   18,  147,  148,  149,   26,   37,   37,   18,
   37,   37,   78,   37,   18,  217,   18,   27,   43,   18,
   37,   78,   82,  219,   18,   43,   81,   37,  124,  225,
  220,  124,   37,   78,   37,   78,   82,   37,   78,   82,
   81,  228,   37,   81,  222,  232,   18,   18,   18,   18,
   18,   18,   18,   18,   18,   18,   18,   18,   18,   18,
   18,  223,   28,  128,   37,   37,   37,   37,   37,   37,
   37,   37,   37,   37,   37,   37,   37,   37,   37,   18,
  209,  233,   88,   18,   18,  242,   84,   51,  128,   45,
  128,   45,   45,   18,  237,   72,   88,   18,   18,   88,
   84,   45,  236,   84,  239,   18,   38,   18,   61,   61,
   18,   61,   61,   18,   61,   45,   75,  203,   45,   90,
  240,   61,  257,  246,   90,   75,   88,   88,   61,   48,
   84,   48,   48,   61,   18,   61,   73,  243,   61,   75,
  244,   48,   75,   61,   71,   45,   18,   18,   18,   18,
   18,   18,   70,  245,  255,   48,  259,  268,   48,   64,
   65,   66,   67,   68,   69,   61,   61,   61,   61,   61,
   61,   61,   61,   61,   61,   61,   61,   62,   62,   95,
   62,   62,  121,   62,   95,   48,   48,  121,   93,  102,
   62,   74,  267,   93,  102,   63,   63,   62,   63,   63,
   74,   63,   62,  132,   62,   91,  122,   62,   63,  132,
   91,  122,   62,   94,   74,   63,  132,   74,   94,    1,
   63,   42,   63,   42,   42,   63,    2,  102,   36,  193,
   63,   96,  241,   42,   62,   62,   62,   62,   62,   62,
   62,   62,   62,   62,   62,   62,  168,   42,  269,  218,
   42,    0,   63,   63,   63,   63,   63,   63,   63,   63,
   63,   63,   63,   63,   26,    0,    0,    0,   26,   26,
    0,    0,    0,    0,    0,   27,    0,    0,   26,   27,
   27,    0,   26,   26,    0,   43,    0,   43,   43,   27,
   26,    0,   26,   27,   27,   26,    0,   43,   26,    0,
    0,   27,    0,   27,    0,    0,   27,    0,    0,   27,
    0,   43,    0,    0,   43,    0,    0,    0,    0,   26,
   28,    0,    0,    0,   28,   28,    0,    0,    0,    0,
   27,   26,   26,   26,   28,    0,    0,    0,   28,   28,
    0,    0,   27,   27,   27,    0,   28,   51,   28,   51,
   51,   28,    0,    0,   28,    0,    0,   72,   72,   51,
    0,    0,    0,   38,    0,   38,   38,   72,   38,   38,
    0,    0,    0,   51,    0,   28,   51,   38,   38,   72,
   38,   72,    0,    0,   72,    0,    0,   28,   28,   28,
   38,    0,   38,  127,   38,   38,  133,    0,   73,   73,
    0,    0,    0,   51,   51,    0,   71,   71,   73,    0,
    0,    0,    0,  134,   70,   70,   71,    0,    0,    0,
   73,    0,   73,    0,   70,   73,    0,  135,    0,    0,
   71,    0,  164,   71,    0,  167,    0,  171,   70,    0,
    0,   70,  136,  175,    0,  176,    0,    0,  178,    0,
  134,  144,  145,  146,  147,  148,  149,  182,  183,  184,
  185,  186,  187,  188,  135,    0,  137,  138,  139,  140,
  141,  142,  143,  144,  145,  146,  147,  148,  149,  136,
    0,    0,    0,    0,    0,    0,    0,    0,  213,    0,
  175,    0,   79,    0,    0,  250,  144,  145,  146,  147,
  148,  149,   38,  137,  138,  139,  140,  141,  142,  143,
  144,  145,  146,  147,  148,  149,  224,   39,    0,   40,
   81,   42,  226,    0,   79,  251,  229,  132,  175,    0,
    0,    0,   37,   83,   38,    0,    0,    0,  164,   38,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   39,
  252,   40,   81,   42,   39,    0,   40,   41,   42,   13,
    0,    0,    0,  262,  252,   83,   13,    0,    0,    0,
   43,    0,    0,    0,    0,    0,    0,    0,  271,  252,
  252,   13,    0,   13,   13,   13,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   13,   97,   98,
   99,  100,  101,  102,  103,  104,  105,  106,  107,  108,
  109,  110,  191,   98,   99,  100,  101,  102,  103,  192,
  105,  106,  107,  108,  109,  110,  138,  139,  140,  141,
  142,  143,  144,  145,  146,  147,  148,  149,
};
const short yycheck[] = {                                      63,
    0,   27,    0,   52,   80,  258,   82,  315,    0,  258,
  258,   37,    0,  281,  258,  252,  257,   43,    0,    0,
    0,  177,  284,   37,    0,  262,   75,  289,  286,   43,
  271,  284,   58,   25,  270,   61,  284,  313,   64,   65,
   66,   67,   68,   69,   58,   94,  259,  288,  259,  295,
   64,   65,   66,   67,   68,   69,  278,   49,   21,  215,
   23,   24,  126,  281,  313,   28,  281,  292,   31,  313,
   33,  284,  318,  284,    0,  151,  152,  278,  257,  257,
    0,  328,  329,  330,    0,  313,  264,  288,  313,  274,
  257,    0,  271,  284,  295,  287,  288,   60,  289,    0,
  325,  326,  327,  328,  329,  330,  259,  285,  313,  310,
  288,  312,  313,  314,  288,  276,  277,  295,  285,    0,
  313,  288,  257,    0,  292,  326,  284,  289,  295,  289,
  156,  289,  310,  125,  312,  313,  314,  287,  288,    0,
  318,  288,  156,  310,  284,  312,  313,  314,  326,  289,
  285,  318,  323,  288,  316,  317,  316,  317,  288,  326,
  295,  316,  317,  264,  325,  326,  327,  328,  329,  330,
  313,  284,  316,  317,  166,  310,  289,  312,  313,  314,
  250,  251,  313,  290,  285,  177,  313,  288,    0,  293,
  290,  326,  257,  219,  295,  158,  266,  267,    0,  289,
  289,  288,  284,  257,  265,  219,    0,  289,  260,  310,
  236,  312,  313,  314,  310,  269,  289,  318,  310,  273,
  246,  275,  236,  215,  295,  326,  280,  313,  282,  283,
  289,    0,  246,  259,  317,  325,  326,  327,  328,  329,
  330,    2,    3,    4,    5,  259,  319,  320,  321,  322,
  323,  324,  325,  326,  327,  328,  329,  330,  289,  259,
  260,  288,  262,  263,  288,  265,  325,  326,  327,  328,
  329,  330,  272,  328,  329,  330,    0,  259,  260,  279,
  262,  263,  263,  265,  284,  288,  286,    0,  286,  289,
  272,  272,  272,  261,  294,    0,  272,  279,  286,  288,
  262,  289,  284,  284,  286,  286,  286,  289,  289,  289,
  286,  289,  294,  289,  291,  310,  316,  317,  318,  319,
  320,  321,  322,  323,  324,  325,  326,  327,  328,  329,
  330,  291,    0,  259,  316,  317,  318,  319,  320,  321,
  322,  323,  324,  325,  326,  327,  328,  329,  330,  258,
  284,  310,  272,  262,  263,  289,  272,    0,  284,  260,
  286,  262,  263,  272,  263,    0,  286,  276,  277,  289,
  286,  272,  261,  289,  293,  284,    0,  286,  259,  260,
  289,  262,  263,  292,  265,  286,  263,  284,  289,  284,
  293,  272,  289,  284,  289,  272,  316,  317,  279,  260,
  316,  262,  263,  284,  313,  286,    0,  289,  289,  286,
  289,  272,  289,  294,    0,  316,  325,  326,  327,  328,
  329,  330,    0,  289,  272,  286,  284,  273,  289,  325,
  326,  327,  328,  329,  330,  316,  317,  318,  319,  320,
  321,  322,  323,  324,  325,  326,  327,  259,  260,  284,
  262,  263,  284,  265,  289,  316,  317,  289,  284,  284,
  272,  263,  317,  289,  289,  259,  260,  279,  262,  263,
  272,  265,  284,  270,  286,  284,  284,  289,  272,  313,
  289,  289,  294,  284,  286,  279,  281,  289,  289,    0,
  284,  260,  286,  262,  263,  289,    0,  293,   25,  153,
  294,   61,  225,  272,  316,  317,  318,  319,  320,  321,
  322,  323,  324,  325,  326,  327,  123,  286,  259,  195,
  289,   -1,  316,  317,  318,  319,  320,  321,  322,  323,
  324,  325,  326,  327,  258,   -1,   -1,   -1,  262,  263,
   -1,   -1,   -1,   -1,   -1,  258,   -1,   -1,  272,  262,
  263,   -1,  276,  277,   -1,  260,   -1,  262,  263,  272,
  284,   -1,  286,  276,  277,  289,   -1,  272,  292,   -1,
   -1,  284,   -1,  286,   -1,   -1,  289,   -1,   -1,  292,
   -1,  286,   -1,   -1,  289,   -1,   -1,   -1,   -1,  313,
  258,   -1,   -1,   -1,  262,  263,   -1,   -1,   -1,   -1,
  313,  325,  326,  327,  272,   -1,   -1,   -1,  276,  277,
   -1,   -1,  325,  326,  327,   -1,  284,  260,  286,  262,
  263,  289,   -1,   -1,  292,   -1,   -1,  262,  263,  272,
   -1,   -1,   -1,  257,   -1,  259,  260,  272,  262,  263,
   -1,   -1,   -1,  286,   -1,  313,  289,  271,  272,  284,
  274,  286,   -1,   -1,  289,   -1,   -1,  325,  326,  327,
  284,   -1,  286,   80,  288,  289,   83,   -1,  262,  263,
   -1,   -1,   -1,  316,  317,   -1,  262,  263,  272,   -1,
   -1,   -1,   -1,  265,  262,  263,  272,   -1,   -1,   -1,
  284,   -1,  286,   -1,  272,  289,   -1,  279,   -1,   -1,
  286,   -1,  119,  289,   -1,  122,   -1,  289,  286,   -1,
   -1,  289,  294,  130,   -1,  132,   -1,   -1,  135,   -1,
  265,  325,  326,  327,  328,  329,  330,  144,  145,  146,
  147,  148,  149,  150,  279,   -1,  318,  319,  320,  321,
  322,  323,  324,  325,  326,  327,  328,  329,  330,  294,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  317,   -1,
  177,   -1,  285,   -1,   -1,  288,  325,  326,  327,  328,
  329,  330,  295,  318,  319,  320,  321,  322,  323,  324,
  325,  326,  327,  328,  329,  330,  203,  310,   -1,  312,
  313,  314,  209,   -1,  285,  318,  213,  288,  215,   -1,
   -1,   -1,  288,  326,  295,   -1,   -1,   -1,  225,  295,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  310,
  237,  312,  313,  314,  310,   -1,  312,  313,  314,  288,
   -1,   -1,   -1,  250,  251,  326,  295,   -1,   -1,   -1,
  326,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  265,  266,
  267,  310,   -1,  312,  313,  314,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  326,  296,  297,
  298,  299,  300,  301,  302,  303,  304,  305,  306,  307,
  308,  309,  296,  297,  298,  299,  300,  301,  302,  303,
  304,  305,  306,  307,  308,  309,  319,  320,  321,  322,
  323,  324,  325,  326,  327,  328,  329,  330,
};
#define YYFINAL 8
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 331
#if YYDEBUG
const char * const yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"SELECT","FROM","WHERE","GROUP",
"BY","ORDER","HAVING","EXISTS","IN","INTERSECT","UNION","WITH","INSERT","INTO",
"VALUES","FOR","UPDATE","SET","DELETE","ASC","DESC","DISTINCT","BETWEEN",
"CREATE","TABLE","DROP","TRUNCATE","COMMA","COLON","SEMICOLON","DOT","LCURVE",
"RCURVE","LBRACKET","RBRACKET","LBRACE","RBRACE","IS_OP","NULL_OP","TYPE_CHAR",
"TYPE_UCHAR","TYPE_SHORT","TYPE_USHORT","TYPE_INT","TYPE_UINT","TYPE_LONG",
"TYPE_VCHAR","TYPE_ULONG","TYPE_FLOAT","TYPE_DOUBLE","TYPE_DATE","TYPE_TIME",
"TYPE_DATETIME","CONSTANT","BADTOKEN","DCONSTANT","IDENT","STRING","COMMENTS",
"OR_OP","AND_OP","NOT","GT","LT","GE","LE","EQ","NEQ","ADD","SUB","STRCAT",
"MUL","DIV","MOD","UMINUS",
};
const char * const yyrule[] = {
"$accept : raw_sql_stmts",
"raw_sql_stmts : sql_stmt",
"raw_sql_stmts : sql_stmt SEMICOLON",
"raw_sql_stmts : sql_stmt SEMICOLON raw_sql_stmts",
"sql_stmt : select_stmt",
"sql_stmt : insert_stmt",
"sql_stmt : update_stmt",
"sql_stmt : delete_stmt",
"sql_stmt : create_stmt",
"sql_stmt : drop_stmt",
"sql_stmt : truncate_stmt",
"select_stmt : SELECT comments_stmt distinct_list select_list FROM table_list where_clause group_clause order_clause having_clause lock_clause",
"distinct_list : DISTINCT",
"distinct_list :",
"select_list : select_item",
"select_list : select_list COMMA select_item",
"select_item : select_name type_def",
"select_item : select_name IDENT type_def",
"simple_item : IDENT",
"simple_item : IDENT DOT IDENT",
"simple_item : CONSTANT",
"simple_item : DCONSTANT",
"simple_item : STRING",
"simple_item : NULL_OP",
"select_name : simple_item",
"select_name : select_func_name",
"select_name : select_name ADD select_name",
"select_name : select_name SUB select_name",
"select_name : select_name STRCAT select_name",
"select_name : select_name MUL select_name",
"select_name : select_name DIV select_name",
"select_name : select_name MOD select_name",
"select_name : SUB select_name",
"select_name : LCURVE select_name RCURVE",
"type_def : LBRACE TYPE_CHAR LBRACKET CONSTANT RBRACKET RBRACE",
"type_def : LBRACE TYPE_VCHAR LBRACKET CONSTANT RBRACKET RBRACE",
"type_def : LBRACE ITYPE RBRACE",
"type_def :",
"table_name : IDENT",
"table_name : IDENT IDENT",
"table_list : table_name",
"table_list : table_list COMMA table_name",
"where_clause : WHERE where_list",
"where_clause :",
"where_list : where_item",
"where_list : where_list OR_OP where_list",
"where_list : where_list AND_OP where_list",
"where_list : NOT where_list",
"where_item : composite_item COMP_OP composite_item",
"where_item : composite_item IN LCURVE arg_list RCURVE",
"where_item : composite_item NOT IN LCURVE arg_list RCURVE",
"where_item : composite_item BETWEEN composite_item AND_OP composite_item",
"where_item : EXISTS LCURVE select_stmt RCURVE",
"where_item : composite_item IN LCURVE select_stmt RCURVE",
"where_item : composite_item NOT IN LCURVE select_stmt RCURVE",
"where_item : LCURVE where_list RCURVE",
"where_item : composite_item IS_OP NULL_OP",
"where_item : composite_item IS_OP NOT NULL_OP",
"composite_item : simple_item",
"composite_item : func_name",
"composite_item : COLON IDENT type_def",
"composite_item : composite_item ADD composite_item",
"composite_item : composite_item SUB composite_item",
"composite_item : composite_item STRCAT composite_item",
"composite_item : composite_item MUL composite_item",
"composite_item : composite_item DIV composite_item",
"composite_item : composite_item MOD composite_item",
"composite_item : SUB composite_item",
"composite_item : LCURVE composite_item RCURVE",
"composite_item : LCURVE select_stmt RCURVE",
"group_clause : GROUP BY group_list",
"group_clause :",
"group_list : select_name",
"group_list : group_list COMMA select_name",
"order_clause : ORDER BY order_list",
"order_clause :",
"order_list : order_item",
"order_list : order_list COMMA order_item",
"order_item : select_name",
"order_item : select_name ASC",
"order_item : select_name DESC",
"having_clause : HAVING having_list",
"having_clause :",
"having_list : having_item",
"having_list : having_list OR_OP having_list",
"having_list : having_list AND_OP having_list",
"having_list : NOT having_list",
"having_list : LCURVE having_list RCURVE",
"having_item : composite_item COMP_OP composite_item",
"select_func_name : IDENT LCURVE select_arg_list RCURVE",
"select_arg_list : select_name",
"select_arg_list : select_arg_list COMMA select_name",
"func_name : IDENT LCURVE arg_list RCURVE",
"arg_list : composite_item",
"arg_list : arg_list COMMA composite_item",
"arg_list :",
"COMP_OP : GT",
"COMP_OP : LT",
"COMP_OP : GE",
"COMP_OP : LE",
"COMP_OP : EQ",
"COMP_OP : NEQ",
"ITYPE : TYPE_CHAR",
"ITYPE : TYPE_UCHAR",
"ITYPE : TYPE_SHORT",
"ITYPE : TYPE_USHORT",
"ITYPE : TYPE_INT",
"ITYPE : TYPE_UINT",
"ITYPE : TYPE_LONG",
"ITYPE : TYPE_ULONG",
"ITYPE : TYPE_FLOAT",
"ITYPE : TYPE_DOUBLE",
"ITYPE : TYPE_DATE",
"ITYPE : TYPE_TIME",
"ITYPE : TYPE_DATETIME",
"insert_stmt : INSERT comments_stmt INTO table_name LCURVE ident_list RCURVE VALUES LCURVE insert_list RCURVE",
"insert_stmt : INSERT comments_stmt INTO table_name VALUES LCURVE insert_list RCURVE",
"insert_stmt : INSERT comments_stmt INTO table_name LCURVE ident_list RCURVE select_stmt",
"insert_stmt : INSERT comments_stmt INTO table_name select_stmt",
"ident_list : IDENT",
"ident_list : ident_list COMMA IDENT",
"insert_list : composite_item",
"insert_list : insert_list COMMA composite_item",
"lock_clause : FOR UPDATE",
"lock_clause :",
"update_stmt : UPDATE comments_stmt table_name SET update_list where_clause",
"update_list : update_item",
"update_list : update_list COMMA update_item",
"update_item : IDENT EQ composite_item",
"delete_stmt : DELETE comments_stmt FROM table_name where_clause",
"delete_stmt : DELETE comments_stmt table_name where_clause",
"comments_stmt : COMMENTS",
"comments_stmt :",
"create_stmt : CREATE comments_stmt TABLE table_name LCURVE create_list RCURVE",
"create_list : create_item",
"create_list : create_list COMMA create_item",
"create_item : IDENT create_type_def",
"create_type_def : TYPE_CHAR LCURVE CONSTANT RCURVE",
"create_type_def : TYPE_VCHAR LCURVE CONSTANT RCURVE",
"create_type_def : ITYPE",
"drop_stmt : DROP TABLE table_name",
"truncate_stmt : TRUNCATE TABLE table_name",
};
#endif
#if YYDEBUG
#include <stdio.h>
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
int yystacksize;
#line 980 "src/sql.y"

#include <string>
#include <vector>
#include "machine.h"

using namespace ai::sg;
using namespace ai::scci;
using namespace std;

void yyerror(const char *s)
{
	if (inquote) {
		cerr << (_("\nERROR: end of input reached without terminating quote\n")).str(SGLOCALE);
	} else if (incomments) {
		cerr << (_("\nERROR: end of input reached without terminating */\n")).str(SGLOCALE);
	} else {
		cerr << "\n" << tminline << "\n";
		cerr << std::setfill('^') << std::setw(column) << "" << "\n";
		cerr << std::setfill('^') << std::setw(column) << s << "\n";
	}
	errcnt++;
}

static simple_item_t * set_simple(wordtype_enum word_type, const string& value)
{
	simple_item_t *item = new simple_item_t();
	item->word_type = word_type;
	if (word_type == WORDTYPE_IDENT)
		item->value = boost::to_lower_copy(value);
	else
		item->value = value;
	return item;
}

static simple_item_t * set_simple(wordtype_enum word_type, const string& schema, const string& value)
{
	simple_item_t *item = new simple_item_t();
	item->word_type = word_type;
	item->schema = boost::to_lower_copy(schema);
	if (word_type == WORDTYPE_IDENT)
		item->value = boost::to_lower_copy(value);
	else
		item->value = value;
	return item;
}

static composite_item_t *set_relation(optype_enum op, composite_item_t *left, composite_item_t *right)
{
	composite_item_t *item = new composite_item_t();
	item->rela_item = new relation_item_t(op, left, right);
	return item;
}

static relation_item_t *set_relation(optype_enum op, relation_item_t *left, relation_item_t *right)
{
	composite_item_t *left_ = NULL;
	if (left) {
		left_ = new composite_item_t();
		left_->rela_item = left;
	}
	composite_item_t *right_ = new composite_item_t();
	right_->rela_item = right;
	return new relation_item_t(op, left_, right_);
}

#line 729 "src/sql.cpp"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack()
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    i = yyssp - yyss;
    newss = yyss ? (short *)realloc(yyss, newsize * sizeof *newss) :
      (short *)malloc(newsize * sizeof *newss);
    if (newss == NULL)
        return -1;
    yyss = newss;
    yyssp = newss + i;
    newvs = yyvs ? (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs) :
      (YYSTYPE *)malloc(newsize * sizeof *newvs);
    if (newvs == NULL)
        return -1;
    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab

#ifndef YYPARSE_PARAM
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG void
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif	/* ANSI-C/C++ */
#else	/* YYPARSE_PARAM */
#ifndef YYPARSE_PARAM_TYPE
#define YYPARSE_PARAM_TYPE void *
#endif
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG YYPARSE_PARAM_TYPE YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL YYPARSE_PARAM_TYPE YYPARSE_PARAM;
#endif	/* ANSI-C/C++ */
#endif	/* ! YYPARSE_PARAM */

int
yyparse (YYPARSE_PARAM_ARG)
    YYPARSE_PARAM_DECL
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register const char *yys;

    if ((yys = getenv("YYDEBUG")))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate])) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#if defined(lint) || defined(__GNUC__)
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#if defined(lint) || defined(__GNUC__)
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 4:
#line 91 "src/sql.y"
{
		(yyvsp[0].sql_val)->set_stmt_type(STMTTYPE_SELECT);
		(yyvsp[0].sql_val)->set_root();
		sql_statement::add_statement(yyvsp[0].sql_val);
	}
break;
case 5:
#line 97 "src/sql.y"
{
		(yyvsp[0].sql_val)->set_stmt_type(STMTTYPE_INSERT);
		(yyvsp[0].sql_val)->set_root();
		sql_statement::add_statement(yyvsp[0].sql_val);
	}
break;
case 6:
#line 103 "src/sql.y"
{
		(yyvsp[0].sql_val)->set_stmt_type(STMTTYPE_UPDATE);
		(yyvsp[0].sql_val)->set_root();
		sql_statement::add_statement(yyvsp[0].sql_val);
	}
break;
case 7:
#line 109 "src/sql.y"
{
		(yyvsp[0].sql_val)->set_stmt_type(STMTTYPE_DELETE);
		(yyvsp[0].sql_val)->set_root();
		sql_statement::add_statement(yyvsp[0].sql_val);
	}
break;
case 8:
#line 115 "src/sql.y"
{
		(yyvsp[0].sql_val)->set_stmt_type(STMTTYPE_CREATE);
		(yyvsp[0].sql_val)->set_root();
		sql_statement::add_statement(yyvsp[0].sql_val);
	}
break;
case 9:
#line 121 "src/sql.y"
{
		(yyvsp[0].sql_val)->set_stmt_type(STMTTYPE_DROP);
		(yyvsp[0].sql_val)->set_root();
		sql_statement::add_statement(yyvsp[0].sql_val);
	}
break;
case 10:
#line 127 "src/sql.y"
{
		(yyvsp[0].sql_val)->set_stmt_type(STMTTYPE_TRUNCATE);
		(yyvsp[0].sql_val)->set_root();
		sql_statement::add_statement(yyvsp[0].sql_val);
	}
break;
case 11:
#line 142 "src/sql.y"
{
		sql_select *item = new sql_select();
		item->set_comments(yyvsp[-9].sval);
		item->set_distinct_flag(yyvsp[-8].lval == 1);
		item->set_select_items(yyvsp[-7].aval);
		item->set_table_items(yyvsp[-5].aval);
		item->set_where_items(yyvsp[-4].rela_val);
		item->set_group_items(yyvsp[-3].aval);
		item->set_order_items(yyvsp[-2].aval);
		item->set_having_items(yyvsp[-1].rela_val);
		item->set_lock_flag(yyvsp[0].lval);
		yyval.sql_val = item;
		delete yyvsp[-9].sval;
	}
break;
case 12:
#line 160 "src/sql.y"
{
		yyval.lval = 1;
	}
break;
case 13:
#line 164 "src/sql.y"
{
		yyval.lval = 0;
	}
break;
case 14:
#line 171 "src/sql.y"
{
		arg_list_t *item = new arg_list_t();
		item->push_back(yyvsp[0].composite_val);
		yyval.aval = item;
	}
break;
case 15:
#line 177 "src/sql.y"
{
		(yyvsp[-2].aval)->push_back(yyvsp[0].composite_val);
		yyval.aval = yyvsp[-2].aval;
	}
break;
case 16:
#line 185 "src/sql.y"
{
		(yyvsp[-1].composite_val)->field_desc = yyvsp[0].desc_val;
		yyval.composite_val = yyvsp[-1].composite_val;
	}
break;
case 17:
#line 190 "src/sql.y"
{
		(yyvsp[0].desc_val)->alias = yyvsp[-1].sval;
		(yyvsp[-2].composite_val)->field_desc = yyvsp[0].desc_val;
		yyval.composite_val = yyvsp[-2].composite_val;
		delete []yyvsp[-1].sval;
	}
break;
case 18:
#line 200 "src/sql.y"
{
		yyval.simple_val = set_simple(WORDTYPE_IDENT, yyvsp[0].sval);
		delete []yyvsp[0].sval;
	}
break;
case 19:
#line 205 "src/sql.y"
{
		yyval.simple_val = set_simple(WORDTYPE_IDENT, yyvsp[-2].sval, yyvsp[0].sval);
		delete []yyvsp[-2].sval;
		delete []yyvsp[0].sval;
	}
break;
case 20:
#line 211 "src/sql.y"
{
		char buf[32];
		sprintf(buf, "%ld", yyvsp[0].lval);
		yyval.simple_val = set_simple(WORDTYPE_CONSTANT, buf);
	}
break;
case 21:
#line 217 "src/sql.y"
{
		char buf[32];
		sprintf(buf, "%f", yyvsp[0].dval);
		yyval.simple_val = set_simple(WORDTYPE_DCONSTANT, buf);
	}
break;
case 22:
#line 223 "src/sql.y"
{
		yyval.simple_val = set_simple(WORDTYPE_STRING, yyvsp[0].sval);
		delete []yyvsp[0].sval;
	}
break;
case 23:
#line 228 "src/sql.y"
{
		yyval.simple_val = set_simple(WORDTYPE_NULL, "");
	}
break;
case 24:
#line 235 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->simple_item = yyvsp[0].simple_val;
		yyval.composite_val = item;
	}
break;
case 25:
#line 241 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->func_item = yyvsp[0].fval;
		yyval.composite_val = item;
	}
break;
case 26:
#line 247 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_ADD, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 27:
#line 251 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_SUB, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 28:
#line 255 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_STRCAT, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 29:
#line 259 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_MUL, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 30:
#line 263 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_DIV, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 31:
#line 267 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_MOD, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 32:
#line 271 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_UMINUS, NULL, yyvsp[0].composite_val);
	}
break;
case 33:
#line 275 "src/sql.y"
{
		yyval.composite_val = yyvsp[-1].composite_val;
		(yyval.composite_val)->has_curve = true;
	}
break;
case 34:
#line 283 "src/sql.y"
{
		field_desc_t *item = new field_desc_t();
		item->type = SQLTYPE_STRING;
		item->len = yyvsp[-2].lval;
		yyval.desc_val = item;
	}
break;
case 35:
#line 290 "src/sql.y"
{
		field_desc_t *item = new field_desc_t();
		item->type = SQLTYPE_VSTRING;
		item->len = yyvsp[-2].lval;
		yyval.desc_val = item;
	}
break;
case 36:
#line 297 "src/sql.y"
{
		field_desc_t *item = new field_desc_t();
		item->type = static_cast<sqltype_enum>(yyvsp[-1].lval);
		yyval.desc_val = item;
	}
break;
case 37:
#line 303 "src/sql.y"
{
		field_desc_t *item = new field_desc_t();
		item->type = SQLTYPE_ANY;
		yyval.desc_val = item;
	}
break;
case 38:
#line 312 "src/sql.y"
{
		simple_item_t *item = set_simple(WORDTYPE_IDENT, yyvsp[0].sval);
		composite_item_t *composite_item = new composite_item_t();
		composite_item->simple_item = item;
		yyval.composite_val = composite_item;
		delete []yyvsp[0].sval;
	}
break;
case 39:
#line 320 "src/sql.y"
{
		simple_item_t *item = set_simple(WORDTYPE_IDENT, yyvsp[-1].sval, yyvsp[0].sval);
		composite_item_t *composite_item = new composite_item_t();
		composite_item->simple_item = item;
		yyval.composite_val = composite_item;
		delete []yyvsp[-1].sval;
		delete []yyvsp[0].sval;
	}
break;
case 40:
#line 332 "src/sql.y"
{
		arg_list_t *item = new arg_list_t();
		item->push_back(yyvsp[0].composite_val);
		yyval.aval = item;
	}
break;
case 41:
#line 338 "src/sql.y"
{
		(yyvsp[-2].aval)->push_back(yyvsp[0].composite_val);
		yyval.aval = yyvsp[-2].aval;
	}
break;
case 42:
#line 346 "src/sql.y"
{
		yyval.rela_val = yyvsp[0].rela_val;
	}
break;
case 43:
#line 350 "src/sql.y"
{
		yyval.rela_val = NULL;
	}
break;
case 44:
#line 357 "src/sql.y"
{
		yyval.rela_val = yyvsp[0].rela_val;
	}
break;
case 45:
#line 361 "src/sql.y"
{
		yyval.rela_val = set_relation(OPTYPE_OR, yyvsp[-2].rela_val, yyvsp[0].rela_val);
	}
break;
case 46:
#line 365 "src/sql.y"
{
		yyval.rela_val = set_relation(OPTYPE_AND, yyvsp[-2].rela_val, yyvsp[0].rela_val);
	}
break;
case 47:
#line 369 "src/sql.y"
{
		yyval.rela_val = set_relation(OPTYPE_NOT, NULL, yyvsp[0].rela_val);
	}
break;
case 48:
#line 376 "src/sql.y"
{
		yyval.rela_val = new relation_item_t(static_cast<optype_enum>(yyvsp[-1].lval), yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 49:
#line 380 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->arg_item = yyvsp[-1].aval;
		item->has_curve = true;
		yyval.rela_val = new relation_item_t(OPTYPE_IN, yyvsp[-4].composite_val, item);
	}
break;
case 50:
#line 387 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->arg_item = yyvsp[-1].aval;
		item->has_curve = true;
		yyval.rela_val = new relation_item_t(OPTYPE_NOT_IN, yyvsp[-5].composite_val, item);
	}
break;
case 51:
#line 394 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->rela_item = new relation_item_t(OPTYPE_AND, yyvsp[-2].composite_val, yyvsp[0].composite_val);
		yyval.rela_val = new relation_item_t(OPTYPE_BETWEEN, yyvsp[-4].composite_val, item);
	}
break;
case 52:
#line 400 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->select_item = dynamic_cast<sql_select *>(yyvsp[-1].sql_val);
		item->has_curve = true;
		yyval.rela_val = new relation_item_t(OPTYPE_EXISTS, NULL, item);
	}
break;
case 53:
#line 407 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->select_item = dynamic_cast<sql_select *>(yyvsp[-1].sql_val);
		item->has_curve = true;
		yyval.rela_val = new relation_item_t(OPTYPE_IN, yyvsp[-4].composite_val, item);
	}
break;
case 54:
#line 414 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->select_item = dynamic_cast<sql_select *>(yyvsp[-1].sql_val);
		item->has_curve = true;
		yyval.rela_val = new relation_item_t(OPTYPE_NOT_IN, yyvsp[-5].composite_val, item);
	}
break;
case 55:
#line 421 "src/sql.y"
{
		yyval.rela_val = yyvsp[-1].rela_val;
		(yyval.rela_val)->has_curve = true;
	}
break;
case 56:
#line 426 "src/sql.y"
{
		yyval.rela_val = new relation_item_t(OPTYPE_IS_NULL, yyvsp[-2].composite_val, NULL);
	}
break;
case 57:
#line 430 "src/sql.y"
{
		yyval.rela_val = new relation_item_t(OPTYPE_IS_NOT_NULL, yyvsp[-3].composite_val, NULL);
	}
break;
case 58:
#line 437 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->simple_item = yyvsp[0].simple_val;
		yyval.composite_val = item;
	}
break;
case 59:
#line 443 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->func_item = yyvsp[0].fval;
		yyval.composite_val = item;
	}
break;
case 60:
#line 449 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		(yyvsp[0].desc_val)->alias = yyvsp[-1].sval;
		item->field_desc = yyvsp[0].desc_val;
		yyval.composite_val = item;
		delete []yyvsp[-1].sval;
	}
break;
case 61:
#line 457 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_ADD, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 62:
#line 461 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_SUB, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 63:
#line 465 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_STRCAT, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 64:
#line 469 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_MUL, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 65:
#line 473 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_DIV, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 66:
#line 477 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_MOD, yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 67:
#line 481 "src/sql.y"
{
		yyval.composite_val = set_relation(OPTYPE_UMINUS, NULL, yyvsp[0].composite_val);
	}
break;
case 68:
#line 485 "src/sql.y"
{
		yyval.composite_val = yyvsp[-1].composite_val;
		(yyval.composite_val)->has_curve = true;
	}
break;
case 69:
#line 490 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->select_item = dynamic_cast<sql_select *>(yyvsp[-1].sql_val);
		item->has_curve = true;
		yyval.composite_val = item;
	}
break;
case 70:
#line 500 "src/sql.y"
{
		yyval.aval = yyvsp[0].aval;
	}
break;
case 71:
#line 504 "src/sql.y"
{
		yyval.aval = NULL;
	}
break;
case 72:
#line 511 "src/sql.y"
{
		arg_list_t *item = new arg_list_t();
		item->push_back(yyvsp[0].composite_val);
		yyval.aval = item;
	}
break;
case 73:
#line 517 "src/sql.y"
{
		(yyvsp[-2].aval)->push_back(yyvsp[0].composite_val);
		yyval.aval = yyvsp[-2].aval;
	}
break;
case 74:
#line 525 "src/sql.y"
{
		yyval.aval = yyvsp[0].aval;
	}
break;
case 75:
#line 529 "src/sql.y"
{
		yyval.aval = NULL;
	}
break;
case 76:
#line 536 "src/sql.y"
{
		arg_list_t *item = new arg_list_t();
		item->push_back(yyvsp[0].composite_val);
		yyval.aval = item;
	}
break;
case 77:
#line 542 "src/sql.y"
{
		(yyvsp[-2].aval)->push_back(yyvsp[0].composite_val);
		yyval.aval = yyvsp[-2].aval;
	}
break;
case 78:
#line 550 "src/sql.y"
{
		yyval.composite_val = yyvsp[0].composite_val;
	}
break;
case 79:
#line 554 "src/sql.y"
{
		yyval.composite_val = yyvsp[-1].composite_val;
		(yyval.composite_val)->order_type = ORDERTYPE_ASC;
	}
break;
case 80:
#line 559 "src/sql.y"
{
		yyval.composite_val = yyvsp[-1].composite_val;
		(yyval.composite_val)->order_type = ORDERTYPE_DESC;
	}
break;
case 81:
#line 567 "src/sql.y"
{
		yyval.rela_val = yyvsp[0].rela_val;
	}
break;
case 82:
#line 571 "src/sql.y"
{
		yyval.rela_val = NULL;
	}
break;
case 83:
#line 578 "src/sql.y"
{
		yyval.rela_val = yyvsp[0].rela_val;
	}
break;
case 84:
#line 582 "src/sql.y"
{
		yyval.rela_val = set_relation(OPTYPE_AND, yyvsp[-2].rela_val, yyvsp[0].rela_val);
	}
break;
case 85:
#line 586 "src/sql.y"
{
		yyval.rela_val = set_relation(OPTYPE_AND, yyvsp[-2].rela_val, yyvsp[0].rela_val);
	}
break;
case 86:
#line 590 "src/sql.y"
{
		yyval.rela_val = set_relation(OPTYPE_NOT, NULL, yyvsp[0].rela_val);
	}
break;
case 87:
#line 594 "src/sql.y"
{
		yyval.rela_val = yyvsp[-1].rela_val;
		(yyval.rela_val)->has_curve = true;
	}
break;
case 88:
#line 602 "src/sql.y"
{
		yyval.rela_val = new relation_item_t(static_cast<optype_enum>(yyvsp[-1].lval), yyvsp[-2].composite_val, yyvsp[0].composite_val);
	}
break;
case 89:
#line 609 "src/sql.y"
{
		sql_func_t *func_node = new sql_func_t();
		func_node->func_name = "";
		func_node->func_name += yyvsp[-3].sval;
		func_node->args = yyvsp[-1].aval;
		yyval.fval = func_node;
		delete []yyvsp[-3].sval;
	}
break;
case 90:
#line 621 "src/sql.y"
{
		arg_list_t *item = new arg_list_t();
		item->push_back(yyvsp[0].composite_val);
		yyval.aval = item;
	}
break;
case 91:
#line 627 "src/sql.y"
{
		(yyvsp[-2].aval)->push_back(yyvsp[0].composite_val);
		yyval.aval = yyvsp[-2].aval;
	}
break;
case 92:
#line 635 "src/sql.y"
{
		sql_func_t *func_node = new sql_func_t();
		func_node->func_name = "";
		func_node->func_name += yyvsp[-3].sval;
		func_node->args = yyvsp[-1].aval;
		yyval.fval = func_node;
		delete []yyvsp[-3].sval;
	}
break;
case 93:
#line 647 "src/sql.y"
{
		arg_list_t *item = new arg_list_t();
		item->push_back(yyvsp[0].composite_val);
		yyval.aval = item;
	}
break;
case 94:
#line 653 "src/sql.y"
{
		(yyvsp[-2].aval)->push_back(yyvsp[0].composite_val);
		yyval.aval = yyvsp[-2].aval;
	}
break;
case 95:
#line 658 "src/sql.y"
{
		yyval.aval = NULL;
	}
break;
case 96:
#line 665 "src/sql.y"
{
		yyval.lval = OPTYPE_GT;
	}
break;
case 97:
#line 669 "src/sql.y"
{
		yyval.lval = OPTYPE_LT;
	}
break;
case 98:
#line 673 "src/sql.y"
{
		yyval.lval = OPTYPE_GE;
	}
break;
case 99:
#line 677 "src/sql.y"
{
		yyval.lval = OPTYPE_LE;
	}
break;
case 100:
#line 681 "src/sql.y"
{
		yyval.lval = OPTYPE_EQ;
	}
break;
case 101:
#line 685 "src/sql.y"
{
		yyval.lval = OPTYPE_NEQ;
	}
break;
case 102:
#line 692 "src/sql.y"
{
		yyval.lval = SQLTYPE_CHAR;
	}
break;
case 103:
#line 696 "src/sql.y"
{
		yyval.lval = SQLTYPE_UCHAR;
	}
break;
case 104:
#line 700 "src/sql.y"
{
		yyval.lval = SQLTYPE_SHORT;
	}
break;
case 105:
#line 704 "src/sql.y"
{
		yyval.lval = SQLTYPE_USHORT;
	}
break;
case 106:
#line 708 "src/sql.y"
{
		yyval.lval = SQLTYPE_INT;
	}
break;
case 107:
#line 712 "src/sql.y"
{
		yyval.lval = SQLTYPE_UINT;
	}
break;
case 108:
#line 716 "src/sql.y"
{
		yyval.lval = SQLTYPE_LONG;
	}
break;
case 109:
#line 720 "src/sql.y"
{
		yyval.lval = SQLTYPE_ULONG;
	}
break;
case 110:
#line 724 "src/sql.y"
{
		yyval.lval = SQLTYPE_FLOAT;
	}
break;
case 111:
#line 728 "src/sql.y"
{
		yyval.lval = SQLTYPE_DOUBLE;
	}
break;
case 112:
#line 732 "src/sql.y"
{
		yyval.lval = SQLTYPE_DATE;
	}
break;
case 113:
#line 736 "src/sql.y"
{
		yyval.lval = SQLTYPE_TIME;
	}
break;
case 114:
#line 740 "src/sql.y"
{
		yyval.lval = SQLTYPE_DATETIME;
	}
break;
case 115:
#line 747 "src/sql.y"
{
		sql_insert *item = new sql_insert();
		item->set_comments(yyvsp[-9].sval);
		item->set_table_items(yyvsp[-7].composite_val);
		item->set_ident_items(yyvsp[-5].aval);
		item->set_insert_items(yyvsp[-1].aval);
		yyval.sql_val = item;
		delete yyvsp[-9].sval;
	}
break;
case 116:
#line 757 "src/sql.y"
{
		sql_insert *item = new sql_insert();
		item->set_comments(yyvsp[-6].sval);
		item->set_table_items(yyvsp[-4].composite_val);
		item->set_insert_items(yyvsp[-1].aval);
		yyval.sql_val = item;
		delete yyvsp[-6].sval;
	}
break;
case 117:
#line 766 "src/sql.y"
{
		sql_insert *item = new sql_insert();
		item->set_comments(yyvsp[-6].sval);
		item->set_table_items(yyvsp[-4].composite_val);
		item->set_ident_items(yyvsp[-2].aval);
		item->set_select_items(dynamic_cast<sql_select *>(yyvsp[0].sql_val));
		yyval.sql_val = item;
		delete yyvsp[-6].sval;
	}
break;
case 118:
#line 776 "src/sql.y"
{
		sql_insert *item = new sql_insert();
		item->set_comments(yyvsp[-3].sval);
		item->set_table_items(yyvsp[-1].composite_val);
		item->set_select_items(dynamic_cast<sql_select *>(yyvsp[0].sql_val));
		yyval.sql_val = item;
		delete yyvsp[-3].sval;
	}
break;
case 119:
#line 788 "src/sql.y"
{
		arg_list_t *item = new arg_list_t();
		simple_item_t *simple_item = set_simple(WORDTYPE_IDENT, yyvsp[0].sval);
		composite_item_t *composite_item = new composite_item_t();
		composite_item->simple_item = simple_item;
		item->push_back(composite_item);
		yyval.aval = item;
		delete []yyvsp[0].sval;
	}
break;
case 120:
#line 798 "src/sql.y"
{
		simple_item_t *simple_item = set_simple(WORDTYPE_IDENT, yyvsp[0].sval);
		composite_item_t *composite_item = new composite_item_t();
		composite_item->simple_item = simple_item;
		(yyvsp[-2].aval)->push_back(composite_item);
		yyval.aval = yyvsp[-2].aval;
		delete []yyvsp[0].sval;
	}
break;
case 121:
#line 810 "src/sql.y"
{
		arg_list_t *item = new arg_list_t();
		item->push_back(yyvsp[0].composite_val);
		yyval.aval = item;
	}
break;
case 122:
#line 816 "src/sql.y"
{
		(yyvsp[-2].aval)->push_back(yyvsp[0].composite_val);
		yyval.aval = yyvsp[-2].aval;
	}
break;
case 123:
#line 823 "src/sql.y"
{
		yyval.lval = 1;
	}
break;
case 124:
#line 827 "src/sql.y"
{
		yyval.lval = 0;
	}
break;
case 125:
#line 834 "src/sql.y"
{
		sql_update *item = new sql_update();
		item->set_comments(yyvsp[-4].sval);
		item->set_table_items(yyvsp[-3].composite_val);
		item->set_update_items(yyvsp[-1].aval);
		item->set_where_items(yyvsp[0].rela_val);
		yyval.sql_val = item;
		delete yyvsp[-4].sval;
	}
break;
case 126:
#line 847 "src/sql.y"
{
		arg_list_t *item = new arg_list_t();
		item->push_back(yyvsp[0].composite_val);
		yyval.aval = item;
	}
break;
case 127:
#line 853 "src/sql.y"
{
		(yyvsp[-2].aval)->push_back(yyvsp[0].composite_val);
		yyval.aval = yyvsp[-2].aval;
	}
break;
case 128:
#line 861 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->simple_item = set_simple(WORDTYPE_IDENT, yyvsp[-2].sval);
		yyval.composite_val = set_relation(OPTYPE_EQ, item, yyvsp[0].composite_val);
		delete []yyvsp[-2].sval;
	}
break;
case 129:
#line 871 "src/sql.y"
{
		sql_delete *item = new sql_delete();
		item->set_comments(yyvsp[-3].sval);
		item->set_table_items(yyvsp[-1].composite_val);
		item->set_where_items(yyvsp[0].rela_val);
		yyval.sql_val = item;
		delete yyvsp[-3].sval;

	}
break;
case 130:
#line 881 "src/sql.y"
{
		sql_delete *item = new sql_delete();
		item->set_comments(yyvsp[-2].sval);
		item->set_table_items(yyvsp[-1].composite_val);
		item->set_where_items(yyvsp[0].rela_val);
		yyval.sql_val = item;
		delete yyvsp[-2].sval;
	}
break;
case 131:
#line 893 "src/sql.y"
{
		yyval.sval = yyvsp[0].sval;
	}
break;
case 132:
#line 897 "src/sql.y"
{
		yyval.sval = NULL;
	}
break;
case 133:
#line 904 "src/sql.y"
{
		sql_create *item = new sql_create();
		item->set_comments(yyvsp[-5].sval);
		item->set_table_items(yyvsp[-3].composite_val);
		item->set_field_items(yyvsp[-1].aval);
		yyval.sql_val = item;
		delete yyvsp[-5].sval;
	}
break;
case 134:
#line 916 "src/sql.y"
{
		arg_list_t *item = new arg_list_t();
		item->push_back(yyvsp[0].composite_val);
		yyval.aval = item;
	}
break;
case 135:
#line 922 "src/sql.y"
{
		(yyvsp[-2].aval)->push_back(yyvsp[0].composite_val);
		yyval.aval = yyvsp[-2].aval;
	}
break;
case 136:
#line 930 "src/sql.y"
{
		composite_item_t *item = new composite_item_t();
		item->simple_item = set_simple(WORDTYPE_IDENT, yyvsp[-1].sval);
		item->field_desc = yyvsp[0].desc_val;
		yyval.composite_val = item;
	}
break;
case 137:
#line 940 "src/sql.y"
{
		field_desc_t *item = new field_desc_t();
		item->type = SQLTYPE_STRING;
		item->len = yyvsp[-1].lval;
		yyval.desc_val = item;
	}
break;
case 138:
#line 947 "src/sql.y"
{
		field_desc_t *item = new field_desc_t();
		item->type = SQLTYPE_VSTRING;
		item->len = yyvsp[-1].lval;
		yyval.desc_val = item;
	}
break;
case 139:
#line 954 "src/sql.y"
{
		field_desc_t *item = new field_desc_t();
		item->type = static_cast<sqltype_enum>(yyvsp[0].lval);
		yyval.desc_val = item;
	}
break;
case 140:
#line 963 "src/sql.y"
{
		sql_drop *item = new sql_drop();
		item->set_table_items(yyvsp[0].composite_val);
		yyval.sql_val = item;
	}
break;
case 141:
#line 972 "src/sql.y"
{
		sql_truncate *item = new sql_truncate();
		item->set_table_items(yyvsp[0].composite_val);
		yyval.sql_val = item;
	}
break;
#line 1956 "src/sql.cpp"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
