#pragma once

#include<stdlib.h>
#include<string>
#include<sstream>
#include<vector>

using std::string;
using std::vector;
using std::stringstream;

enum OperandDim {
    FLOAT1=0,
    FLOAT2,
    FLOAT3,
    FLOAT4,
    FLOAT4X4,
    TEXTURE2D,
    DIM_UNDEFINE
};

class OrdinaryOperand;

string int2str( int v );
OperandDim swizzle2Dim( string swizzle );
OrdinaryOperand toOrdinaryOperand( const string str );

class Operand {
public:
    virtual string Type() const = 0;
    virtual int Index() const = 0;
    virtual void setType(const string &) = 0;
    virtual void setIndex(int) = 0;
    virtual OperandDim Dimension() const = 0;
    virtual string FullText() const = 0;
    virtual bool hasSwizzle() const { return false; }
    virtual bool hasArray() const { return false; }
    virtual bool hasTextFlag() const { return false; }
    virtual string getSwizzle() const { return ""; }
    virtual string getArray() const { return ""; }
    virtual string getTextFlag() const { return ""; }
    virtual ~Operand() {}
    virtual Operand* clone() = 0;
};

class OrdinaryOperand : public Operand {

public:
    string  Type() const { return type; }
    int     Index() const { return index; }
    void    setType(const string &s) { type = s; }
    void    setIndex(int i) { index = i; }
    virtual string FullText() const
    {
        string indexStr = ( index == -1 ) ? "" : int2str(index);
        return type + indexStr;
    }
    virtual OperandDim Dimension() const { return FLOAT4; }

    OrdinaryOperand( const string &t, const int i = -1 ) : type(t) , index(i) {}
    OrdinaryOperand( const OrdinaryOperand &opnd ) : type(opnd.Type()) , index(opnd.Index()) {}
    virtual Operand* clone() { Operand* opnd = new OrdinaryOperand(*this);  return opnd; }
protected:
    string type;
    int index;

};

class SwizzleOperand : public OrdinaryOperand {

public:
    string FullText() const { return OrdinaryOperand::FullText() + "." + swizzleString; }
    OperandDim Dimension() const { return dim; }
    bool hasSwizzle() const { return true; }
    string getSwizzle() const { return swizzleString; }

    SwizzleOperand( const string &t, const string &s, int i = -1 ) : OrdinaryOperand(t,i), swizzleString(s), dim(swizzle2Dim(s)) {}
    SwizzleOperand( const OrdinaryOperand &opnd , string s ) : OrdinaryOperand(opnd), swizzleString(s), dim(swizzle2Dim(s)) {}
    SwizzleOperand( const SwizzleOperand &opnd ) : OrdinaryOperand(opnd.Type(), opnd.Index()), swizzleString(opnd.getSwizzle()), dim(opnd.Dimension()) {}
    Operand* clone() { Operand* opnd = new SwizzleOperand(*this);  return opnd; }
private:
    string swizzleString;
    OperandDim dim;

};

struct ArrayExpression {

    enum expressionType
    {
        VALUE_ONLY,
        Operand_ONLY,
        VALUE_AND_Operand
    };

    expressionType exprType;
    SwizzleOperand* opnd;
    int offset;

    ArrayExpression( const SwizzleOperand &opnd )
    {
        this->exprType = Operand_ONLY;
        this->opnd = new SwizzleOperand( opnd.Type(), opnd.getSwizzle(), opnd.Index() );
        this->offset = 0;
    }
    ArrayExpression( int offset )
    {
        this->exprType = VALUE_ONLY;
        this->opnd =  0;
        this->offset = offset;
    }
    ArrayExpression( const SwizzleOperand &opnd, int offset )
    {
        this->exprType = VALUE_AND_Operand;
        this->opnd = new SwizzleOperand( opnd.Type(), opnd.getSwizzle(), opnd.Index() );
        this->offset = offset;
    }
    ArrayExpression( const ArrayExpression &expr )
    {
        this->exprType = expr.exprType;
        if( expr.opnd )
            this->opnd = new SwizzleOperand( expr.opnd->Type(), expr.opnd->getSwizzle(), expr.opnd->Index() );
        else
            this->opnd = 0;
        this->offset = expr.offset;
    }
    ArrayExpression& operator=( const ArrayExpression &expr )
    {
        this->exprType = expr.exprType;
        this->opnd = new SwizzleOperand( expr.opnd->Type(), expr.opnd->getSwizzle(), expr.opnd->Index() );
        this->offset = expr.offset;
        return *this;
    }

    ~ArrayExpression()
    {
        delete opnd;
    }

    string toString() const
    {
        switch( exprType )
        {
            case VALUE_ONLY:
                return "[" + int2str( offset ) + "]";
            case Operand_ONLY:
                return "[" + opnd->FullText() + "]";
            case VALUE_AND_Operand:
                return "[" + opnd->FullText() + "+" + int2str( offset ) + "]";
            default:
                return "";
        }
    }

};


class ArrayOperand : public OrdinaryOperand {

public:
    string FullText() const { return OrdinaryOperand::FullText() + expr.toString(); }
    OperandDim Dimension() const { return FLOAT4X4; }
    bool hasArray() const { return true; }
    string getArray() const { return expr.toString(); }

    ArrayOperand( const string &t,const ArrayExpression &e, int i = -1 ) : OrdinaryOperand(t,i), expr(e) {}
    ArrayOperand( const OrdinaryOperand &opnd , const ArrayExpression &e ) : OrdinaryOperand(opnd), expr(e) {}
    ArrayOperand( const ArrayOperand &opnd ) : OrdinaryOperand( opnd.Type(), opnd.Index() ), expr(opnd.expr) {}
    Operand* clone() { Operand* opnd = new ArrayOperand(*this);  return opnd; }

    ArrayExpression expr;
};


class TextureOperand : public OrdinaryOperand {

public:
    string FullText() const { return OrdinaryOperand::FullText() + textureflag; }
    OperandDim Dimension() const { return TEXTURE2D; }
    bool hasTextFlag() const { return true; }
    string getTextFlag() const { return textureflag; }

    TextureOperand( const string &t, const string &text, int i ) : OrdinaryOperand(t,i), textureflag(text) {}
    TextureOperand( const OrdinaryOperand &opnd , string text ) : OrdinaryOperand( opnd.Type() ,opnd.Index() ) , textureflag(text) {}
    TextureOperand( const TextureOperand & opnd ) : OrdinaryOperand(opnd.Type(), opnd.Index() ) , textureflag(opnd.getTextFlag()) {}
    Operand* clone() { Operand* opnd = new TextureOperand(*this);  return opnd; }
private:
    string textureflag;
};



struct Ast_node {
  string             opcode;  // op
  Operand*              dst;  // lhs
  vector<Operand*> src_list;  // rhs
  string getDstValue()
  {
      string temp = this->opcode;
      for( vector<Operand*>::size_type i=0; i <src_list.size(); i++  )
      {
            temp += src_list[i]->FullText();
      }
      return temp;
  }
  string FullText()
  {
      string text = opcode + " " + dst->FullText() + ", ";
      for( vector<Operand*>::size_type i =0; i < src_list.size(); i++ )
      {
          text += src_list[i]->FullText();
          if( i != src_list.size()-1 )
              text += ", ";
      }
      return text;
  }

};

bool comparePair( const std::pair<unsigned,unsigned> &a , const std::pair<unsigned,unsigned> &b );
