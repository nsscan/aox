// Copyright 2009 The Archiveopteryx Developers <info@aox.org>

#ifndef POPCOMMAND_H
#define POPCOMMAND_H

#include "event.h"
#include "pop.h"


class PopCommand
    : public EventHandler
{
public:
    enum Command {
        Quit, Capa, Noop, Stls, Auth, User, Pass, Apop,
        Stat, List, Retr, Dele, Rset, Top, Uidl,
        Session
    };

    PopCommand( class POP *, Command, class EStringList * );

    void read();
    void execute();
    void finish();
    bool done();

private:
    class PopCommandData * d;

    EString nextArg();
    bool startTls();
    bool auth();
    bool user();
    bool pass();
    bool apop();
    bool session();
    bool fetch822Size();
    bool stat();
    bool list();
    bool retr( bool );
    bool dele();
    bool uidl();
};


#endif
