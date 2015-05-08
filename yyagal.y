%{

#include<string.h>
#include"agal_ast.h"
#include<stdio.h>
#include<iostream>
using std::cout;

#define __PS__DEFINED__

#ifdef __PS__DEFINED__
    #define IS_PIXELSHADER  1
    #define IS_VERTEXSHADER 0
    #define OUT_PUT_REG  "oc"
    #define TEMP_REG     "ft"
#else
    #define IS_PIXELSHADER  0
    #define IS_VERTEXSHADER 1
    #define OUT_PUT_REG  "op"
    #define TEMP_REG     "vt"
#endif

extern vector<Ast_node*> parseTree;

extern char* yytext;
extern int yylex(void);
extern void yyerror(const char *message);

int line_num = 1;
static bool isTempDef[8] = {false,false,false,false,false,false,false,false};
static bool isOutputDef = false;

void parseError( const string &message, const string &code  )
{
    fprintf( stderr , "%s [line %d]: %s", message.c_str(), line_num, code.c_str() );
    exit(EXIT_FAILURE);
}


void checkMask( char swizzle[] )
{
    string s(swizzle);
    for( std::size_t i=0; i < s.size()-1; i++ )
    {
       for( std::size_t j=i+1; j<s.size(); j++ )
       {
           if(s[i]==s[j])
               parseError("error C3100: Destination mask conflict" , s );
       }
    }

}

%}

%error-verbose


%union {
    int value;
    char str[1024];
    Ast_node* node;
    Operand* opnd;
    ArrayExpression* arrayexpr;
}

    /* pixel shader register tokens */
%token<str> T_PS_OC T_PS_FT T_PS_FC T_PS_FS T_PS_FC_ARRAY
    /* vertex shader register tokens */
%token<str> T_VS_OP T_VS_VT T_VS_VC T_VS_VA T_VSPS_V T_VS_VC_ARRAY
    /* operands & operands */
%token L_BRACKET R_BRACKET PLUS COMMA DOT
%token<str> P_SWIZZLE C_SWIZZLE P_SWIZZLES C_SWIZZLES TEXTURE_FLAGS
%token<value> VALUE
    /* opcode tokens */
%token T_OP_MIN T_OP_MAX T_OP_SQT T_OP_RSQ T_OP_LOG T_OP_EXP T_OP_NRM T_OP_ABS T_OP_SAT
%token T_OP_MOV T_OP_RCP T_OP_FRC T_OP_NEG T_OP_SIN T_OP_COS
%token T_OP_POW T_OP_ADD T_OP_SUB T_OP_MUL T_OP_DIV T_OP_SGE T_OP_SLT T_OP_TEX T_OP_CRS T_OP_DP3
%token T_OP_DP4 T_OP_M33 T_OP_M44 T_OP_M34
%token T_OP_KIL

%type<str> op_one_operand op_two_operands op_three_operands src_reg src_reg_array dst_reg allswizzle singleswizzle
%type<node> agal_statements agalstatement
%type<opnd> src dst
%type<arrayexpr> expr

%%

agal_shader:
{
    if(!isOutputDef)
    {
       fprintf( stderr , "error C2010: Missing output register: %s", OUT_PUT_REG );
       exit(EXIT_FAILURE);
    }

}
|agal_statements
{
    if(!isOutputDef)
    {
       fprintf( stderr , "error C2010: Missing output register: %s", OUT_PUT_REG );
       exit(EXIT_FAILURE);
    }

};


agal_statements: agal_statements agalstatement
{
    parseTree.push_back($2);
}
| agalstatement
{
    parseTree.push_back($1);
};


