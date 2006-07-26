// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#include "append.h"

#include "date.h"
#include "imap.h"
#include "list.h"
#include "query.h"
#include "string.h"
#include "message.h"
#include "mailbox.h"
#include "injector.h"
#include "imapsession.h"


class AppendData
    : public Garbage
{
public:
    AppendData()
        : mailbox( 0 ), message( 0 ), injector( 0 ),
          permissions( 0 )
    {}

    Date date;
    String mbx;
    Mailbox * mailbox;
    Message * message;
    Injector * injector;
    StringList flags;
    Permissions * permissions;
};


/*! \class Append append.h
    Adds a message to a mailbox (RFC 3501 section 6.3.11)

    Parsing mostly relies on the Message class, execution on the
    Injector. There is no way to insert anything but conformant
    messages, unlike some other IMAP servers. How could we do that?
    Not at all, I think.
*/

Append::Append()
    : Command(), d( new AppendData )
{
    // nothing more needed
}


void Append::parse()
{
    // the grammar used is:
    // append = "APPEND" SP mailbox SP [flag-list SP] [date-time SP] literal
    space();
    d->mbx = astring();
    space();

    if ( present( "(" ) ) {
        if ( nextChar() != ')' ) {
            d->flags.append( flag() );
            while( nextChar() == ' ' ) {
                space();
                d->flags.append( flag() );
            }
        }
        require( ")" );
        space();
    }

    if ( present( "\"" ) ) {
        uint day;
        if ( nextChar() == ' ' ) {
            space();
            day = number( 1 );
        }
        else {
            day = number( 2 );
        }
        require( "-" );
        String month = letters( 3, 3 );
        require( "-" );
        uint year = number( 4 );
        space();
        uint hour = number( 2 );
        require( ":" );
        uint minute = number( 2 );
        require( ":" );
        uint second = number( 2 );
        space();
        int zone = 1;
        if ( nextChar() == '-' )
            zone = -1;
        else if ( nextChar() != '+' )
            error( Bad, "Time zone must start with + or -" );
        step();
        zone = zone * ( ( 60 * number( 2 ) ) + number( 2 ) );
        require( "\"" );
        space();
        d->date.setDate( year, month, day, hour, minute, second, zone );
        if ( !d->date.valid() )
            error( Bad, "Date supplied is not valid" );
    }

    d->message = new Message( literal() );
    d->message->setInternalDate( d->date.unixTime() );
    if ( !d->message->valid() )
        error( Bad, d->message->error() );
}


/*! This new version of number() demands \a n digits and returns the
    number.
*/

uint Append::number( uint n )
{
    String tmp = digits( n, n );
    return tmp.number( 0 );
}


void Append::execute()
{
    if ( !d->permissions ) {
        d->mailbox = Mailbox::find( imap()->mailboxName( d->mbx ) );
        if ( !d->mailbox ) {
            error( No, "No such mailbox: '" + d->mbx + "'" );
            finish();
            return;
        }

        ImapSession *is = imap()->session();
        if ( is ) {
            d->permissions = is->permissions();
        }
        else {
            d->permissions = new Permissions( d->mailbox, imap()->user(),
                                              this );
        }
    }

    if ( d->permissions && !d->injector ) {
        if ( !d->permissions->ready() )
            return;
        if ( !d->permissions->allowed( Permissions::Insert ) ) {
            error( No, d->mbx + " is not accessible" );
            finish();
            return;
        }
    }

    if ( !d->injector ) {
        SortedList<Mailbox> * m = new SortedList<Mailbox>;
        m->append( d->mailbox );
        d->injector = new Injector( d->message, m, this );
        d->injector->setFlags( d->flags );
        d->injector->execute();
    }

    if ( imap()->session() && !imap()->session()->initialised() ) {
        imap()->session()->refresh( this );
        return;
    }

    if ( d->injector->failed() )
        error( No, "Could not append to " + d->mbx );

    if ( !d->injector->done() || d->injector->failed() )
        return;

    d->injector->announce();
    respond( "OK [APPENDUID " +
             fn( d->mailbox->uidvalidity() ) +
             " " +
             fn( d->injector->uid( d->mailbox ) ) +
             "] done",
             Tagged );

    finish();
}
