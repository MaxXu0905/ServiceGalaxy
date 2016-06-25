%{

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

%}

%token SELECT FROM WHERE GROUP BY ORDER HAVING EXISTS IN INTERSECT UNION WITH
%token INSERT INTO VALUES FOR UPDATE SET DELETE ASC DESC DISTINCT BETWEEN
%token CREATE TABLE DROP TRUNCATE
%token COMMA COLON SEMICOLON DOT LCURVE RCURVE LBRACKET RBRACKET LBRACE RBRACE
%token IS_OP NULL_OP
%token TYPE_CHAR TYPE_UCHAR TYPE_SHORT TYPE_USHORT TYPE_INT TYPE_UINT TYPE_LONG TYPE_VCHAR
%token TYPE_ULONG TYPE_FLOAT TYPE_DOUBLE TYPE_DATE TYPE_TIME TYPE_DATETIME
%token <lval> CONSTANT BADTOKEN
%token <dval> DCONSTANT
%token <sval> IDENT STRING COMMENTS

%type <sval> comments_stmt
%type <lval> ITYPE COMP_OP distinct_list lock_clause
%type <fval> select_func_name func_name
%type <aval> select_list select_arg_list arg_list table_list group_clause group_list order_clause order_list insert_list update_list ident_list create_list
%type <simple_val> simple_item
%type <composite_val> select_item composite_item select_name table_name update_item order_item create_item
%type <desc_val> type_def create_type_def
%type <rela_val> where_clause where_list where_item having_clause having_list having_item
%type <sql_val> select_stmt insert_stmt update_stmt delete_stmt create_stmt drop_stmt truncate_stmt

%left OR_OP
%left AND_OP
%right NOT
%left GT LT GE LE EQ NEQ
%left ADD SUB STRCAT
%left MUL DIV MOD
%right UMINUS // precedence for unary minus

%start raw_sql_stmts

%union
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
}

%%

raw_sql_stmts:
	sql_stmt
	| sql_stmt SEMICOLON
	| sql_stmt SEMICOLON raw_sql_stmts

sql_stmt:
	select_stmt
	{
		($1)->set_stmt_type(STMTTYPE_SELECT);
		($1)->set_root();
		sql_statement::add_statement($1);
	}
	| insert_stmt
	{
		($1)->set_stmt_type(STMTTYPE_INSERT);
		($1)->set_root();
		sql_statement::add_statement($1);
	}
	| update_stmt
	{
		($1)->set_stmt_type(STMTTYPE_UPDATE);
		($1)->set_root();
		sql_statement::add_statement($1);
	}
	| delete_stmt
	{
		($1)->set_stmt_type(STMTTYPE_DELETE);
		($1)->set_root();
		sql_statement::add_statement($1);
	}
	| create_stmt
	{
		($1)->set_stmt_type(STMTTYPE_CREATE);
		($1)->set_root();
		sql_statement::add_statement($1);
	}
	| drop_stmt
	{
		($1)->set_stmt_type(STMTTYPE_DROP);
		($1)->set_root();
		sql_statement::add_statement($1);
	}
	| truncate_stmt
	{
		($1)->set_stmt_type(STMTTYPE_TRUNCATE);
		($1)->set_root();
		sql_statement::add_statement($1);
	}
	;

select_stmt:
	SELECT comments_stmt distinct_list select_list
	FROM table_list
	where_clause
	group_clause
	order_clause
	having_clause
	lock_clause
	{
		sql_select *item = new sql_select();
		item->set_comments($2);
		item->set_distinct_flag($3 == 1);
		item->set_select_items($4);
		item->set_table_items($6);
		item->set_where_items($7);
		item->set_group_items($8);
		item->set_order_items($9);
		item->set_having_items($10);
		item->set_lock_flag($11);
		$$ = item;
		delete $2;
	}
	;

distinct_list:
	DISTINCT
	{
		$$ = 1;
	}
	|
	{
		$$ = 0;
	}
	;