agalstatement: op_one_operand dst
{
    if( $2->Type() == TEMP_REG )
        if( isTempDef[ $2->Index() ] == false )
            parseError("error C2000: Use a register before its definition" , $2->FullText() );
    if( $2->Dimension() != FLOAT1 )
        parseError( "error C3000: Destination in \"kil\" is not a scalar" , $2->FullText() );

    $$ = new Ast_node();
    $$->opcode = $1;
    $$->dst = $2;
    //cout<<"match statement 1op\n";
}
| op_two_operands dst COMMA src
{
    if( $4->Type() == TEMP_REG )
        if( isTempDef[ $4->Index() ] == false )
            parseError("error C2000: Use a register before its definition" , $4->FullText() );
    if( $2->Type() == TEMP_REG )
        isTempDef[ $2->Index() ] = true;
    
    if( $2->Type() == OUT_PUT_REG )    
        isOutputDef = true;

    $$ = new Ast_node();
    $$->opcode = $1;
    $$->dst = $2;
    $$->src_list.push_back($4);
    //cout<<"match statement 2op\n";

}
| op_three_operands dst COMMA src COMMA src
{
    if( $4->Type() == TEMP_REG )
        if( isTempDef[ $4->Index() ] == false )
            parseError("error C2000: Use a register before its definition" , $4->FullText() );
    if( $6->Type() == TEMP_REG  )
        if( isTempDef[ $6->Index() ] == false )
            parseError("error C2000: Use a register before its definition" , $6->FullText() );
    if( $2->Type() == TEMP_REG )
        isTempDef[ $2->Index() ] = true;
    if( $2->Type() == OUT_PUT_REG )    
        isOutputDef = true;


    $$ = new Ast_node();
    $$->opcode = $1;
    $$->dst = $2;
    $$->src_list.push_back($4);
    $$->src_list.push_back($6);

    //cout<<"match statement 3op\n";
};


op_one_operand: T_OP_KIL
{
    if( IS_VERTEXSHADER )
        parseError( "error C0003: Not a vertex shader operation" , "\"kil\"" );
    strcpy($$ , "kil");
};

op_two_operands:
  T_OP_MIN       { strcpy($$ , "min"); }
  | T_OP_MAX     { strcpy($$ , "max"); }
  | T_OP_SQT     { strcpy($$ , "sqt"); }
  | T_OP_RSQ     { strcpy($$ , "rsq"); }
  | T_OP_LOG     { strcpy($$ , "log"); }
  | T_OP_EXP     { strcpy($$ , "exp"); }
  | T_OP_NRM     { strcpy($$ , "nrm"); }
  | T_OP_ABS     { strcpy($$ , "abs"); }
  | T_OP_SAT     { strcpy($$ , "sat"); }
  | T_OP_MOV     { strcpy($$ , "mov"); }
  | T_OP_RCP     { strcpy($$ , "rcp"); }
  | T_OP_FRC     { strcpy($$ , "frc"); }
  | T_OP_NEG     { strcpy($$ , "neg"); }
  | T_OP_SIN     { strcpy($$ , "sin"); }
  | T_OP_COS     { strcpy($$ , "cos"); }
  ;

op_three_operands:
  T_OP_POW       { strcpy($$ , "pow"); }
  | T_OP_ADD     { strcpy($$ , "add"); }
  | T_OP_SUB     { strcpy($$ , "sub"); }
  | T_OP_MUL     { strcpy($$ , "mul"); }
  | T_OP_DIV     { strcpy($$ , "div"); }
  | T_OP_SGE     { strcpy($$ , "sge"); }
  | T_OP_SLT     { strcpy($$ , "slt"); }
  | T_OP_CRS     { strcpy($$ , "crs"); }
  | T_OP_DP3     { strcpy($$ , "dp3"); }
  | T_OP_DP4     { strcpy($$ , "dp4"); }
  | T_OP_M33     { strcpy($$ , "m33"); }
  | T_OP_M44     { strcpy($$ , "m44"); }
  | T_OP_M34     { strcpy($$ , "m34"); }
  | T_OP_TEX     {
      if( IS_VERTEXSHADER )
          parseError( "error C0003: Not a vertex shader operation" , "\"tex\"" );
      strcpy($$ , "tex");
  };

