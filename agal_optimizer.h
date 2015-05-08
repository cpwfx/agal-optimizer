#pragma once

#include<assert.h>
#include<map>
#include<set>
#include <algorithm>
#include<iostream>
#include<stdio.h>

#include<fstream>

#include"agal_ast.h"

using std::pair;
using std::map;
using std::set;
using std::fstream;
using std::cout;

#define __PS__DEFINED__

#ifdef __PS__DEFINED__
    #define TEMP_REGISTER  "ft"
    #define OUT_REGISTER   "oc"
#else
    #define TEMP_REGISTER  "vt"
    #define OUT_REGISTER   "op"
#endif

#define TEMP_REG_SIZE 8

class AGALOptimizer {

private :
    struct regRecord
    {
        pair<int,int>     liveness;
        string             hashKey;  //for debug purpose
        unsigned long    hashValue;

        regRecord() : liveness(0,-1), hashKey(), hashValue(0) {}
    };

    struct DDGNode
    {
        vector<unsigned>  dependencies;
        unsigned               lineNum;
        unsigned              indegree;
        bool               isRedundant;
        bool                  complete;
        unsigned     longestPathtoSink;

        DDGNode() : dependencies(), lineNum(0), indegree(0), isRedundant(true), complete(false), longestPathtoSink(0) {}
    };

    struct DDGraph
    {
        unsigned rootIndex;
        vector<DDGNode> nodes;
    };

public:
    struct Allocator
    {

        map<int,int> mapping;
        bool isEmpty[TEMP_REG_SIZE];
        bool isFull;

