// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#include "fieldcache.h"

#include "transaction.h"
#include "allocator.h"
#include "event.h"
#include "query.h"
#include "dict.h"
#include "list.h"
#include "map.h"


static Map< String > *fieldsById;
static Dict< uint > *fieldsByName;
static PreparedStatement *fieldLookup;
static PreparedStatement *fieldInsert;


/*! \class FieldNameCache fieldcache.h
    This class maintains a cache of the field_names table.

    This class is responsible for finding the numeric id corresponding
    to the name of a header field in the field_names table. It may use
    its in-memory cache to find the id, SELECT a row from field_names,
    or, failing that, INSERT a new row and retrieve its id.

    (...We want to describe id lookups here...)

    This class is used only by the Injector at present.
*/

/*! This function initialises the cache of field names at startup.
    It expects to be called from ::main().
*/

void FieldNameCache::setup()
{
    fieldsById = new Map< String >;
    fieldsByName = new Dict< uint >( 400 );

    fieldLookup =
        new PreparedStatement( "select id from field_names where name=$1" );

    fieldInsert =
        new PreparedStatement( "insert into field_names (name) "
                               "select $1 where not exists "
                               "(select id from field_names where name=$1)" );

    Allocator::addEternal( fieldsById, "field name cache by id" );
    Allocator::addEternal( fieldsByName, "field name cache by name" );
    Allocator::addEternal( fieldLookup, "field name lookup statement" );
    Allocator::addEternal( fieldInsert, "field inserter" );
}


class FieldLookup
    : public EventHandler
{
protected:
    Query *i, *q;
    String field;
    CacheLookup *status;
    EventHandler *owner;
    List< Query > *queries;
    Transaction *transaction;

public:
    FieldLookup() {}
    FieldLookup( Transaction *t, const String &f, List< Query > *l,
                 CacheLookup *st, EventHandler *ev )
        : field( f ), status( st ), owner( ev ), queries( l ),
          transaction( t )
    {
        i = new Query( *fieldInsert, this );
        i->bind( 1, field );
        transaction->enqueue( i );

        q = new Query( *fieldLookup, this );
        q->bind( 1, field );
        transaction->enqueue( q );
        l->append( q );
    }
    virtual ~FieldLookup() {}

    void execute();
};


void FieldLookup::execute() {
    if ( !i->done() || !q->done() )
        return;

    queries->remove( q );

    Row *r = q->nextRow();
    if ( r ) {
        uint id = r->getInt( "id" );
        String *name = new String( field );
        name->detach();
        fieldsById->insert( id, name );
        uint * tmp = (uint*)Allocator::alloc( sizeof(uint), 0 );
        *tmp = id;
        fieldsByName->insert( *name, tmp );
    }

    if ( queries->isEmpty() ) {
        status->setState( CacheLookup::Completed );
        owner->execute();
    }
}


/*! This function takes a List \a l of field names, and notifies \a ev
    after it has updated its cache for each field therein. The caller
    may then use translate() to retrieve the id.

    Any required queries will be run in the Transaction \a t.
*/

CacheLookup *FieldNameCache::lookup( Transaction *t, List< String > *l,
                                     EventHandler *ev )
{
    CacheLookup *status = new CacheLookup;
    List< Query > *lookups = new List< Query >;

    List< String >::Iterator it( l );
    while ( it ) {
        String field = *it;

        if ( fieldsByName->find( field ) == 0 )
            (void)new FieldLookup( t, field, lookups, status, ev );

        ++it;
    }

    if ( lookups->isEmpty() )
        status->setState( CacheLookup::Completed );
    else
        t->execute();

    return status;
}


/*! This static function returns the numeric id corresponding to the
    specified \a field name. If \a field is not cached, translate()
    returns 0.
*/

uint FieldNameCache::translate( const String &field )
{
    HeaderField::Type * t = (HeaderField::Type *)fieldsByName->find( field );
    if ( !t )
        return 0;
    return *t;
}


/*! Inserts a field with \a name and \a id into the cache. This function
    exists for the Injector to use.
*/

void FieldNameCache::insert( const String & name, uint id )
{
    uint * tmp = (uint*)Allocator::alloc( sizeof(uint), 0 );
    String * n = new String( name );
    n->detach();
    *tmp = id;
    fieldsById->insert( id, n );
    fieldsByName->insert( *n, tmp );
}