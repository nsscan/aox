// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#include "managesieve.h"

#include "log.h"
#include "user.h"
#include "query.h"
#include "string.h"
#include "buffer.h"
#include "mechanism.h"
#include "eventloop.h"
#include "stringlist.h"
#include "configuration.h"
#include "managesievecommand.h"


class ManageSieveData
    : public Garbage
{
public:
    ManageSieveData()
        : state( ManageSieve::Unauthorised ), user( 0 ),
          commands( new List< ManageSieveCommand > ), reader( 0 ),
          reserved( false ), readingLiteral( false ),
          literalSize( 0 ), args( 0 )
    {}

    ManageSieve::State state;

    User * user;

    List< ManageSieveCommand > * commands;
    ManageSieveCommand * reader;
    bool reserved;

    bool readingLiteral;
    uint literalSize;

    StringList * args;
};


static void newCommand( List< ManageSieveCommand > *, ManageSieve *,
                        ManageSieveCommand::Command, StringList * = 0,
                        int = -1 );


/*! \class ManageSieve sieve.h
    This class implements a ManageSieve server.

    The ManageSieve protocol is defined in draft-martin-managesieve-06.txt.
*/

/*! Creates a ManageSieve server for the fd \a s, and sends the initial banner.
*/

ManageSieve::ManageSieve( int s )
    : Connection( s, Connection::ManageSieveServer ),
      d( new ManageSieveData )
{
    capabilities();
    setTimeoutAfter( 1800 );
    EventLoop::global()->addConnection( this );
}


/*! Sets this server's state to \a s, which may be either Unauthorised
    or Authorised (as defined in ManageSieve::State).
*/

void ManageSieve::setState( State s )
{
    d->state = s;
}


/*! Returns the server's current state. */

ManageSieve::State ManageSieve::state() const
{
    return d->state;
}


void ManageSieve::react( Event e )
{
    switch ( e ) {
    case Read:
        setTimeoutAfter( 600 );
        parse();
        break;

    case Timeout:
        log( "Idle timeout" );
        send( "BYE Idle timeout" );
        Connection::setState( Closing );
        break;

    case Connect:
    case Error:
    case Close:
        break;

    case Shutdown:
        send( "BYE Server shutdown" );
        break;
    }

    commit();
}


/*! Parses ManageSieve client commands. */

void ManageSieve::parse()
{
    Buffer *b = readBuffer();

    while ( b->size() > 0 ) {
        if ( d->reader ) {
            d->reader->read();
        }
        else if ( d->readingLiteral ) {
            if ( b->size() < d->literalSize )
                return;

            d->args->append( b->string( d->literalSize ) );
            b->remove( d->literalSize );
            d->readingLiteral = false;
        }
        else {
            if ( d->reserved )
                break;

            String *s = b->removeLine( 2048 );

            if ( !s ) {
                log( "Connection closed due to overlong line (" +
                     fn( b->size() ) + " bytes)", Log::Error );
                send( "BYE Line too long. Closing connection." );
                Connection::setState( Closing );
                return;
            }

            StringList * l = StringList::split( ' ', *s );

            if ( !d->args )
                d->args = new StringList;

            StringList::Iterator it( l );
            while ( it ) {
                d->args->append( it );
                ++it;
            }

            String lit = *d->args->last();

            if ( lit[0] == '{' && lit.endsWith( "+}" ) ) {
                bool ok;
                d->literalSize = lit.mid( 1, lit.length()-3 ).number( &ok );
                if ( ok )
                    d->readingLiteral = true;
                else
                    no( "Bad literal" );
            }

            if ( !d->readingLiteral )
                addCommand();
        }

        runCommands();
    }
}


/*! Creates a new ManageSieveCommand based on the arguments received
    from the client.
*/