dst: dst_reg
{
    $$ = new OrdinaryOperand( toOrdinaryOperand($1) );
}
| dst_reg DOT allswizzle
{
    checkMask($3);
    OrdinaryOperand opnd = toOrdinaryOperand($1);
    string swizzle($3);
    $$ = new SwizzleOperand( opnd, swizzle );
};


src: src_reg
{
   $$ = new OrdinaryOperand( toOrdinaryOperand($1) );
}
| src_reg DOT allswizzle
{
    OrdinaryOperand opnd = toOrdinaryOperand($1);
    string swizzle($3);
    $$ = new SwizzleOperand( opnd, swizzle );
}
| src_reg TEXTURE_FLAGS
{
    OrdinaryOperand opnd = toOrdinaryOperand($1);
    string textflags($2);
    $$ = new TextureOperand( opnd, textflags );
}
| src_reg_array  L_BRACKET expr R_BRACKET
{
   OrdinaryOperand opnd = toOrdinaryOperand($1);
   ArrayExpression* expr = $3;
   $$ = new ArrayOperand( opnd, *expr );
};


allswizzle: P_SWIZZLE
       | P_SWIZZLES
       | C_SWIZZLE
       | C_SWIZZLES
{
    strcpy($$,$1);
};


singleswizzle: P_SWIZZLE
             | C_SWIZZLE
{
    strcpy($$,$1);
};


expr: src_reg DOT singleswizzle PLUS VALUE
{
    OrdinaryOperand opnd = toOrdinaryOperand($1);
    string swizzle($3);
    $$ = new ArrayExpression( SwizzleOperand( opnd, swizzle ) , $5 );
}
| src_reg DOT singleswizzle
{
    OrdinaryOperand opnd = toOrdinaryOperand($1);
    string swizzle($3);
    $$ = new ArrayExpression( SwizzleOperand( opnd, swizzle ) );
}
| VALUE
{
    $$ = new ArrayExpression($1);
};


dst_reg: T_PS_OC
{
    if( IS_VERTEXSHADER )
        parseError( "error C0001: Not a vertex shader register" , string($1) );
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
}
| T_PS_FT
{
    if( IS_VERTEXSHADER )
        parseError( "error C0001: Not a vertex shader register" , string($1) );
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
}
| T_VS_OP
{
    if( IS_PIXELSHADER )
         parseError( "error C0002: Not a pixel shader register" , string($1) );
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
}
| T_VS_VT
{
    if( IS_PIXELSHADER )
         parseError( "error C0002: Not a pixel shader register" , string($1) );
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
}
| T_VSPS_V
{
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
};

src_reg: T_PS_FT
{
    if( IS_VERTEXSHADER )
        parseError( "error C0001: Not a vertex shader register" , string($1) );
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
}
| T_PS_FC
{
    if( IS_VERTEXSHADER )
         parseError( "error C0001: Not a vertex shader register" , string($1) );
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
}
| T_PS_FS
{
    if( IS_VERTEXSHADER )
         parseError( "error C0001: Not a vertex shader register" , string($1) );
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
}
| T_VS_VT
{
    if( IS_PIXELSHADER )
         parseError( "error C0002: Not a pixel shader register" , string($1) );
    strcpy($$ , $1);
   // cout<<"match "<<$1<<"\n";
}
| T_VS_VC
{
    if( IS_PIXELSHADER )
         parseError( "error C0002: Not a pixel shader register" , string($1) );
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
}
| T_VS_VA
{
    if( IS_PIXELSHADER )
         parseError( "error C0002: Not a pixel shader register" , string($1) );
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
}
| T_VSPS_V
{
    strcpy($$ , $1);
    //cout<<"match "<<$1<<"\n";
};


src_reg_array: T_PS_FC_ARRAY
{
    if( IS_VERTEXSHADER )
        parseError( "error C0001: Not a vertex shader register" , string($1) );
    strcpy($$ , $1);
}
|T_VS_VC_ARRAY
{
    if( IS_PIXELSHADER )
         parseError( "error C0002: Not a pixel shader register" , string($1) );
    strcpy($$ , $1);
};


%%
