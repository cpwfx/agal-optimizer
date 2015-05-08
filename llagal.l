%{

#include<stdio.h>
#include<string.h>
#include"agal_ast.h"
#include"test.tab.h"

extern int line_num;

void yyerror(const char *message)
{ 
    fprintf( stderr, " %s [line %d]: '%s'\n", message, line_num, yytext );
    //fprintf( stderr, "error C1001: Syntax error [line %d]: missing something before \"%s\"\n", line_num, yytext );
    //fprintf( stderr, "%s [line %d]: '%s'\n", message, line_num, yytext );
    exit(EXIT_FAILURE);
}

%}

%option noyywrap nounput pointer
%x COMMENT CHECK_MODE TEXTURE_MODE ARRAY_MODE MASK GET_DOT MASK_IN_ARRAY

WHITESPACE [ \t\r]+
VALUE      [0-9]|([1-9][0-9]*)
LITERAL    [a-zA-Z0-9_][a-zA-Z0-9_]*

OC         oc  
FT         ft[0-7]  
FC         fc([0-9]|[1-9][0-9]|1[0-1][0-9]|12[0-7]) 
FC_ARRAY   fc
FS         fs[0-7]  
OP         op
VT         vt[0-7]  
VC         vc([0-9]|[1-9][0-9]|1[0-1][0-9]|12[0-7]) 
VC_ARRAY   vc
VA         va[0-7]  
V          v[0-7] 

%%
    /* single line comment */
"//"                                    { BEGIN(COMMENT); }
<COMMENT>.                              {}
<COMMENT>\n                             { line_num++; BEGIN(INITIAL); }

    /* keywords for pixel shader register  */
{OC}                           { strcpy(yylval.str, yytext); BEGIN(CHECK_MODE); return T_PS_OC; }
{FT}                           { strcpy(yylval.str, yytext); BEGIN(CHECK_MODE); return T_PS_FT; }
{FC}                           { strcpy(yylval.str, yytext); BEGIN(CHECK_MODE); return T_PS_FC; }
{FC_ARRAY}                     { strcpy(yylval.str, yytext); BEGIN(ARRAY_MODE); return T_PS_FC_ARRAY; }
{FS}                           { strcpy(yylval.str, yytext); BEGIN(TEXTURE_MODE); return T_PS_FS; }
<TEXTURE_MODE>[ \t]*"<"[ \t]*{LITERAL}[ \t]*(,[ \t]*{LITERAL})*[ \t]*">"   { strcpy(yylval.str, yytext); BEGIN(INITIAL); return TEXTURE_FLAGS; }
<TEXTURE_MODE>.|\n                                                         { yyerror("syntax error in texture flag\n");  }                                                  

    /* keywords for vertex shader register */
{OP}                           { strcpy(yylval.str, yytext); BEGIN(CHECK_MODE); return T_VS_OP; }
{VT}                           { strcpy(yylval.str, yytext); BEGIN(CHECK_MODE); return T_VS_VT; }
{VC}                           { strcpy(yylval.str, yytext); BEGIN(CHECK_MODE); return T_VS_VC; }
{VC_ARRAY}                     { strcpy(yylval.str, yytext); BEGIN(ARRAY_MODE); return T_VS_VC_ARRAY; }
{VA}                           { strcpy(yylval.str, yytext); BEGIN(CHECK_MODE); return T_VS_VA; }

    /* varying register */
{V}                            { strcpy(yylval.str, yytext); BEGIN(CHECK_MODE); return T_VSPS_V; }

    /* opcode */
