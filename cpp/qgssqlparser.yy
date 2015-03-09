/***************************************************************************
                         qgssqlparser.yy
                          --------------------
    begin                : August 2011
    copyright            : (C) 2011 by Martin Dobias
    email                : wonder.sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

%{
#include <qglobal.h>
#include <QList>
#include <cstdlib>
#include "qgsexpression.h"
#include "qgssql.h"

#ifdef _MSC_VER
#  pragma warning( disable: 4065 )  // switch statement contains 'default' but no 'case' labels
#  pragma warning( disable: 4702 )  // unreachable code
#endif

// don't redeclare malloc/free
#define YYINCLUDED_STDLIB_H 1

struct expression_parser_context;
#include "qgssqlparser.hpp"

//! from lexer
typedef void* yyscan_t;
typedef struct yy_buffer_state* YY_BUFFER_STATE;
extern int exp_lex_init(yyscan_t* scanner);
extern int exp_lex_init_extra (void* user_defined, yyscan_t* scanner);
extern int exp_lex_destroy(yyscan_t scanner);
 extern int exp_lex(YYSTYPE* yylval_param, yyscan_t yyscanner);
extern YY_BUFFER_STATE exp__scan_string(const char* buffer, yyscan_t scanner);

/** returns parsed tree, otherwise returns NULL and sets parserErrorMsg
    (interface function to be called from QgsExpression)
  */
QgsExpression::Node* parseExpression(const QString& str, QString& parserErrorMsg);

/** error handler for bison */
 void exp_error(expression_parser_context* parser_ctx, const char* msg);

struct expression_parser_context
{
  // lexer context
  yyscan_t flex_scanner;

  // varible where the parser error will be stored
  QString errorMsg;
  // root node of the expression
  QgsExpression::Node* rootNode;
  QgsSql::Node* sqlNode;
  bool isSql;
};

#define scanner parser_ctx->flex_scanner

// we want verbose error messages
#define YYERROR_VERBOSE 1

#define BINOP(x, y, z)  new QgsExpression::NodeBinaryOperator(x, y, z)

%}

// make the parser reentrant
%define api.pure
%lex-param {void * scanner}
%parse-param {expression_parser_context* parser_ctx}

%name-prefix "exp_"

%union
{
  QgsExpression::Node* node;
  QgsExpression::NodeList* nodelist;
  double numberFloat;
  int    numberInt;
  QString* text;
  QgsExpression::BinaryOperator b_op;
  QgsExpression::UnaryOperator u_op;
  QgsExpression::WhenThen* whenthen;
  QgsExpression::WhenThenList* whenthenlist;
    QgsSql::Node* sqlnode;
    QgsSql::List* sqlnodelist;
    bool boolean;
}

%start root


//
// token definitions
//

// operator tokens
%token <b_op> OR AND EQ NE LE GE LT GT REGEXP LIKE IS PLUS MINUS MUL DIV INTDIV MOD CONCAT POW
%token <u_op> NOT
%token IN

// literals
%token <numberFloat> NUMBER_FLOAT
%token <numberInt> NUMBER_INT
%token NULLVALUE

// tokens for conditional expressions
%token CASE WHEN THEN ELSE END

// keyword tokens
%token SELECT AS FROM WHERE ALL DISTINCT INNER OUTER CROSS JOIN NATURAL LEFT USING ON

%token <text> STRING COLUMN_REF FUNCTION SPECIAL_COL

%token COMMA

%token Unknown_CHARACTER

// pseudo tokens
%token START_EXPR START_SQL

//
// definition of non-terminal types
//

%type <node> expression
%type <nodelist> exp_list
%type <whenthen> when_then_clause
%type <whenthenlist> when_then_clauses

%type <sqlnode> select_stmt result_column optional_from table_or_subquery join_operator non_natural_join_operator join_constraint augmented_expression optional_where
%type <sqlnodelist> result_column_list table_or_subquery_list
%type <boolean> is_distinct is_natural

// debugging
%error-verbose

//
// operator precedence
//

// left associativity means that 1+2+3 translates to (1+2)+3
// the order of operators here determines their precedence