select_list: // arg_list_t
	select_item
	{
		arg_list_t *item = new arg_list_t();
		item->push_back($1);
		$$ = item;
	}
	| select_list COMMA select_item
	{
		($1)->push_back($3);
		$$ = $1;
	}
	;

select_item: // composite_item_t
	select_name type_def
	{
		($1)->field_desc = $2;
		$$ = $1;
	}
	| select_name IDENT type_def
	{
		($3)->alias = $2;
		($1)->field_desc = $3;
		$$ = $1;
		delete []$2;
	}
	;

simple_item: // simple_item_t
	IDENT
	{
		$$ = set_simple(WORDTYPE_IDENT, $1);
		delete []$1;
	}
	| IDENT DOT IDENT
	{
		$$ = set_simple(WORDTYPE_IDENT, $1, $3);
		delete []$1;
		delete []$3;
	}
	| CONSTANT
	{
		char buf[32];
		sprintf(buf, "%ld", $1);
		$$ = set_simple(WORDTYPE_CONSTANT, buf);
	}
	| DCONSTANT
	{
		char buf[32];
		sprintf(buf, "%f", $1);
		$$ = set_simple(WORDTYPE_DCONSTANT, buf);
	}
	| STRING
	{
		$$ = set_simple(WORDTYPE_STRING, $1);
		delete []$1;
	}
	| NULL_OP
	{
		$$ = set_simple(WORDTYPE_NULL, "");
	}
	;

select_name: // composite_item_t
	simple_item
	{
		composite_item_t *item = new composite_item_t();
		item->simple_item = $1;
		$$ = item;
	}
	| select_func_name
	{
		composite_item_t *item = new composite_item_t();
		item->func_item = $1;
		$$ = item;
	}
	| select_name ADD select_name
	{
		$$ = set_relation(OPTYPE_ADD, $1, $3);
	}
	| select_name SUB select_name
	{
		$$ = set_relation(OPTYPE_SUB, $1, $3);
	}
	| select_name STRCAT select_name
	{
		$$ = set_relation(OPTYPE_STRCAT, $1, $3);
	}
	| select_name MUL select_name
	{
		$$ = set_relation(OPTYPE_MUL, $1, $3);
	}
	| select_name DIV select_name
	{
		$$ = set_relation(OPTYPE_DIV, $1, $3);
	}
	| select_name MOD select_name
	{
		$$ = set_relation(OPTYPE_MOD, $1, $3);
	}
	| SUB select_name %prec UMINUS
	{
		$$ = set_relation(OPTYPE_UMINUS, NULL, $2);
	}
	| LCURVE select_name RCURVE
	{
		$$ = $2;
		($$)->has_curve = true;
	}
	;

type_def: // field_desc_t
	LBRACE TYPE_CHAR LBRACKET CONSTANT RBRACKET RBRACE
	{
		field_desc_t *item = new field_desc_t();
		item->type = SQLTYPE_STRING;
		item->len = $4;
		$$ = item;
	}
	| LBRACE TYPE_VCHAR LBRACKET CONSTANT RBRACKET RBRACE
	{
		field_desc_t *item = new field_desc_t();
		item->type = SQLTYPE_VSTRING;
		item->len = $4;
		$$ = item;
	}
	| LBRACE ITYPE RBRACE
	{
		field_desc_t *item = new field_desc_t();
		item->type = static_cast<sqltype_enum>($2);
		$$ = item;
	}
	|
	{
		field_desc_t *item = new field_desc_t();
		item->type = SQLTYPE_ANY;
		$$ = item;
	}
	;

table_name: // composite_item_t
	IDENT
	{
		simple_item_t *item = set_simple(WORDTYPE_IDENT, $1);
		composite_item_t *composite_item = new composite_item_t();
		composite_item->simple_item = item;
		$$ = composite_item;
		delete []$1;
	}
	| IDENT IDENT
	{
		simple_item_t *item = set_simple(WORDTYPE_IDENT, $1, $2);
		composite_item_t *composite_item = new composite_item_t();
		composite_item->simple_item = item;
		$$ = composite_item;
		delete []$1;
		delete []$2;
	}
	;

