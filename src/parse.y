/*
** parse.y - streem parser
**
** See Copyright Notice in LICENSE file.
*/

%{
#define YYDEBUG 1
#define YYERROR_VERBOSE 1

#include "strm.h"
#include "node.h"

static void
node_lineinfo(parser_state* p, node* node)
{
  if (!node) return;
  node->fname = p->fname;
  node->lineno = p->lineno;
}

%}

%union {
  node* nd;
  node_string id;
}

%type <nd> program topstmts topstmt_list topstmt stmts stmt_list stmt
%type <nd> expr condition block primary opt_else
%type <nd> case_body cparam pattern pary pstruct pterm
%type <nd> bparam arg args opt_args opt_block f_args opt_f_args
%type <id> identifier var label fname
%type <nd> lit_string lit_symbol lit_number lit_time

%pure-parser
%parse-param {parser_state *p}
%lex-param {p}

%{
int yylex(YYSTYPE *lval, parser_state *p);
static void yyerror(parser_state *p, const char *s);
%}

%token
        keyword_if
        keyword_else
        keyword_case
        keyword_emit
        keyword_skip
        keyword_return
        keyword_namespace
        keyword_class
        keyword_import
        keyword_def
        keyword_method
        keyword_new
        keyword_nil
        keyword_true
        keyword_false
        op_lasgn
        op_rasgn
        op_lambda     /* -> */
        op_lambda2    /* )-> */
        op_lambda3    /* )->{*/
        op_plus
        op_minus
        op_mult
        op_div
        op_mod
        op_eq
        op_neq
        op_lt
        op_le
        op_gt
        op_ge
        op_and
        op_or
        op_bar
        op_amper
        op_colon2

%token
        lit_time
        lit_number
        lit_symbol
        lit_string
        identifier
        label

/*
 * precedence table
 */

%nonassoc op_LOWEST

%right op_lambda op_lambda2 op_lambda3
%right keyword_else
%right keyword_if
%left  op_bar
%left  op_amper
%left  op_or
%left  op_and
%nonassoc  op_eq op_neq
%left  op_lt op_le op_gt op_ge
%left  op_plus op_minus
%left  op_mult op_div op_mod
%right '!' '~'

%token op_HIGHEST

%%
program         : topstmts
                    {
                      p->lval = $1;
                    }
                ;

topstmts        : topstmt_list opt_terms
                    {
                      $$ = $1;
                    }
                | terms topstmt_list opt_terms
                    {
                      $$ = $2;
                    }
                | opt_terms
                    {
                      $$ = NULL;
                    }
                ;

topstmt_list    : topstmt
                    {
                      $$ = node_nodes_new();
                      node_lineinfo(p, $$);
                      if ($1) {
                        node_nodes_add($$, $1);
                        node_lineinfo(p, $1);
                      }
                    }
                | topstmt_list terms topstmt
                    {
                      $$ = $1;
                      if ($3) {
                        if ($1) {
                          node_nodes_add($$, $3);
                        }
                        else {
                          $1 = $3;
                        }
                        node_lineinfo(p, $3);
                      }
                    }
                ;

topstmt         : /* namespace statement:
                    namespace name_of_namespace {
                      declarations
                    }

                    define a new namespace. */
                  keyword_namespace identifier '{' topstmts '}'
                    {
                      $$ = node_ns_new($2, $4);
                    }
                | /* class statement:
                    class name_of_namespace {
                      declarations
                    }

                    alias of namespace statement.
                    namespace work like class when bound with new expression. */
                  keyword_class identifier '{' topstmts '}'
                    {
                      $$ = node_ns_new($2, $4);
                    }
                | /* import statement:
                     import name_of_namespace

                     copies names to the current namespace. */
                  keyword_import identifier
                    {
                      $$ = node_import_new($2);
                    }
                | /* method statement:
                    method foo(args) {
                      statements
                    }

                    define a method named foo.
                    Above method statement works like:

                    method foo(self, args) {
                      statements
                    } */
                  keyword_method fname '(' opt_f_args ')' '{' stmts '}'
                    {
                      $$ = node_let_new($2, node_method_new($4, $7));
                    }
                | /* alternative method statement:
                    method foo(args) = expr

                    define a method named foo.
                    Above method statement works like:

                    method foo(self, args) = expr
                   */
                  keyword_method fname '(' opt_f_args ')' '=' expr
                    {
                      $$ = node_let_new($2, node_method_new($4, $7));
                    }
                | stmt
                ;