%left OR
%left AND
%right NOT
%left EQ NE LE GE LT GT REGEXP LIKE IS IN
%left PLUS MINUS
%left MUL DIV INTDIV MOD
%right POW
%left CONCAT

%right UMINUS  // fictitious symbol (for unary minus)

%left COMMA

%destructor { delete $$; } <node>
%destructor { delete $$; } <nodelist>
%destructor { delete $$; } <text>

%%

root : expr_root | sql_root;

expr_root: START_EXPR expression { parser_ctx->rootNode = $2; parser_ctx->isSql = false; }
                ;

sql_root: START_SQL select_stmt { parser_ctx->sqlNode = $2; parser_ctx->isSql = true; }
                ;

select_stmt:    SELECT is_distinct result_column_list optional_from optional_where { $$ = new QgsSql::Select( $3, $4, static_cast<QgsSql::Expression*>($5), $2 ); }
        ;

is_distinct:
                /*empty*/ { $$ = false; }
        |       DISTINCT { $$ = true; }
        |       ALL { $$ = false; }
        ;

optional_from:  /*empty*/ { $$ = 0; }
        |       FROM table_or_subquery_list { $$ = $2; }
        ;

optional_where:  /*empty*/ { $$ = 0; }
        |       WHERE augmented_expression { $$ = $2; }
        ;

is_natural:
                /*empty*/ { $$ = false; }
        |       NATURAL { $$ = true; }              
        ;

join_operator:  is_natural non_natural_join_operator { $$ = $2; static_cast<QgsSql::JoinedTable*>($$)->set_is_natural($1); }
        ;

non_natural_join_operator:
                LEFT JOIN { $$ = new QgsSql::JoinedTable(QgsSql::JoinedTable::JOIN_LEFT); }
        |       LEFT OUTER JOIN { $$ = new QgsSql::JoinedTable(QgsSql::JoinedTable::JOIN_LEFT); }
        |       INNER JOIN { $$ = new QgsSql::JoinedTable(QgsSql::JoinedTable::JOIN_INNER); }
        |       CROSS JOIN { $$ = new QgsSql::JoinedTable(QgsSql::JoinedTable::JOIN_CROSS); }
        |       JOIN { $$ = new QgsSql::JoinedTable(QgsSql::JoinedTable::JOIN_INNER); }
        ;

join_constraint:
                /*empty*/ { $$ = 0; }
        |       ON augmented_expression { $$ = $2; }
        |       USING '(' result_column_list ')' { $$ = $3; }
        ;

result_column_list:
                result_column { $$ = new QgsSql::List(); $$->append( $1 ); }
        |       result_column_list COMMA result_column { $$ = $1; $$->append($3); }
        ;

result_column:
                MUL { $$ = new QgsSql::TableColumn( "*" ); }
        |       COLUMN_REF '.' MUL { $$ = new QgsSql::TableColumn( "*", *$1 ); delete $1; }
        |       augmented_expression { $$ = new QgsSql::ExpressionColumn( static_cast<QgsSql::Expression*>($1) ); }
        |       augmented_expression AS COLUMN_REF { $$ = new QgsSql::ExpressionColumn( static_cast<QgsSql::Expression*>($1), *$3 ); delete $3; }
        ;

table_or_subquery_list:
                table_or_subquery { $$ = new QgsSql::List(); $$->append($1); }
        |       table_or_subquery_list COMMA table_or_subquery
        {
            $$ = $1;
            QgsSql::JoinedTable* jt = new QgsSql::JoinedTable(QgsSql::JoinedTable::JOIN_CROSS);
            jt->set_right_table($3);
            $$->append(jt);
        }
        |       table_or_subquery_list join_operator table_or_subquery join_constraint
        {
            $$ = $1;
            QgsSql::JoinedTable* jt = static_cast<QgsSql::JoinedTable*>($2);
            jt->set_right_table($3);
            if ($4) {
                if ($4->type() == QgsSql::Node::NODE_LIST) {
                    jt->set_using_columns(static_cast<QgsSql::List*>($4));
                }
                else {
                    jt->set_on_expression(static_cast<QgsSql::Expression*>($4));
                }
            }
            $$->append(jt);
        }
        ;