table_list: // arg_list_t
	table_name
	{
		arg_list_t *item = new arg_list_t();
		item->push_back($1);
		$$ = item;
	}
	| table_list COMMA table_name
	{
		($1)->push_back($3);
		$$ = $1;
	}
	;

where_clause: // relation_item_t
	WHERE where_list
	{
		$$ = $2;
	}
	|
	{
		$$ = NULL;
	}
	;

where_list: // relation_item_t
	where_item
	{
		$$ = $1;
	}
	| where_list OR_OP where_list
	{
		$$ = set_relation(OPTYPE_OR, $1, $3);
	}
	| where_list AND_OP where_list
	{
		$$ = set_relation(OPTYPE_AND, $1, $3);
	}
	| NOT where_list
	{
		$$ = set_relation(OPTYPE_NOT, NULL, $2);
	}
	;

where_item: // relation_item_t
	composite_item COMP_OP composite_item
	{
		$$ = new relation_item_t(static_cast<optype_enum>($2), $1, $3);
	}
	| composite_item IN LCURVE arg_list RCURVE
	{
		composite_item_t *item = new composite_item_t();
		item->arg_item = $4;
		item->has_curve = true;
		$$ = new relation_item_t(OPTYPE_IN, $1, item);
	}
	| composite_item NOT IN LCURVE arg_list RCURVE
	{
		composite_item_t *item = new composite_item_t();
		item->arg_item = $5;
		item->has_curve = true;
		$$ = new relation_item_t(OPTYPE_NOT_IN, $1, item);
	}
	| composite_item BETWEEN composite_item AND_OP composite_item
	{
		composite_item_t *item = new composite_item_t();
		item->rela_item = new relation_item_t(OPTYPE_AND, $3, $5);
		$$ = new relation_item_t(OPTYPE_BETWEEN, $1, item);
	}
	| EXISTS LCURVE select_stmt RCURVE
	{
		composite_item_t *item = new composite_item_t();
		item->select_item = dynamic_cast<sql_select *>($3);
		item->has_curve = true;
		$$ = new relation_item_t(OPTYPE_EXISTS, NULL, item);
	}
	| composite_item IN LCURVE select_stmt RCURVE
	{
		composite_item_t *item = new composite_item_t();
		item->select_item = dynamic_cast<sql_select *>($4);
		item->has_curve = true;
		$$ = new relation_item_t(OPTYPE_IN, $1, item);
	}
	| composite_item NOT IN LCURVE select_stmt RCURVE
	{
		composite_item_t *item = new composite_item_t();
		item->select_item = dynamic_cast<sql_select *>($5);
		item->has_curve = true;
		$$ = new relation_item_t(OPTYPE_NOT_IN, $1, item);
	}
	| LCURVE where_list RCURVE
	{
		$$ = $2;
		($$)->has_curve = true;
	}
	| composite_item IS_OP NULL_OP
	{
		$$ = new relation_item_t(OPTYPE_IS_NULL, $1, NULL);
	}
	| composite_item IS_OP NOT NULL_OP
	{
		$$ = new relation_item_t(OPTYPE_IS_NOT_NULL, $1, NULL);
	}
	;