stmts           : stmt_list opt_terms
                    {
                      $$ = $1;
                    }
                | terms stmt_list opt_terms
                    {
                      $$ = $2;
                    }
                | opt_terms
                    {
                      $$ = NULL;
                    }
                ;

stmt_list       : stmt
                    {
                      $$ = node_nodes_new();
                      node_lineinfo(p, $$);
                      if ($1) {
                        node_nodes_add($$, $1);
                        node_lineinfo(p, $1);
                      }
                    }
                | stmt_list terms stmt
                    {
                      $$ = $1;
                      if ($3) {
                        if ($1) {
                          node_nodes_add($$, $3);
                        }
                        else {
                          $1 = $3;
                        }
                        node_lineinfo(p, $3);
                      }
                    }
                ;

stmt            : var '=' expr
                    {
                      $$ = node_let_new($1, $3);
                    }
                | /* def statement:
                    def foo(args) {
                      statements
                    }

                    define a function named foo. */
                  keyword_def fname '(' opt_f_args ')' '{' stmts '}'
                    {
                      $$ = node_let_new($2, node_lambda_new($4, $7));
                    }
                | /* alternative def statement:
                    def foo(args) = expr

                    define a function named foo. */
                  keyword_def fname '(' opt_f_args ')' '=' expr
                    {
                      $$ = node_let_new($2, node_lambda_new($4, $7));
                    }
                | /* assignment using def statement:
                    def foo = expr

                    define a function named foo. */
                  keyword_def fname '=' expr
                    {
                      $$ = node_let_new($2, node_lambda_new(NULL, $4));
                    }
                | expr op_rasgn var
                    {
                      $$ = node_let_new($3, $1);
                    }
                | keyword_skip
                    {
                      $$ = node_skip_new();
                    }
                | keyword_emit opt_args
                    {
                      $$ = node_emit_new($2);
                    }
                | keyword_return opt_args
                    {
                      $$ = node_return_new($2);
                    }
                | expr
                ;

var             : identifier
                ;

fname           : identifier
                | lit_string
                    {
                      $$ = ((node_str*)$1)->value;
                    }
                ;

expr            : expr op_plus expr
                    {
                      $$ = node_op_new("+", $1, $3);
                    }
                | expr op_minus expr
                    {
                      $$ = node_op_new("-", $1, $3);
                    }
                | expr op_mult expr
                    {
                      $$ = node_op_new("*", $1, $3);
                    }
                | expr op_div expr
                    {
                      $$ = node_op_new("/", $1, $3);
                    }
                | expr op_mod expr
                    {
                      $$ = node_op_new("%", $1, $3);
                    }
                | expr op_bar expr
                    {
                      $$ = node_op_new("|", $1, $3);
                    }
                | expr op_amper expr
                    {
                      $$ = node_op_new("&", $1, $3);
                    }
                | expr op_gt expr
                    {
                      $$ = node_op_new(">", $1, $3);
                    }
                | expr op_ge expr
                    {
                      $$ = node_op_new(">=", $1, $3);
                    }
                | expr op_lt expr
                    {
                      $$ = node_op_new("<", $1, $3);
                    }
                | expr op_le expr
                    {
                      $$ = node_op_new("<=", $1, $3);
                    }
                | expr op_eq expr
                    {
                      $$ = node_op_new("==", $1, $3);
                    }
                | expr op_neq expr
                    {
                      $$ = node_op_new("!=", $1, $3);
                    }
                | op_plus expr                 %prec '!'
                    {
                      $$ = $2;
                    }
                | op_minus expr                %prec '!'
                    {
                      $$ = node_op_new("-", NULL, $2);
                    }
                | '!' expr
                    {
                      $$ = node_op_new("!", NULL, $2);
                    }
                | '~' expr
                    {
                      $$ = node_op_new("~", NULL, $2);
                    }
                | expr op_and expr
                    {
                      $$ = node_op_new("&&", $1, $3);
                    }
                | expr op_or expr
                    {
                      $$ = node_op_new("||", $1, $3);
                    }
                | '(' opt_f_args op_lambda2 expr
                    {
                      $$ = node_lambda_new($2, $4);
                    }
                | '(' opt_f_args op_lambda3 stmts '}'
                    {
                      $$ = node_lambda_new($2, $4);
                    }
                | keyword_if condition expr opt_else
                    {
                      $$ = node_if_new($2, $3, $4);
                    }
                | primary
                ;