        Allocator()
        {
            mapping = map<int,int>();
            for(int i=0;i<TEMP_REG_SIZE; i++ )
                isEmpty[i] = true;
            isFull = false;
        }
        int request( int key )
        {
            static int ptr = 0;
            if( isFull )
                return -1;
            for(int i=0; i<TEMP_REG_SIZE; i++)
            {
                if( isEmpty[ptr] == true )
                {
                    mapping.insert( pair<int ,int>( key, ptr ) );
                    isEmpty[ptr] = false;
                    int temp = ptr;
                    ptr = (ptr+1) % TEMP_REG_SIZE;
                    return temp;
                }
                ptr = (ptr+1) % TEMP_REG_SIZE;
            }
            isFull = true;
            cout<<"reg full!!\n";
            return -1;
        }
        bool modifyKey( int oldKey, int newKey )
        {
            if( mapping.find(oldKey) != mapping.end() )
            {
                int value = mapping[oldKey];
                mapping.erase(oldKey);
                mapping.insert( pair<int,int>( newKey, value ) );
            }
            return false;
        }
        bool release( int key )
        {
            if( mapping.find(key) != mapping.end() )
            {
                isEmpty[ mapping[key] ] = true;
                mapping.erase(key);
                isFull = false;
                return true;
            }
            return false;
        }
        int query( int key )
        {
            if( mapping.find(key) != mapping.end() )
                return mapping[key];
            else
                return -1;
        }
        bool hasKey( int key )
        {
            if( mapping.find(key) != mapping.end() )
                return true;
            else
                return false;
        }
    };

public:
    void run( const vector<Ast_node*> &raw , const string &output )
    {
        toSSA( raw );
        writeIR("IR.txt");
        LVN();

#ifndef NDEBUG
        writeIR("IR_After_LVN.txt");
        writeAnalysis("IR_Analysis.txt");
#endif
        buildDDG();
        markRedundancy( DDG.nodes[DDG.rootIndex] );

#ifndef NDEBUG
        writeDDG("IR_DDG.txt");
#endif
        easyOpt();
        //Sethi_Ullman( DDG.nodes[ DDG.rootIndex ] );
        writeAgalasm(output);
        //writeEmitSequence("IR_emit.txt");
    }

public:
    unsigned long hashFunction( string key )
    {
        unsigned long value = 0;
        unsigned long length = static_cast<unsigned long>( key.size() );
        unsigned long quotient =  length / 4;
        unsigned long temp;
        for( unsigned long i=0; i<quotient; i++ )
        {
            temp = 1;
            for( unsigned long j=i*4; j<(i+1)*4; j++ )
            {
                value += static_cast<unsigned long>( key[ static_cast<size_t>(j) ] )*temp;
                temp *= 256;
            }
        }

        temp = 1;
        for( unsigned long i=4*quotient; i<length; i++ )
        {
            value += static_cast<unsigned long>( key[ static_cast<size_t>(i) ] )*temp;
            temp *= 256;
        }

        return value;
    }

private:
    void toSSA( const vector<Ast_node*> &raw )
    {
        counter = 0;
        int S[8] = {-1,-1,-1,-1,-1,-1,-1,-1};

        for( unsigned i = 0; i<raw.size(); i++ )
        {
            Ast_node &statement = *raw[i];
            Ast_node* IRstatement = new Ast_node();

            // transform opcode
            IRstatement->opcode = statement.opcode;

            //insert mov instruction if necessary
            Operand *IRdst = statement.dst->clone();
            if( IRdst->Type() == TEMP_REGISTER )
            {
                if( IRdst->hasSwizzle() )
                {
                    // insert mov instruction
                    if( S[IRdst->Index()] > -1 )
                    {
                        Ast_node *IR_insertStatement = new Ast_node();
                        IR_insertStatement->opcode = "mov";
                        IR_insertStatement->src_list.push_back( new OrdinaryOperand( "$", S[IRdst->Index()] ) );
                        IR_insertStatement->dst = new OrdinaryOperand( "$" , counter );
                        agalIR.push_back( IR_insertStatement );
                        isNew.push_back(true);
                    }

                    S[ IRdst->Index() ] = counter;
                    counter++;
                    IRdst->setType("$");
                    IRdst->setIndex( S[ IRdst->Index() ] );
                }
            }

            // transform src regs
            for( unsigned j = 0; j<statement.src_list.size(); j++ )
            {
                Operand *IRsrc = statement.src_list[j]->clone();
                if( IRsrc->Type() == TEMP_REGISTER )
                {
                    IRsrc->setType("$");
                    IRsrc->setIndex( S[IRsrc->Index()] );
                }
                IRstatement->src_list.push_back( IRsrc );
            }

            // transform dst reg
            if( IRdst->Type() == TEMP_REGISTER )
            {
                S[ IRdst->Index() ] = counter;
                counter++;
                IRdst->setType("$");
                IRdst->setIndex( S[ IRdst->Index() ] );
            }
            IRstatement->dst = IRdst;

            agalIR.push_back(IRstatement);
            isNew.push_back(false);
        }
    }