composite_item: // composite_item_t
	simple_item
	{
		composite_item_t *item = new composite_item_t();
		item->simple_item = $1;
		$$ = item;
	}
	| func_name
	{
		composite_item_t *item = new composite_item_t();
		item->func_item = $1;
		$$ = item;
	}
	| COLON IDENT type_def
	{
		composite_item_t *item = new composite_item_t();
		($3)->alias = $2;
		item->field_desc = $3;
		$$ = item;
		delete []$2;
	}
	| composite_item ADD composite_item
	{
		$$ = set_relation(OPTYPE_ADD, $1, $3);
	}
	| composite_item SUB composite_item
	{
		$$ = set_relation(OPTYPE_SUB, $1, $3);
	}
	| composite_item STRCAT composite_item
	{
		$$ = set_relation(OPTYPE_STRCAT, $1, $3);
	}
	| composite_item MUL composite_item
	{
		$$ = set_relation(OPTYPE_MUL, $1, $3);
	}
	| composite_item DIV composite_item
	{
		$$ = set_relation(OPTYPE_DIV, $1, $3);
	}
	| composite_item MOD composite_item
	{
		$$ = set_relation(OPTYPE_MOD, $1, $3);
	}
	| SUB composite_item %prec UMINUS
	{
		$$ = set_relation(OPTYPE_UMINUS, NULL, $2);
	}
	| LCURVE composite_item RCURVE
	{
		$$ = $2;
		($$)->has_curve = true;
	}
	| LCURVE select_stmt RCURVE
	{
		composite_item_t *item = new composite_item_t();
		item->select_item = dynamic_cast<sql_select *>($2);
		item->has_curve = true;
		$$ = item;
	}
	;

group_clause: // arg_list_t
	GROUP BY group_list
	{
		$$ = $3;
	}
	|
	{
		$$ = NULL;
	}
	;

group_list: // arg_list_t
	select_name
	{
		arg_list_t *item = new arg_list_t();
		item->push_back($1);
		$$ = item;
	}
	| group_list COMMA select_name
	{
		($1)->push_back($3);
		$$ = $1;
	}
	;

order_clause: // arg_list_t
	ORDER BY order_list
	{
		$$ = $3;
	}
	|
	{
		$$ = NULL;
	}
	;

order_list: // arg_list_t
	order_item
	{
		arg_list_t *item = new arg_list_t();
		item->push_back($1);
		$$ = item;
	}
	| order_list COMMA order_item
	{
		($1)->push_back($3);
		$$ = $1;
	}
	;

order_item: // composite_item_t
	select_name
	{
		$$ = $1;
	}
	| select_name ASC
	{
		$$ = $1;
		($$)->order_type = ORDERTYPE_ASC;
	}
	| select_name DESC
	{
		$$ = $1;
		($$)->order_type = ORDERTYPE_DESC;
	}
	;

having_clause: // relation_item_t
	HAVING having_list
	{
		$$ = $2;
	}
	|
	{
		$$ = NULL;
	}
	;

having_list: // relation_item_t
	having_item
	{
		$$ = $1;
	}
	| having_list OR_OP having_list
	{
		$$ = set_relation(OPTYPE_AND, $1, $3);
	}
	| having_list AND_OP having_list
	{
		$$ = set_relation(OPTYPE_AND, $1, $3);
	}
	| NOT having_list
	{
		$$ = set_relation(OPTYPE_NOT, NULL, $2);
	}
	| LCURVE having_list RCURVE
	{
		$$ = $2;
		($$)->has_curve = true;
	}
	;

having_item: // relation_item_t
	composite_item COMP_OP composite_item
	{
		$$ = new relation_item_t(static_cast<optype_enum>($2), $1, $3);
	}
	;

select_func_name: // sql_func_t
	IDENT LCURVE select_arg_list RCURVE
	{
		sql_func_t *func_node = new sql_func_t();
		func_node->func_name = "";
		func_node->func_name += $1;
		func_node->args = $3;
		$$ = func_node;
		delete []$1;
	}
	;

select_arg_list: // arg_list_t
	select_name
	{
		arg_list_t *item = new arg_list_t();
		item->push_back($1);
		$$ = item;
	}
	| select_arg_list COMMA select_name
	{
		($1)->push_back($3);
		$$ = $1;
	}
	;

func_name: // sql_func_t
	IDENT LCURVE arg_list RCURVE
	{
		sql_func_t *func_node = new sql_func_t();
		func_node->func_name = "";
		func_node->func_name += $1;
		func_node->args = $3;
		$$ = func_node;
		delete []$1;
	}
	;

