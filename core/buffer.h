// Copyright 2009 The Archiveopteryx Developers <info@aox.org>

#ifndef BUFFER_H
#define BUFFER_H

#include "global.h"
#include "list.h"


class EString;


class Buffer
    : public Garbage
{
public:
    Buffer();

    enum Compression{ None, Compressing, Decompressing };
    void setCompression( Compression );
    Compression compression() const;

    void append( const EString & );
    void append( const char *, uint );

    void read( int );
    void write( int );

    uint size() const { return bytes; }
    void remove( uint );
    EString string( uint ) const;
    EString * removeLine( uint = 0 );

    char operator[]( uint i ) const {
        if ( i >= bytes )
            return 0;

        i += firstused;
        Vector *v = vecs.firstElement();
        if ( v && v->len > i )
            return *( v->base + i );

        return at( i );
    }

    void close();

private:
    char at( uint ) const;

private:
    void append( const char *, uint, bool );
    void append2( const char *, uint );

    struct Vector
        : public Garbage
    {
        Vector() : base( 0 ), len( 0 ) {
            setFirstNonPointer( &len );
        }
        char *base;
        // no pointers after this line
        uint len;
    };

    List< Vector > vecs;
    Compression filter;
    struct z_stream_s * zs;
    uint firstused, firstfree;
    uint bytes;
};


#endif
