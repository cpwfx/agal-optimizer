#include<stdio.h>
#include"agal_ast.h"
#include "agal_optimizer.h"

extern FILE *yyin, *yyout;
extern int yyparse(void);

// parse tree
vector<Ast_node*> parseTree;

int main(int argc, char** argv )
{
    if( argc == 1 )
    {
        std::cerr<<"agal: fatal error: no input file\nusage: agal <input file name>  <output file name>\n";
        return 0;
    }

    string input = string(argv[1]);
    string output;

    if( argc == 2 )
    {
        if( !( yyin = fopen( input.c_str(), "r" ) ) )
        {
            std::cerr<<"error: \""<<input<<"\": No such file or directory\n";
            return 0;
        }
    }

    if( argc > 2 )
        output = string(argv[2]);
    else
        output = "out.agal";

    yyparse();
    fclose(yyin);
    AGALOptimizer opt;
    opt.run( parseTree , output );
    return 0;
}
