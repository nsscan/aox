// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#ifndef ARCHIVEMESSAGE_H
#define ARCHIVEMESSAGE_H

#include "pagecomponent.h"
#include "field.h"


class Message;


class ArchiveMessage
    : public PageComponent
{
public:
    ArchiveMessage( class Link * );

    void execute();

private:
    class ArchiveMessageData * d;

    static String textPlain( const String & );
    static String textHtml( const String & );
    static String address( class Address * );
    static String addressField( Message *, HeaderField::Type );
    static String twoLines( Message * );

    String bodypart( Message *, class Bodypart * );
    String message( Message *, Message * );
    String jsToggle( const String &, bool, const String &, const String & );
};


#endif