min                                       { return T_OP_MIN; }
max                                       { return T_OP_MAX; }
sqt                                       { return T_OP_SQT; }
rsq                                       { return T_OP_RSQ; }
pow                                       { return T_OP_POW; }
log                                       { return T_OP_LOG; }
exp                                       { return T_OP_EXP; }
nrm                                       { return T_OP_NRM; }
abs                                       { return T_OP_ABS; }
sat                                       { return T_OP_SAT; }
mov                                       { return T_OP_MOV; }
add                                       { return T_OP_ADD; }
sub                                       { return T_OP_SUB; }
mul                                       { return T_OP_MUL; }
div                                       { return T_OP_DIV; }
rcp                                       { return T_OP_RCP; }
frc                                       { return T_OP_FRC; }
neg                                       { return T_OP_NEG; }
sin                                       { return T_OP_SIN; }
cos                                       { return T_OP_COS; }
kil                                       { return T_OP_KIL; }
sge                                       { return T_OP_SGE; }
slt                                       { return T_OP_SLT; }
tex                                       { return T_OP_TEX; }
crs                                       { return T_OP_CRS; }
dp3                                       { return T_OP_DP3; }
dp4                                       { return T_OP_DP4; }
m33                                       { return T_OP_M33; }
m44                                       { return T_OP_M44; }
m34                                       { return T_OP_M34; }

","                                       { return COMMA; }
<ARRAY_MODE>"["                           { return L_BRACKET; }
<ARRAY_MODE>"]"                           { BEGIN(INITIAL);  return R_BRACKET; }
<ARRAY_MODE>"+"                           { return PLUS; }
<ARRAY_MODE>{VALUE}                       { yylval.value = (int) atoi(yytext); return VALUE; }
<ARRAY_MODE>{FT}                          { strcpy(yylval.str, yytext); BEGIN(GET_DOT); return T_PS_FT; }
<ARRAY_MODE>{FC}                          { strcpy(yylval.str, yytext); BEGIN(GET_DOT); return T_PS_FC; }
<ARRAY_MODE>{FS}                          { strcpy(yylval.str, yytext); BEGIN(GET_DOT); return T_PS_FS; }
<ARRAY_MODE>{VT}                          { strcpy(yylval.str, yytext); BEGIN(GET_DOT); return T_VS_VT; }
<ARRAY_MODE>{VC}                          { strcpy(yylval.str, yytext); BEGIN(GET_DOT); return T_VS_VC; }
<ARRAY_MODE>{VA}                          { strcpy(yylval.str, yytext); BEGIN(GET_DOT); return T_VS_VA; }
<ARRAY_MODE>{V}                           { strcpy(yylval.str, yytext); BEGIN(GET_DOT); return T_VSPS_V; }
<ARRAY_MODE>{WHITESPACE}                  {}
<ARRAY_MODE>.|\n                          { yyerror("syntax error"); }
<GET_DOT>"."                              { BEGIN(MASK_IN_ARRAY); return DOT; }
<GET_DOT>.|\n                             { yyerror("syntax error"); }
<MASK_IN_ARRAY>[xyzw]                     { strcpy(yylval.str, yytext); BEGIN(ARRAY_MODE); return P_SWIZZLE; }
<MASK_IN_ARRAY>[rgba]                     { strcpy(yylval.str, yytext); BEGIN(ARRAY_MODE); return C_SWIZZLE; }
<MASK_IN_ARRAY>.|\n                       { yyerror("syntax error"); }
<CHECK_MODE>","                           { BEGIN(INITIAL); return COMMA; }
<CHECK_MODE>{WHITESPACE}|\n               { BEGIN(INITIAL); }
<CHECK_MODE>"."                           { BEGIN(MASK); return DOT; }
<CHECK_MODE>.                             { yyerror("syntax error"); }   
<MASK>[xyzw][xyzw][xyzw]?[xyzw]?          { strcpy(yylval.str, yytext); BEGIN(INITIAL); return P_SWIZZLES; }
<MASK>[rgba][rgba][rgba]?[rgba]?          { strcpy(yylval.str, yytext); BEGIN(INITIAL); return C_SWIZZLES; }
<MASK>[xyzw]                              { strcpy(yylval.str, yytext); BEGIN(INITIAL); return P_SWIZZLE; }
<MASK>[rgba]                              { strcpy(yylval.str, yytext); BEGIN(INITIAL); return C_SWIZZLE; }
<MASK>.|\n                                { yyerror("syntax error"); }

    /* etc */
{WHITESPACE}                              {}
\n                                        { line_num++; }

  /* error */
.                                         { yyerror("unknown token"); }    

%%