void ManageSieve::addCommand()
{
    bool unknown = false;

    String cmd = d->args->take( d->args->first() )->lower();

    if ( cmd == "logout" ) {
        newCommand( d->commands, this, ManageSieveCommand::Logout,
                    d->args, 0 );
    }
    else if ( cmd == "capability" ) {
        newCommand( d->commands, this, ManageSieveCommand::Capability,
                    d->args, 0 );
    }
    else if ( d->state == Unauthorised ) {
        if ( cmd == "starttls" ) {
            if ( hasTls() )
                no( "Nested STARTTLS" );
            else
                newCommand( d->commands, this,
                            ManageSieveCommand::StartTls );
        }
        else if ( cmd == "authenticate" ) {
            newCommand( d->commands, this,
                        ManageSieveCommand::Authenticate, d->args );
        }
        else {
            unknown = true;
        }
    }
    else if ( d->state == Authorised ) {
        if ( cmd == "havespace" ) {
            newCommand( d->commands, this,
                        ManageSieveCommand::HaveSpace, d->args, 2 );
        }
        else if ( cmd == "putscript" ) {
            newCommand( d->commands, this,
                        ManageSieveCommand::PutScript, d->args, 2 );
        }
        else if ( cmd == "setactive" ) {
            newCommand( d->commands, this,
                        ManageSieveCommand::SetActive, d->args, 2 );
        }
        else if ( cmd == "listscripts" ) {
            newCommand( d->commands, this,
                        ManageSieveCommand::ListScripts, 0 );
        }
        else if ( cmd == "getscript" ) {
            newCommand( d->commands, this,
                        ManageSieveCommand::GetScript, d->args, 1 );
        }
        else if ( cmd == "deletescript" ) {
            newCommand( d->commands, this,
                        ManageSieveCommand::DeleteScript, d->args, 1 );
        }
        else {
            unknown = true;
        }
    }
    else {
        unknown = true;
    }

    if ( unknown )
        no( "Unknown command" );

    d->args = 0;
}


/*! Sends \a s as a positive OK response. */

void ManageSieve::ok( const String &s )
{
    enqueue( "OK" );
    if ( !s.isEmpty() )
        enqueue( " " + s.quoted() );
    enqueue( "\r\n" );
}


/*! Sends \a s as a negative NO response. */

void ManageSieve::no( const String &s )
{
    enqueue( "NO" );
    if ( !s.isEmpty() )
        enqueue( " " + s.quoted() );
    enqueue( "\r\n" );
    setReader( 0 );
}


/*! Sends the literal response \a s without adding a tag. */

void ManageSieve::send( const String &s )
{
    enqueue( s );
    enqueue( "\r\n" );
}


/*! The ManageSieve server maintains a list of commands received from the
    client and processes them one at a time in the order they were
    received. This function executes the first command in the list,
    or if the first command has completed, removes it and executes
    the next one.

    It should be called when a new command has been created (i.e.,
    by ManageSieve::parse()) or when a running command finishes.
*/

void ManageSieve::runCommands()
{
    List< ManageSieveCommand >::Iterator it( d->commands );
    if ( !it )
        return;
    if ( it->done() )
        d->commands->take( it );
    if ( it )
        it->execute();
}


static void newCommand( List< ManageSieveCommand > * l, ManageSieve * sieve,
                        ManageSieveCommand::Command cmd,
                        StringList * args, int argc )
{
    int ac = 0;
    if ( args )
        ac = (int)args->count();
    if ( argc >= 0 && ac != argc )
        sieve->no( "Wrong number of arguments (expected " +
                   fn( argc ) + ", received " + fn( args->count() ) + ")" );
    else
        l->append( new ManageSieveCommand( sieve, cmd, args ) );
}


/*! Sets the current user of this ManageSieve server to \a u. Called upon
    successful completion of an Authenticate command.
*/

void ManageSieve::setUser( User * u )
{
    d->user = u;
}


/*! Returns the current user of this ManageSieve server, or an empty string if
    setUser() has never been called after a successful authentication.
*/

User * ManageSieve::user() const
{
    return d->user;
}


/*! Reserves the input stream to inhibit parsing if \a r is true. If
    \a r is false, then the server processes input as usual. Used by
    STLS to inhibit parsing.
*/

void ManageSieve::setReserved( bool r )
{
    d->reserved = r;
}


/*! Reserves the input stream for processing by \a cmd, which may be 0
    to indicate that the input should be processed as usual. Used by
    AUTH to parse non-command input.
*/

void ManageSieve::setReader( ManageSieveCommand * cmd )
{
    d->reader = cmd;
    d->reserved = d->reader;
}


/*! Enqueues a suitably-formatted list of our capabilities. */

void ManageSieve::capabilities()
{
    String v( Configuration::compiledIn( Configuration::Version ) );
    enqueue( "\"SIEVE\" \"Fileinto Refuse Reject\"\r\n" );
    enqueue( "\"IMPLEMENTATION\" \"Archiveopteryx " + v + "\"\r\n" );
    enqueue( "\"SASL\" \"" + SaslMechanism::allowedMechanisms( "", hasTls() ) +
             "\"\r\n" );
    enqueue( "\"STARTTLS\"\r\n" );
    enqueue( "OK\r\n" );
}