    void LVN()
    {
        IRregRecord = vector<regRecord>( counter , regRecord() );

        for( unsigned i=0; i<agalIR.size(); i++ )
        {
            if( isNew[i] == true )
            {
                Operand &srcReg = *((*agalIR[i]).src_list[0]);
                unsigned long hashValue = IRregRecord[ srcReg.Index() ].hashValue;
                map<unsigned long, set<int> >::iterator it = congruence.find(hashValue);
                srcReg.setIndex( *(it->second.begin()) );
                IRregRecord[ srcReg.Index() ].liveness.second = i;
                IRregRecord[ (*agalIR[i]).dst->Index() ].hashKey =srcReg.FullText()+"/";

                i++;

                Ast_node &nextIRstatement         = *agalIR[i];
                Operand  &nextDstReg              = *nextIRstatement.dst;
                vector<Operand*> &nextSrcReg_list = nextIRstatement.src_list;
                string hashKey = ( nextIRstatement.opcode == "mov" ) ? string() : nextIRstatement.opcode;

                for( unsigned j=0; j<nextSrcReg_list.size(); j++ )
                {
                    Operand &nextSrcReg = *nextSrcReg_list[j];
                    if( nextSrcReg.Type() == "$" )
                    {
                        unsigned long hashValue;
                        if( nextSrcReg.Index() != nextDstReg.Index() )
                            hashValue = IRregRecord[ nextSrcReg.Index() ].hashValue;
                        else
                            hashValue = IRregRecord[ srcReg.Index() ].hashValue;

                        map<unsigned long, set<int> >::iterator it = congruence.find(hashValue);
                        nextSrcReg.setIndex( *it->second.begin() );
                        IRregRecord[nextSrcReg.Index()].liveness.second = i;

                    }
                    hashKey += nextSrcReg.FullText();
                }

                if( nextDstReg.Type() == "$" )
                {
                    IRregRecord[nextDstReg.Index()].liveness.first = i;

                    if( nextDstReg.hasSwizzle() )
                        hashKey = nextDstReg.getSwizzle() + ":" + hashKey;

                    IRregRecord[nextDstReg.Index()].hashKey += hashKey;

                    unsigned long hashValue = hashFunction( IRregRecord[nextDstReg.Index()].hashKey );
                    IRregRecord[nextDstReg.Index()].hashValue = hashValue;

                    map< unsigned long, set<int> >::iterator it = congruence.find(hashValue);
                    if( it != congruence.end() )
                    {
                        it->second.insert( nextDstReg.Index() );
                    }
                    else
                    {
                        congruence.insert( pair< unsigned long, set<int> >( hashValue, set<int>() ) );
                        congruence[hashValue].insert( nextDstReg.Index() );
                    }
                }
            }
            else
            {
                Ast_node &IRstatement         = *agalIR[i];
                Operand  &dstReg              = *IRstatement.dst;
                vector<Operand*> &srcReg_list = IRstatement.src_list;
                string hashKey = ( IRstatement.opcode == "mov" ) ? string() : IRstatement.opcode;

                for( unsigned j=0; j<srcReg_list.size(); j++ )
                {
                    Operand &srcReg = *srcReg_list[j];
                    if( srcReg.Type() == "$" )
                    {
                        unsigned long hashValue = IRregRecord[ srcReg.Index() ].hashValue;
                        map<unsigned long, set<int> >::iterator it = congruence.find(hashValue);
                        srcReg.setIndex( *it->second.begin() );
                        IRregRecord[srcReg.Index()].liveness.second = i;
                    }
                    hashKey += srcReg.FullText();
                }

                if( dstReg.Type() == "$" )
                {
                    IRregRecord[dstReg.Index()].liveness.first = i;

                    if( dstReg.hasSwizzle() )
                        hashKey = dstReg.getSwizzle() + ":" + hashKey;

                    IRregRecord[dstReg.Index()].hashKey = hashKey;

                    unsigned long hashValue = hashFunction( IRregRecord[dstReg.Index()].hashKey );
                    IRregRecord[dstReg.Index()].hashValue = hashValue;

                    map< unsigned long, set<int> >::iterator it = congruence.find(hashValue);
                    if( it != congruence.end() )
                    {
                        it->second.insert( dstReg.Index() );
                    }
                    else
                    {
                        congruence.insert( pair< unsigned long, set<int> >( hashValue, set<int>() ) );
                        congruence[hashValue].insert( dstReg.Index() );
                    }
                }
            }
        }
    }

