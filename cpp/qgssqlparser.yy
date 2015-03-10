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

/** error handler for bison */
 void exp_error(expression_parser_context* parser_ctx, const char* msg);

struct expression_parser_context
{
  // lexer context
  yyscan_t flex_scanner;

  // varible where the parser error will be stored
  QString errorMsg;
  // root node of the expression
  QgsSql::Node* sqlNode;
};

#define scanner parser_ctx->flex_scanner

// we want verbose error messages
#define YYERROR_VERBOSE 1

#define BINOP(x, y, z)  new QgsSql::ExpressionBinaryOperator(x, y, z)

%}

// make the parser reentrant
%define api.pure
%lex-param {void * scanner}
%parse-param {expression_parser_context* parser_ctx}

%name-prefix "exp_"

%union
{
  double numberFloat;
  int    numberInt;
  QString* text;
  QgsSql::Node* sqlnode;
  QgsSql::List* sqlnodelist;
  QgsSql::Expression* expression;
  bool boolean;
  QgsExpression::BinaryOperator b_op;
  QgsExpression::UnaryOperator u_op;
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
%token SELECT AS FROM WHERE ALL DISTINCT INNER OUTER CROSS JOIN NATURAL LEFT USING ON UNION EXCEPT INTERSECT GROUP BY HAVING ASC DESC LIMIT OFFSET ORDER

%token <text> STRING COLUMN_REF FUNCTION SPECIAL_COL

%token COMMA

%token Unknown_CHARACTER

//
// definition of non-terminal types
//

%type   <expression>    expression
%type   <sqlnode>       when_then_clause select_stmt result_column optional_from table_or_subquery join_operator non_natural_join_operator join_constraint optional_where optional_group_by optional_having
%type   <sqlnode>       optional_order_by optional_limit ordering_term optional_offset select_core
%type   <sqlnodelist>   exp_list result_column_list table_or_subquery_list when_then_clauses compound_select ordering_terms
%type   <boolean>       is_distinct is_natural

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

%destructor { delete $$; } <text>

%%

root: select_stmt { parser_ctx->sqlNode = $1; }
                ;

select_stmt:    
                compound_select optional_order_by optional_limit { $$ = new QgsSql::SelectStmt( $1, static_cast<QgsSql::OrderBy*>($2), static_cast<QgsSql::LimitOffset*>($3) ); }
        ;

optional_order_by:
                /*empty*/ { $$ = 0; }
        |       ORDER BY ordering_terms { $$ = new QgsSql::OrderBy( $3 ); }
        ;

ordering_term:  
                expression ASC { $$ = new QgsSql::OrderingTerm( $1, true ); }
        |       expression DESC { $$ = new QgsSql::OrderingTerm( $1, false ); }
        ;

ordering_terms: 
                ordering_term { $$ = new QgsSql::List(); $$->append($1); }
        |       ordering_terms COMMA ordering_term { $$ = $1; $$->append($3); }
        ;

optional_limit: 
                LIMIT expression optional_offset
                {
                    if ($3) {
                        $$ = $3;
                        static_cast<QgsSql::LimitOffset*>($$)->set_limit( $2 );
                    }
                    else {
                        $$ = new QgsSql::LimitOffset( $2 );
                    }
                }
        ;

optional_offset:
                /*empty*/ { $$ = 0; }
        |       OFFSET expression { $$ = new QgsSql::LimitOffset( 0, $2 ); }
        |       COMMA expression { $$ = new QgsSql::LimitOffset( 0, $2 ); }
        ;

compound_select:
                select_core { $$ = new QgsSql::List(); $$->append($1); }
        |       compound_select UNION select_core { $$ = $1; $$->append( new QgsSql::CompoundSelect( static_cast<QgsSql::Select*>($3), QgsSql::CompoundSelect::UNION) ); }
        |       compound_select UNION ALL select_core { $$ = $1; $$->append( new QgsSql::CompoundSelect( static_cast<QgsSql::Select*>($4), QgsSql::CompoundSelect::UNION_ALL) ); }
        |       compound_select INTERSECT select_core { $$ = $1; $$->append( new QgsSql::CompoundSelect( static_cast<QgsSql::Select*>($3), QgsSql::CompoundSelect::INTERSECT) ); }
        |       compound_select EXCEPT select_core { $$ = $1; $$->append( new QgsSql::CompoundSelect( static_cast<QgsSql::Select*>($3), QgsSql::CompoundSelect::EXCEPT) ); }
        ;

select_core:    SELECT
                is_distinct
                result_column_list
                optional_from
                optional_where
                optional_group_by
                { $$ = new QgsSql::Select( $3, $4, static_cast<QgsSql::Expression*>($5), $2 ); }
        ;

is_distinct:
                /*empty*/ { $$ = false; }
        |       DISTINCT { $$ = true; }
        |       ALL { $$ = false; }
        ;

optional_group_by:
                /*empty*/ { $$ = 0; }
        |       GROUP BY exp_list optional_having { $$ = new QgsSql::GroupBy( $3, static_cast<QgsSql::Having*>($4) ); }
        ;

optional_having:
                /*empty*/ { $$ = 0; }
        |       HAVING expression { $$ = new QgsSql::Having( $2 ); }
        ;

optional_from:  /*empty*/ { $$ = 0; }
        |       FROM table_or_subquery_list { $$ = $2; }
        ;

optional_where:  /*empty*/ { $$ = 0; }
        |       WHERE expression { $$ = $2; }
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
        |       ON expression { $$ = $2; }
        |       USING '(' result_column_list ')' { $$ = $3; }
        ;

result_column_list:
                result_column { $$ = new QgsSql::List(); $$->append( $1 ); }
        |       result_column_list COMMA result_column { $$ = $1; $$->append($3); }
        ;

result_column:
                MUL { $$ = new QgsSql::TableColumn( "*" ); }
        |       COLUMN_REF '.' MUL { $$ = new QgsSql::TableColumn( "*", *$1 ); delete $1; }
        |       expression { $$ = new QgsSql::ExpressionColumn( static_cast<QgsSql::Expression*>($1) ); }
        |       expression AS COLUMN_REF { $$ = new QgsSql::ExpressionColumn( static_cast<QgsSql::Expression*>($1), *$3 ); delete $3; }
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
    | NOT expression                  { $$ = new QgsSql::ExpressionUnaryOperator($1, $2); }
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
          $$ = new QgsSql::ExpressionFunction(fnIndex, $3);
          delete $1;
        }

    | expression IN '(' exp_list ')'     { $$ = new QgsSql::ExpressionIn($1, $4, false);  }
    | expression NOT IN '(' exp_list ')' { $$ = new QgsSql::ExpressionIn($1, $5, true); }
        |       expression IN '(' select_stmt ')' { $$ = new QgsSql::ExpressionIn( $1, $4, false ); }
        |       expression NOT IN '(' select_stmt ')' { $$ = new QgsSql::ExpressionIn( $1, $5, true ); }
        |       expression IN COLUMN_REF { $$ = new QgsSql::ExpressionIn( $1, new QgsSql::TableColumn("", *$3), false ); delete $3; }
        |       expression NOT IN COLUMN_REF { $$ = new QgsSql::ExpressionIn( $1, new QgsSql::TableColumn("", *$4), true ); delete $4;}

    | PLUS expression %prec UMINUS { $$ = $2; }
    | MINUS expression %prec UMINUS { $$ = new QgsSql::ExpressionUnaryOperator( QgsExpression::uoMinus, $2); }

    | CASE when_then_clauses END      { $$ = new QgsSql::ExpressionCondition($2); }
    | CASE when_then_clauses ELSE expression END  { $$ = new QgsSql::ExpressionCondition($2,$4); }

    // columns
    | COLUMN_REF                  { $$ = new QgsSql::TableColumn( *$1 ); delete $1; }