arg_list: // arg_list_t
	composite_item
	{
		arg_list_t *item = new arg_list_t();
		item->push_back($1);
		$$ = item;
	}
	| arg_list COMMA composite_item
	{
		($1)->push_back($3);
		$$ = $1;
	}
	|
	{
		$$ = NULL;
	}
	;

COMP_OP:
	GT
	{
		$$ = OPTYPE_GT;
	}
	| LT
	{
		$$ = OPTYPE_LT;
	}
	| GE
	{
		$$ = OPTYPE_GE;
	}
	| LE
	{
		$$ = OPTYPE_LE;
	}
	| EQ
	{
		$$ = OPTYPE_EQ;
	}
	| NEQ
	{
		$$ = OPTYPE_NEQ;
	}
	;

ITYPE: // long
	TYPE_CHAR
	{
		$$ = SQLTYPE_CHAR;
	}
	| TYPE_UCHAR
	{
		$$ = SQLTYPE_UCHAR;
	}
	| TYPE_SHORT
	{
		$$ = SQLTYPE_SHORT;
	}
	| TYPE_USHORT
	{
		$$ = SQLTYPE_USHORT;
	}
	| TYPE_INT
	{
		$$ = SQLTYPE_INT;
	}
	| TYPE_UINT
	{
		$$ = SQLTYPE_UINT;
	}
	| TYPE_LONG
	{
		$$ = SQLTYPE_LONG;
	}
	| TYPE_ULONG
	{
		$$ = SQLTYPE_ULONG;
	}
	| TYPE_FLOAT
	{
		$$ = SQLTYPE_FLOAT;
	}
	| TYPE_DOUBLE
	{
		$$ = SQLTYPE_DOUBLE;
	}
	| TYPE_DATE
	{
		$$ = SQLTYPE_DATE;
	}
	| TYPE_TIME
	{
		$$ = SQLTYPE_TIME;
	}
	| TYPE_DATETIME
	{
		$$ = SQLTYPE_DATETIME;
	}
	;

insert_stmt:
	INSERT comments_stmt INTO table_name LCURVE ident_list RCURVE VALUES LCURVE insert_list RCURVE
	{
		sql_insert *item = new sql_insert();
		item->set_comments($2);
		item->set_table_items($4);
		item->set_ident_items($6);
		item->set_insert_items($10);
		$$ = item;
		delete $2;
	}
	| INSERT comments_stmt INTO table_name VALUES LCURVE insert_list RCURVE
	{
		sql_insert *item = new sql_insert();
		item->set_comments($2);
		item->set_table_items($4);
		item->set_insert_items($7);
		$$ = item;
		delete $2;
	}
	| INSERT comments_stmt INTO table_name LCURVE ident_list RCURVE select_stmt
	{
		sql_insert *item = new sql_insert();
		item->set_comments($2);
		item->set_table_items($4);
		item->set_ident_items($6);
		item->set_select_items(dynamic_cast<sql_select *>($8));
		$$ = item;
		delete $2;
	}
	| INSERT comments_stmt INTO table_name select_stmt
	{
		sql_insert *item = new sql_insert();
		item->set_comments($2);
		item->set_table_items($4);
		item->set_select_items(dynamic_cast<sql_select *>($5));
		$$ = item;
		delete $2;
	}
	;

ident_list: // arg_list_t
	IDENT
	{
		arg_list_t *item = new arg_list_t();
		simple_item_t *simple_item = set_simple(WORDTYPE_IDENT, $1);
		composite_item_t *composite_item = new composite_item_t();
		composite_item->simple_item = simple_item;
		item->push_back(composite_item);
		$$ = item;
		delete []$1;
	}
	| ident_list COMMA IDENT
	{
		simple_item_t *simple_item = set_simple(WORDTYPE_IDENT, $3);
		composite_item_t *composite_item = new composite_item_t();
		composite_item->simple_item = simple_item;
		($1)->push_back(composite_item);
		$$ = $1;
		delete []$3;
	}
	;