    void buildDDG()
    {
        DDG.nodes = vector<DDGNode>( agalIR.size() , DDGNode() );

        for( unsigned i=0; i< agalIR.size(); i++ )
            DDG.nodes[i].lineNum = i;

        for( unsigned i=0; i< agalIR.size(); i++ )
        {
            Ast_node &IRstatement  = *agalIR[i];
            if( isNew[i] ==true )
            {
                unsigned srcRegIndex = static_cast< unsigned >( IRstatement.src_list[0]->Index() );
                unsigned firstDef = static_cast< unsigned >( IRregRecord[ srcRegIndex ].liveness.first );
                DDG.nodes[ firstDef ].indegree++;
                DDG.nodes[i].dependencies.push_back( firstDef );
                DDG.nodes[i].indegree++;
                DDG.nodes[i+1].dependencies.push_back( i );

                i++;

                Ast_node &nextIRstatement         = *agalIR[i];
                vector<Operand*> &nextSrcReg_list = nextIRstatement.src_list;

                for( unsigned j=0 ; j< nextSrcReg_list.size(); j++ )
                {
                    if( nextSrcReg_list[j]->Type() == "$" )
                    {
                        unsigned nextsrcRegIndex =  static_cast< unsigned >( nextSrcReg_list[j]->Index() );
                        if( nextsrcRegIndex != srcRegIndex )
                        {
                            unsigned def = static_cast< unsigned >( IRregRecord[ nextsrcRegIndex ].liveness.first );
                            DDG.nodes[ def ].indegree++;
                            DDG.nodes[i].dependencies.push_back( def );
                        }
                    }
                }
            }
            else
            {
                Operand &dstReg               = *IRstatement.dst;
                vector<Operand*> &SrcReg_list = IRstatement.src_list;

                for( unsigned j=0 ; j< SrcReg_list.size(); j++ )
                {
                    if( SrcReg_list[j]->Type() == "$" )
                    {
                        unsigned srcRegIndex = static_cast< unsigned >( SrcReg_list[j]->Index() );
                        unsigned def = static_cast< unsigned >( IRregRecord[ srcRegIndex ].liveness.first );
                        DDG.nodes[ def ].indegree++;
                        DDG.nodes[i].dependencies.push_back( def );
                    }
                }

                if( dstReg.Type() == "oc" )
                {
                    DDG.rootIndex = i;
                }
            }
        }

        for( unsigned i=0; i< DDG.nodes.size(); i++  )
        {
            DDGNode &node = DDG.nodes[i];
            vector< pair<unsigned,unsigned> > childNode;
            if( node.dependencies.size() >0 )
            {
                for( unsigned j=0; j< node.dependencies.size(); j++ )
                {
                    childNode.push_back( pair<unsigned,unsigned>(  node.dependencies[j] , DDG.nodes[ node.dependencies[j] ].longestPathtoSink ) );
                }
                std::sort( childNode.begin(), childNode.end(), comparePair );

                node.dependencies.clear();
                for( unsigned j=0; j< childNode.size(); j++ )
                    node.dependencies.push_back( childNode[j].first );
                node.longestPathtoSink = DDG.nodes[ node.dependencies[0] ].longestPathtoSink + 1;
            }
        }
    }

    void  markRedundancy( DDGNode &node )
    {
        if( node.complete )
            return;

        for( unsigned i=0; i<node.dependencies.size(); i++ )
        {
            markRedundancy( DDG.nodes[ node.dependencies[i] ]  );
        }

        node.isRedundant = false;
        node.complete = true;
        return;
    }

    void Sethi_Ullman( DDGNode &node )
    {
        if( node.complete )
        {
            node.indegree--;
            return;
        }

        for( unsigned i=0; i<node.dependencies.size(); i++ )
        {
            Sethi_Ullman( DDG.nodes[ node.dependencies[i] ]  );
        }

        Ast_node &IRstatement = *agalIR[ node.lineNum ];

        if( isNew[node.lineNum] )
        {
            Ast_node &nextIRstatement = *agalIR[ node.lineNum+1 ];
            for(unsigned i=0; i< nextIRstatement.src_list.size(); i++)
            {
                Operand &srcReg = *nextIRstatement.src_list[i];
                if( srcReg.Type() == "$" )
                    if( srcReg.Index() == IRstatement.src_list[0]->Index() )
                        srcReg.setIndex( IRstatement.dst->Index() );
            }

            unsigned indegree = DDG.nodes[ node.dependencies[0] ].indegree;
            if( indegree == 0 )
            {
                regAllocator.modifyKey( IRstatement.src_list[0]->Index(), IRstatement.dst->Index() );
                emitsequence.push_back(  int2str(node.lineNum) + ": " + IRstatement.FullText() + "  --" );
            }
            else
            {
                int hardIndex = regAllocator.request( IRstatement.dst->Index() );
                IRstatement.dst->setIndex( hardIndex );
                IRstatement.dst->setType( TEMP_REGISTER );
                IRstatement.src_list[0]->setIndex( regAllocator.query( IRstatement.src_list[0]->Index() ) );
                IRstatement.src_list[0]->setType( TEMP_REGISTER );
                agalasm.push_back( IRstatement.FullText() );
                emitsequence.push_back( int2str(node.lineNum) + ": " + IRstatement.FullText() + " ++");   //debug
            }
        }
        else
        {
            Operand &dst = *IRstatement.dst;
            vector<Operand*>  &src_list = IRstatement.src_list;

            bool dstRegExist = false;
            if( dst.Type() == "$" )
            {
                dst.setType( TEMP_REGISTER );
                int hardIndex;
                if( regAllocator.hasKey( dst.Index() ) )
                {
                    hardIndex = regAllocator.query( dst.Index() );
                    dstRegExist = true;
                }
                else
                    hardIndex = regAllocator.request( dst.Index() );
                dst.setIndex( hardIndex );
            }

            for( unsigned i =0; i<src_list.size(); i++ )
            {
                if( src_list[i]->Type() == "$" )
                {
                    src_list[i]->setType( TEMP_REGISTER );
                    unsigned pseudoIndex = static_cast<unsigned>( src_list[i]->Index() );
                    int hardIndex = regAllocator.query( pseudoIndex );
                    src_list[i]->setIndex( hardIndex );
                    if(  DDG.nodes[ IRregRecord[ pseudoIndex ].liveness.first ].indegree == 0 )
                    {
                        if( !dstRegExist )
                        {
                            regAllocator.release( pseudoIndex );
                        }
                    }
                }
            }
            agalasm.push_back( IRstatement.FullText() );
            emitsequence.push_back( int2str(node.lineNum) + ": " + IRstatement.FullText() );
        }
        node.indegree--;
        node.complete = true;
        return;
    }