condition       : '(' expr ')'
                    {
                      $$ = $2;
                    }
                ;

opt_else        : /* none */                   %prec keyword_else
                    {
                      $$ = NULL;
                    }
                | keyword_else expr            %prec keyword_else
                    {
                      $$ = $2;
                    }
                ;

opt_args        : /* none */
                    {
                      $$ = NULL;
                    }
                | args
                    {
                      $$ = node_array_headers($1);
                    }
                ;


arg             : expr
                | label expr
                    {
                      $$ = node_pair_new($1, $2);
                    }
                | op_mult expr
                    {
                      $$ = node_splat_new($2);
                    }
                ;

args            : arg
                    {
                      $$ = node_array_new();
                      node_array_add($$, $1);
                    }
                | args ',' arg
                    {
                      $$ = $1;
                      node_array_add($1, $3);
                    }
                ;

primary         : lit_number
                | lit_string
                | lit_symbol
                | lit_time
                | var
                    {
                      $$ = node_ident_new($1);
                    }
                | '(' expr ')'
                    {
                      if ($2->type == NODE_LAMBDA) {
                        node_lambda* lambda = (node_lambda*)$2;
                        if (lambda->block) {
                          lambda->block = 0;
                        }
                      }
                       $$ = $2;
                    }
                | '[' args ']'
                    {
                      $$ = node_array_headers($2);
                    }
                | '[' ']'
                    {
                      $$ = node_array_new();
                    }
                | block
                | keyword_nil
                    {
                      $$ = node_nil();
                    }
                | keyword_true
                    {
                      $$ = node_true();
                    }
                | keyword_false
                    {
                      $$ = node_false();
                    }
                | keyword_new identifier '[' opt_args ']'
                    {
                      $$ = node_obj_new($4, $2);
                    }
                | fname block
                    {
                      $$ = node_call_new($1, NULL, NULL, $2);
                    }
                | fname '(' opt_args ')' opt_block
                    {
                      $$ = node_call_new($1, NULL, $3, $5);
                    }
                | primary '.' fname '(' opt_args ')' opt_block
                    {
                      $$ = node_call_new($3, $1, $5, $7);
                    }
                | primary '.' fname opt_block
                    {
                      $$ = node_call_new($3, $1, NULL, $4);
                    }
                | primary '.' '(' opt_args ')' opt_block
                    {
                      $$ = node_fcall_new($1, $4, $6);
                    }
                | op_amper fname
                    {
                      $$ = node_genfunc_new($2);
                    }
                ;

opt_block       : /* none */
                    {
                      $$ = NULL;
                    }
                | block
                ;

pterm           : var
                    {
                      $$ = node_ident_new($1);
                    }
                | lit_number
                | lit_string
                | keyword_nil
                    {
                      $$ = node_nil();
                    }
                | keyword_true
                    {
                      $$ = node_true();
                    }
                | keyword_false
                    {
                      $$ = node_false();
                    }
                | '[' ']'
                    {
                      $$ = node_pattern_new(NODE_PARRAY);
                    }
                | '[' '@' identifier ']'
                    {
                      $$ = node_ns_new($3, node_pattern_new(NODE_PARRAY));
                    }
                | '[' pattern ']'
                    {
                      $$ = $2;
                    }
                | '[' '@' identifier pattern ']'
                    {
                      $$ = node_ns_new($3, $4);
                    }
                | pterm '@' identifier
                    {
                      $$ = node_ns_new($3, $1);
                    }
                ;