table_or_subquery:
                COLUMN_REF { $$ = new QgsSql::TableName( *$1 ); delete $1; }
        |       COLUMN_REF COLUMN_REF { $$ = new QgsSql::TableName( *$1, *$2 ); delete $1; delete $2;}
        |       COLUMN_REF AS COLUMN_REF { $$ = new QgsSql::TableName( *$1, *$3 ); delete $1; delete $3;}
        |       '('select_stmt ')' { $$ = new QgsSql::TableSelect( $2 ); }
        |       '('select_stmt ')' COLUMN_REF { $$ = new QgsSql::TableSelect( $2, *$4 ); delete $4; }
        |       '('select_stmt ')' AS COLUMN_REF { $$ = new QgsSql::TableSelect( $2, *$5 ); delete $5; }
        ;

expression:
      expression AND expression       { $$ = BINOP($2, $1, $3); }
    | expression OR expression        { $$ = BINOP($2, $1, $3); }
    | expression EQ expression        { $$ = BINOP($2, $1, $3); }
    | expression NE expression        { $$ = BINOP($2, $1, $3); }
    | expression LE expression        { $$ = BINOP($2, $1, $3); }
    | expression GE expression        { $$ = BINOP($2, $1, $3); }
    | expression LT expression        { $$ = BINOP($2, $1, $3); }
    | expression GT expression        { $$ = BINOP($2, $1, $3); }
    | expression REGEXP expression    { $$ = BINOP($2, $1, $3); }
    | expression LIKE expression      { $$ = BINOP($2, $1, $3); }
    | expression IS expression        { $$ = BINOP($2, $1, $3); }
    | expression PLUS expression      { $$ = BINOP($2, $1, $3); }
    | expression MINUS expression     { $$ = BINOP($2, $1, $3); }
    | expression MUL expression       { $$ = BINOP($2, $1, $3); }
    | expression INTDIV expression    { $$ = BINOP($2, $1, $3); }
    | expression DIV expression       { $$ = BINOP($2, $1, $3); }
    | expression MOD expression       { $$ = BINOP($2, $1, $3); }
    | expression POW expression       { $$ = BINOP($2, $1, $3); }
    | expression CONCAT expression    { $$ = BINOP($2, $1, $3); }
    | NOT expression                  { $$ = new QgsExpression::NodeUnaryOperator($1, $2); }
    | '(' expression ')'              { $$ = $2; }

    | FUNCTION '(' exp_list ')'
        {
          int fnIndex = QgsExpression::functionIndex(*$1);
          if (fnIndex == -1)
          {
            // this should not actually happen because already in lexer we check whether an identifier is a known function
            // (if the name is not known the token is parsed as a column)
              exp_error(parser_ctx, "Function is not known");
            YYERROR;
          }
          if ( QgsExpression::Functions()[fnIndex]->params() != -1
               && QgsExpression::Functions()[fnIndex]->params() != $3->count() )
          {
              exp_error(parser_ctx, "Function is called with wrong number of arguments");
            YYERROR;
          }
          $$ = new QgsExpression::NodeFunction(fnIndex, $3);
          delete $1;
        }

    | expression IN '(' exp_list ')'     { $$ = new QgsExpression::NodeInOperator($1, $4, false);  }
    | expression NOT IN '(' exp_list ')' { $$ = new QgsExpression::NodeInOperator($1, $5, true); }

    | PLUS expression %prec UMINUS { $$ = $2; }
    | MINUS expression %prec UMINUS { $$ = new QgsExpression::NodeUnaryOperator( QgsExpression::uoMinus, $2); }

    | CASE when_then_clauses END      { $$ = new QgsExpression::NodeCondition($2); }
    | CASE when_then_clauses ELSE expression END  { $$ = new QgsExpression::NodeCondition($2,$4); }

    // columns
    | COLUMN_REF                  { $$ = new QgsExpression::NodeColumnRef( *$1 ); delete $1; }