    void easyOpt()
    {
        for(unsigned i=0; i<agalIR.size(); i++)
        {
            if( isNew[i] )
            {
                Ast_node & IRstatement = *agalIR[i];
                Ast_node & nextIRstatement = *agalIR[i+1];
                for( unsigned j=0; j< nextIRstatement.src_list.size(); j++ )
                {
                    if( nextIRstatement.src_list[j]->Type() == "$" )
                        if( nextIRstatement.src_list[j]->Index() == IRstatement.src_list[0]->Index() )
                            nextIRstatement.src_list[j]->setIndex( IRstatement.dst->Index() );
                }
            }
        }

        for(unsigned i=0; i<agalIR.size(); i++)
        {
            Ast_node & IRstatement = *agalIR[i];

            for( unsigned j=0; j< IRstatement.src_list.size(); j++ )
            {
                if( IRstatement.src_list[j]->Type() == "$" )
                    IRregRecord[ IRstatement.src_list[j]->Index() ].liveness.second = static_cast<int>(i);
            }
        }

        writeAnalysis("IR_analysis2.txt");
        writeIR("IR_After.txt");

        for(unsigned i=0; i<agalIR.size(); i++)
        {
            Ast_node &IRstatement = *agalIR[i];
            Operand &dst = *IRstatement.dst;
            vector<Operand*>  &src_list = IRstatement.src_list;

            if( DDG.nodes[i].isRedundant )
                continue;

            if( isNew[i] )
            {
                if( IRregRecord[ src_list[0]->Index() ].liveness.second == static_cast<int>(i) )
                {
                    regAllocator.modifyKey( src_list[0]->Index(), dst.Index() );
                    continue;
                }
                else
                {
                    int hardIndex = regAllocator.request( dst.Index() );
                    dst.setType( TEMP_REGISTER );
                    dst.setIndex(hardIndex);
                    src_list[0]->setType( TEMP_REGISTER );
                    src_list[0]->setIndex(regAllocator.query( src_list[0]->Index() ) );
                    agalasm.push_back( IRstatement.FullText() + "  ++" );
                }
            }
            else
            {
                vector<int> indices;
                for( unsigned j=0; j< src_list.size(); j++ )
                {
                    if( src_list[j]->Type() == "$" )
                    {
                        src_list[j]->setType( TEMP_REGISTER );
                        indices.push_back( src_list[j]->Index() );
                        src_list[j]->setIndex( regAllocator.query( src_list[j]->Index() ) );
                    }
                }

                if( dst.Type() == "$" )
                {
                    dst.setType( TEMP_REGISTER );
                    if( regAllocator.hasKey( dst.Index() ) )
                        dst.setIndex( regAllocator.query( dst.Index() ) );
                    else
                    {   int index = dst.Index();
                        int hardIndex = regAllocator.request( dst.Index() );
                        cout<<i<<": "<<"allocate "<<hardIndex<<" ";
                        dst.setIndex( regAllocator.query( index ) );
                        cout<<IRstatement.FullText()<<"\n";
                    }
                }

                for( unsigned j=0; j <indices.size(); j++ )
                {
                    if( IRregRecord[  indices[j] ].liveness.second == static_cast<int>(i) )
                    {
                        int index = regAllocator.query( indices[j] );
                        regAllocator.release( indices[j] );
                        cout<<i<<": "<<"release "<<index<<" "<<IRstatement.FullText()<<"\n";
                    }
                }

                agalasm.push_back( IRstatement.FullText() );
            }
        }
    }

private:
    void writeIR( string fileName )
    {
        fstream fp;
        fp.open( fileName.c_str() , std::ios::out);

        fp<<"AGAL IR:\n";
        for( unsigned i=0; i<agalIR.size(); i++)
        {
            fp<<i<<": "<<agalIR[i]->FullText()<<"\n";
        }

        fp.close();
    }