insert_list: // arg_list_t
	composite_item
	{
		arg_list_t *item = new arg_list_t();
		item->push_back($1);
		$$ = item;
	}
	| insert_list COMMA composite_item
	{
		($1)->push_back($3);
		$$ = $1;
	}

lock_clause:
	FOR UPDATE
	{
		$$ = 1;
	}
	|
	{
		$$ = 0;
	}
	;

update_stmt:
	UPDATE comments_stmt table_name SET update_list where_clause
	{
		sql_update *item = new sql_update();
		item->set_comments($2);
		item->set_table_items($3);
		item->set_update_items($5);
		item->set_where_items($6);
		$$ = item;
		delete $2;
	}
	;

update_list: // arg_list_t
	update_item
	{
		arg_list_t *item = new arg_list_t();
		item->push_back($1);
		$$ = item;
	}
	| update_list COMMA update_item
	{
		($1)->push_back($3);
		$$ = $1;
	}
	;

update_item: // composite_item_t
	IDENT EQ composite_item
	{
		composite_item_t *item = new composite_item_t();
		item->simple_item = set_simple(WORDTYPE_IDENT, $1);
		$$ = set_relation(OPTYPE_EQ, item, $3);
		delete []$1;
	}
	;

delete_stmt:
	DELETE comments_stmt FROM table_name where_clause
	{
		sql_delete *item = new sql_delete();
		item->set_comments($2);
		item->set_table_items($4);
		item->set_where_items($5);
		$$ = item;
		delete $2;

	}
	| DELETE comments_stmt table_name where_clause
	{
		sql_delete *item = new sql_delete();
		item->set_comments($2);
		item->set_table_items($3);
		item->set_where_items($4);
		$$ = item;
		delete $2;
	}
	;

comments_stmt:
	COMMENTS
	{
		$$ = $1;
	}
	|
	{
		$$ = NULL;
	}
	;

create_stmt : // sql_statement
	CREATE comments_stmt TABLE table_name LCURVE create_list RCURVE
	{
		sql_create *item = new sql_create();
		item->set_comments($2);
		item->set_table_items($4);
		item->set_field_items($6);
		$$ = item;
		delete $2;
	}
	;

create_list: // arg_list_t
	create_item
	{
		arg_list_t *item = new arg_list_t();
		item->push_back($1);
		$$ = item;
	}
	| create_list COMMA create_item
	{
		($1)->push_back($3);
		$$ = $1;
	}
	;

create_item: // composite_item_t
	IDENT create_type_def
	{
		composite_item_t *item = new composite_item_t();
		item->simple_item = set_simple(WORDTYPE_IDENT, $1);
		item->field_desc = $2;
		$$ = item;
	}
	;

create_type_def: // field_desc_t
	TYPE_CHAR LCURVE CONSTANT RCURVE
	{
		field_desc_t *item = new field_desc_t();
		item->type = SQLTYPE_STRING;
		item->len = $3;
		$$ = item;
	}
	| TYPE_VCHAR LCURVE CONSTANT RCURVE
	{
		field_desc_t *item = new field_desc_t();
		item->type = SQLTYPE_VSTRING;
		item->len = $3;
		$$ = item;
	}
	| ITYPE
	{
		field_desc_t *item = new field_desc_t();
		item->type = static_cast<sqltype_enum>($1);
		$$ = item;
	}
	;

drop_stmt : // sql_statement
	DROP TABLE table_name
	{
		sql_drop *item = new sql_drop();
		item->set_table_items($3);
		$$ = item;
	}
	;

truncate_stmt : // sql_statement
	TRUNCATE TABLE table_name
	{
		sql_truncate *item = new sql_truncate();
		item->set_table_items($3);
		$$ = item;
	}
	;

%%

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