pary            : pterm
                    {
                      $$ = node_pattern_new(NODE_PARRAY);
                      node_pattern_add($$, $1);
                    }
                | pary ',' pterm
                    {
                      $$ = $1;
                      node_pattern_add($$, $3);
                    }
                ;

pstruct         : label pterm
                    {
                      $$ = node_pattern_new(NODE_PSTRUCT);
                      node_pattern_add($$, node_pair_new($1, $2));
                    }
                | pstruct ',' label pterm
                    {
                      $$ = $1;
                      node_pattern_add($$, node_pair_new($3, $4));
                    }
                ;

pattern         : pary
                | pary  ',' op_mult pterm
                    {
                      $$ = node_psplat_new($1, $4, NULL);
                    }
                | pary ',' op_mult pterm ',' pary
                    {
                      $$ = node_psplat_new($1, $4, $6);
                    }
                | op_mult pterm
                    {
                      $$ = node_psplat_new(NULL, $2, NULL);
                    }
                | op_mult pterm ',' pary
                    {
                      $$ = node_psplat_new(NULL, $2, $4);
                    }
                | pstruct
                | pstruct  ',' op_mult pterm
                    {
                      $$ = node_psplat_new($1, $4, NULL);
                    }
                ;

cparam         : op_lambda
                    {
                      $$ = node_plambda_new(node_pattern_new(), NULL);
                    }
                | keyword_if expr op_lambda
                    {
                      $$ = node_plambda_new(node_pattern_new(), $2);
                    }
                | pattern op_lambda
                    {
                      $$ = node_plambda_new($1, NULL);
                    }
                | pattern keyword_if expr op_lambda
                    {
                      $$ = node_plambda_new($1, $3);
                    }
                ;

case_body       : keyword_case cparam stmts
                    {
                      $$ = node_plambda_body($2, $3);
                    }
                | case_body keyword_case cparam stmts
                    {
                      $$ = node_plambda_add($1, node_plambda_body($3, $4));
                    }
                ;

block           : '{' stmts '}'
                    {
                      $$ = node_block_new($2);
                    }
                | '{' bparam stmts '}'
                    {
                      $$ = node_lambda_new($2, $3);
                    }
                | '{' case_body '}'
                    {
                      $$ = $2;
                    }
                | '{' case_body keyword_else op_lambda stmts '}'
                    {
                      node* e = node_plambda_new(NULL, NULL);
                      $$ = node_plambda_add($2, node_plambda_body(e, $5));
                    }
                ;

bparam          : op_lambda
                    {
                      $$ = NULL;
                    }
                | f_args op_lambda
                    {
                      $$ = $1;
                    }
                ;

opt_f_args      : /* no args */
                    {
                      $$ = NULL;
                    }
                | f_args
                ;

f_args          : var
                    {
                      $$ = node_args_new();
                      node_args_add($$, $1);
                    }
                | f_args ',' var
                    {
                      $$ = $1;
                      node_args_add($$, $3);
                    }
                ;

opt_terms       : /* none */
                | terms
                ;

terms           : term
                | terms term {yyerrok;}
                ;

term            : ';' {yyerrok;}
                | '\n'
                ;
%%
//#define yylval  (*((YYSTYPE*)(p->lval)))

#include "lex.yy.c"

static void
yyerror(parser_state *p, const char *s)
{
  p->nerr++;
  if (p->fname) {
    fprintf(stderr, "%s:%d:%s\n", p->fname, p->lineno, s);
  }
  else {
    fprintf(stderr, "%d:%s\n", p->lineno, s);
  }
}
