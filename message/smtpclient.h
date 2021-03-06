// Copyright 2009 The Archiveopteryx Developers <info@aox.org>

#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#include "connection.h"
#include "event.h"


class DSN;
class EString;
class Message;
class Address;
class Recipient;


class SmtpClient
    : public Connection
{
public:
    SmtpClient( const Endpoint & );

    void react( Event );

    static SmtpClient * provide();

    bool ready() const;
    void send( DSN *, EventHandler * );
    DSN * sending() const;
    bool sent() const;

    void logout( uint );

    EString error() const;

    static uint observedSize();

private:
    class SmtpClientData * d;

    void parse();
    void sendCommand();
    void handleFailure( const EString & );
    void finish( const char * status );
    void recordExtension( const EString & );

    static EString dotted( const EString & );

    static SmtpClient * idleClient();
};


#endif
