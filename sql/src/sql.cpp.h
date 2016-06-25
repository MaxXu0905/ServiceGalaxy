#ifndef YYERRCODE
#define YYERRCODE 256
#endif

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
extern YYSTYPE yylval;