// column name with a table name
        |  COLUMN_REF '.' COLUMN_REF { $$ = new QgsExpression::NodeColumnRef( *$3 ); delete $1; delete $3; }

    // special columns (actually functions with no arguments)
    | SPECIAL_COL
        {
          int fnIndex = QgsExpression::functionIndex(*$1);
          if (fnIndex == -1)
          {
            if ( !QgsExpression::hasSpecialColumn( *$1 ) )
	    {
                exp_error(parser_ctx, "Special column is not known");
	      YYERROR;
	    }
	    // $var is equivalent to _specialcol_( "$var" )
	    QgsExpression::NodeList* args = new QgsExpression::NodeList();
	    QgsExpression::NodeLiteral* literal = new QgsExpression::NodeLiteral( *$1 );
	    args->append( literal );
	    $$ = new QgsExpression::NodeFunction( QgsExpression::functionIndex( "_specialcol_" ), args );
          }
	  else
	  {
	    $$ = new QgsExpression::NodeFunction( fnIndex, NULL );
	    delete $1;
	  }
        }

    //  literals
    | NUMBER_FLOAT                { $$ = new QgsExpression::NodeLiteral( QVariant($1) ); }
    | NUMBER_INT                  { $$ = new QgsExpression::NodeLiteral( QVariant($1) ); }
    | STRING                      { $$ = new QgsExpression::NodeLiteral( QVariant(*$1) ); delete $1; }
    | NULLVALUE                   { $$ = new QgsExpression::NodeLiteral( QVariant() ); }
;

exp_list:
      exp_list COMMA expression { $$ = $1; $1->append($3); }
    | expression              { $$ = new QgsExpression::NodeList(); $$->append($1); }
    ;

when_then_clauses:
      when_then_clauses when_then_clause  { $$ = $1; $1->append($2); }
    | when_then_clause                    { $$ = new QgsExpression::WhenThenList(); $$->append($1); }
    ;

when_then_clause:
      WHEN expression THEN expression     { $$ = new QgsExpression::WhenThen($2,$4); }
    ;

augmented_expression:
                expression { $$ = new QgsSql::Expression( $1 ); }
        |       augmented_expression IN '(' select_stmt ')' { $$ = new QgsSql::ExpressionIn( static_cast<QgsSql::Expression*>($1), true, $4 ); }
        |       augmented_expression NOT IN '(' select_stmt ')' { $$ = new QgsSql::ExpressionIn( static_cast<QgsSql::Expression*>($1), false, $5 ); }
        |       augmented_expression IN COLUMN_REF { $$ = new QgsSql::ExpressionIn( static_cast<QgsSql::Expression*>($1), true, new QgsSql::TableName(*$3) ); delete $3; }
        |       augmented_expression NOT IN COLUMN_REF { $$ = new QgsSql::ExpressionIn( static_cast<QgsSql::Expression*>($1), false, new QgsSql::TableName(*$4) ); delete $4; }
        ;

%%


// returns parsed tree, otherwise returns NULL and sets parserErrorMsg
QgsExpression::Node* parseExpression(const QString& str, QString& parserErrorMsg)
{
  expression_parser_context ctx;
  ctx.rootNode = 0;

  int mode = 1; // expression parsing
  exp_lex_init_extra(&mode, &ctx.flex_scanner);
  exp__scan_string(str.toUtf8().constData(), ctx.flex_scanner);
  int res = exp_parse(&ctx);
  exp_lex_destroy(ctx.flex_scanner);

  // list should be empty when parsing was OK
  if (res == 0) // success?
  {
    return ctx.rootNode;
  }
  else // error?
  {
    parserErrorMsg = ctx.errorMsg;
    return NULL;
  }
}


QgsSql::Node* parseSql(const QString& str, QString& parserErrorMsg)
{
  expression_parser_context ctx;
  ctx.rootNode = 0;

  int mode = 2; // SQL parsing
  exp_lex_init_extra(&mode, &ctx.flex_scanner);
  exp__scan_string(str.toUtf8().constData(), ctx.flex_scanner);
  int res = exp_parse(&ctx);
  exp_lex_destroy(ctx.flex_scanner);

  // list should be empty when parsing was OK
  if (res == 0) // success?
  {
    return ctx.sqlNode;
  }
  else // error?
  {
    parserErrorMsg = ctx.errorMsg;
    return NULL;
  }
}

void exp_error(expression_parser_context* parser_ctx, const char* msg)
{
  parser_ctx->errorMsg = msg;
}