// column name with a table name
        |       COLUMN_REF '.' COLUMN_REF { $$ = new QgsSql::TableColumn( *$3, *$1 ); delete $1; delete $3; }

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
	    QgsSql::List* args = new QgsSql::List();
	    QgsSql::ExpressionLiteral* literal = new QgsSql::ExpressionLiteral( *$1 );
            delete $1;
	    args->append( literal );
	    $$ = new QgsSql::ExpressionFunction( QgsExpression::functionIndex( "_specialcol_" ), args );
          }
	  else
	  {
	    $$ = new QgsSql::ExpressionFunction( fnIndex, NULL );
	    delete $1;
	  }
        }

    //  literals
    | NUMBER_FLOAT                { $$ = new QgsSql::ExpressionLiteral( QVariant($1) ); }
    | NUMBER_INT                  { $$ = new QgsSql::ExpressionLiteral( QVariant($1) ); }
    | STRING                      { $$ = new QgsSql::ExpressionLiteral( QVariant(*$1) ); delete $1; }
    | NULLVALUE                   { $$ = new QgsSql::ExpressionLiteral( QVariant() ); }
;

exp_list:
      exp_list COMMA expression { $$ = $1; $1->append($3); }
    | expression              { $$ = new QgsSql::List(); $$->append($1); }
    ;

when_then_clauses:
      when_then_clauses when_then_clause  { $$ = $1; $1->append($2); }
    | when_then_clause                    { $$ = new QgsSql::List(); $$->append($1); }
    ;

when_then_clause:
      WHEN expression THEN expression     { $$ = new QgsSql::ExpressionWhenThen($2,$4); }
    ;

%%


QgsSql::Node* parseSql(const QString& str, QString& parserErrorMsg)
{
  expression_parser_context ctx;
  ctx.sqlNode = 0;

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

