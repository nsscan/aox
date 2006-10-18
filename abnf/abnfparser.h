// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#ifndef ABNFPARSER_H
#define ABNFPARSER_H

#include "global.h"
#include "string.h"


class AbnfParser
    : public Garbage
{
public:
    AbnfParser( const String & );
    virtual ~AbnfParser();

    bool ok() const;
    String error() const;

    char nextChar();
    void step( uint = 1 );
    bool present( const String & );
    void require( const String & );
    String digits( uint, uint );
    String letters( uint, uint );
    uint number( uint = 0 );
    void end();
    const String following() const;

protected:
    String str;
    uint at;

    void setError( const String & );

private:
    String err;
};


#endif
