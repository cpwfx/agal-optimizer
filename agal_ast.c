#include "agal_ast.h"

string int2str( int v ) {
    stringstream ss;
    ss << v;
    return ss.str();
}

OperandDim swizzle2Dim( string swizzle ) {
    switch( swizzle.length() )
    {
        case 1:
            return FLOAT1;
        case 2:
            return FLOAT2;
        case 3:
            return FLOAT3;
        case 4:
            return FLOAT4;
        default:
            return DIM_UNDEFINE;
    }
}

OrdinaryOperand toOrdinaryOperand( const string str ) {

    string type;
    int index;

    if(str.length()<3) {
      type = str;
      index = -1;
    }
    else {
     type = str.substr(0,2);
     index = atoi(str.substr(2).c_str());
    }
    return OrdinaryOperand(type, index);
}

bool comparePair( const std::pair<unsigned,unsigned> &a , const std::pair<unsigned,unsigned> &b )
{
         return ( a.second > b.second );
}