    void writeAnalysis( string fileName )
    {
        fstream fp;
        fp.open( fileName.c_str() , std::ios::out);

        fp<<"liveness:\n";
        for( unsigned i=0; i< IRregRecord.size(); i++)
        {
            fp<<"$"<<i<<": "<<IRregRecord[i].liveness.first<<","<<IRregRecord[i].liveness.second<<"\n";
        }

        fp<<"\nhash key:\n";
        for( unsigned i=0; i< IRregRecord.size(); i++)
        {
            fp<<"$"<<i<<": "<<IRregRecord[i].hashKey<<"\n";
        }

        fp<<"\ncongruence:\n";
        for( map< unsigned long, set<int> >::iterator it = congruence.begin(); it!=congruence.end(); it++ )
        {
            fp<<it->first<<": ";
            for( set<int>::iterator slt = it->second.begin(); slt != it->second.end(); slt++ )
                fp<<*slt<<", ";
            fp<<"\n";
        }
        fp.close();
    }

    void writeDDG( string fileName )
    {
        fstream fp;
        fp.open( fileName.c_str() , std::ios::out);

        vector<DDGNode> &nodes = DDG.nodes;
        fp<<"DDG:\nroot: "<<DDG.rootIndex<<"\n";
        for( unsigned i=0; i<nodes.size(); i++)
        {
            fp<<"#"<<i<<"\n"<<"dependencies: ";
            if( nodes[i].dependencies.size() == 0 )
            {
                fp<<"none";
            }
            else
            {
                for( unsigned j=0; j<nodes[i].dependencies.size(); j++ )
                    fp<<"#"<<nodes[i].dependencies[j]<<"   ";
            }

            fp<<"\nindegree: "<<nodes[i].indegree<<"\n";
        }
        fp.close();
    }

    void writeAgalasm( string fileName )
    {
        fstream fp;
        fp.open( fileName.c_str() , std::ios::out);

        for( unsigned i=0; i<agalasm.size(); i++)
        {
            fp<<agalasm[i];
            if(i != agalasm.size()-1 )
                fp<<"\n";
        }
        fp.close();
    }

    void writeEmitSequence( string fileName )
    {
        fstream fp;
        fp.open( fileName.c_str() , std::ios::out);

        for( unsigned i=0; i<emitsequence.size(); i++)
        {
            fp<<emitsequence[i];
            if(i != emitsequence.size()-1 )
                fp<<"\n";
        }
        fp.close();
    }

public:
    vector<Ast_node*>                  agalIR;
    vector<string>                    agalasm;
    vector<regRecord>             IRregRecord;
    map< unsigned long, set<int> > congruence; // congruence class
    DDGraph                               DDG; // data dependency graph

private:
    vector<bool> isNew;
    int counter;
    Allocator regAllocator; // register allocator
    vector<string> emitsequence;
    bool explode;
};
