// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#ifndef PATRICIATREE_H
#define PATRICIATREE_H

#include "global.h"
#include "allocator.h"


template< class T >
class PatriciaTree
{
public:
    PatriciaTree(): root( 0 ) { }

    class Node
        : public Garbage
    {
    public:
        Node() : zero( 0 ), one( 0 ),
                 parent( 0 ),
                 payload( 0 ), length( 0 ) {}

        uint count() {
            uint c = 0;
            if ( payload )
                c = 1;
            if ( zero )
                c += zero->count();
            if ( one )
                c += one->count();
            return c;
        }

        void * operator new( size_t ownSize, uint extra ) {
            return Allocator::alloc( ownSize + extra, 4 );
        }

    private:
        friend class PatriciaTree;
        Node * zero;
        Node * one;
        Node * parent;
        T * payload;
        uint length;
        char key[1];
    };

    T * find( char * k, uint l ) const {
        Node * n = locate( k, l );
        if ( n )
            return n->payload;
        return 0;
    }

    void remove( char * k, uint l ) {
        Node * n = locate( k, l );
        if ( !n )
            return;

        if ( n->zero || n->one ) {
            // this is an internal node, so we have to drop the
            // payload, then do no more.
            n->payload = 0;
            return;
        }

        if ( !n->parent ) {
            // this is the root
            root = 0;
        }
        else if ( n->parent->payload ) {
            // the parent has to lose this child, but has payload, so
            // it has to stay
            if ( n->parent->zero == n )
                n->parent->zero = 0;
            else
                n->parent->one = 0;
        }
        else {
            // the other child can be promoted to the parent's slot.
            Node * p = n->parent;
            Node * c = p->zero;
            if ( c == n )
                c = p->one;
            if ( p->parent ) {
                // p is an internal node
                if ( p->parent->one == p )
                    p->parent->one = c;
                else
                    p->parent->zero = c;
                if ( c )
                    c->parent = p->parent;
            }
            else {
                // p is the root
                root = c;
                if ( c )
                    c->parent = 0;
            }
            free( p );
        }
        free( n );
    }

    void insert( char * k, uint l, T * t ) {
        Node * n = root;
        bool d;
        uint b = 0;
        while ( n && !d ) {
            // check entire bytes until we run out of something
            while ( !d &&
                    b / 8 < l / 8 &&
                    b / 8 < n->length / 8 ) {
                if ( k[b/8] == n->key[b/8] )
                    b = ( b | 7 ) + 1;
                else
                    d = true;
            }
            // if no entire byte differed, see if the last bits of n
            // differ from k
            if ( !d && b < n->length && n->length <= l ) {
                uint mask = ( 0xff00 >> (n->length%8) ) & 0xff;
                if ( ( k[b/8] & mask ) == ( n->key[b/8] & mask ) )
                    b = n->length;
                else
                    d = true;
            }
            // if we found a difference, then set b to the first
            // differing bit
            if ( d && b < n->length && b < l ) {
                uint mask = 128 >> ( b % 8 );
                while ( b < n->length && b < l &&
                        ( k[b/8] & mask ) == ( n->key[b/8] & mask ) ) {
                    b++;
                    mask >>= 1;
                    if ( !mask )
                        mask = 128;
                }
            }
            // if the first differing bit is at the end of this node,
            // then we need to go to the right child
            if ( b == n->length ) {
                if ( b == l ) {
                    // no, not to the child, n IS the right node
                    n->payload = t;
                    return;
                }
                d = false;
                Node * c = 0;
                if ( k[b / 8] & ( 128 >> ( b % 8 ) ) )
                    c = n->one;
                else
                    c = n->zero;
                if ( c )
                    n = c;
                else
                    d = true;
            }
        }

        uint kl = (l+7) / 8;
        Node * x = node( kl );
        x->length = l;
        x->payload = t;
        uint i = 0;
        while ( i < kl ) {
            x->key[i] = k[i];
            i++;
        }

        if ( !n ) {
            // the tree is empty; x is the new root
            root = x;
        }
        else if ( b == n->length ) {
            // n's key is a prefix of k, so x must be a child of n
            x->parent = n;
            if ( k[b / 8] & ( 128 >> ( b % 8 ) ) )
                n->one = x;
            else
                n->zero = x;
        }
        else {
            // n's key and k differ, so we make a new intermediate node
            kl = (b+7) / 8;
            Node * p = node( kl );
            x->parent = p;
            p->parent = n->parent;
            n->parent = p;
            if ( !p->parent )
                root = p;
            else if ( p->parent->one == n )
                p->parent->one = p;
            else
                p->parent->zero = p;
            if ( k[b / 8] & ( 128 >> ( b % 8 ) ) ) {
                p->zero = n;
                p->one = x;
            }
            else {
                p->zero = x;
                p->one = n;
            }

            p->length = b;
            i = 0;
            while ( i < kl ) {
                p->key[i] = k[i];
                i++;
            }
        }
    }

    uint count() const {
        if ( !root )
            return 0;
        return root->count();
    }

    void clear() {
        root = 0;
    }

    class Iterator
        : public Garbage
    {
    public:
        Iterator()                   { cur = 0; }
        Iterator( Node *n )          { cur = n; }
        Iterator( PatriciaTree<T> * t ) {
            if ( t )
                cur = t->first();
            else
                cur = 0;
        }
        Iterator( const PatriciaTree<T> &t ) {
            cur = t.first();
        }

        operator bool() { return cur && cur->payload ? true : false; }
        operator T *() { return cur ? cur->payload : 0; }
        T *operator ->() { ok(); return cur->payload; }
        Iterator &operator ++() { ok(); return next(); }
        Iterator &operator --() { ok(); return prev(); }
        Iterator &operator ++( int ) {
            ok();
            Node * p = cur; cur = next();
            return newRef(p);
        }

        Iterator &operator --( int ) {
            ok();
            Node *p = cur; cur = prev();
            return newRef(p);
        }


        T &operator *() {
            ok();
            if ( !cur->payload )
                die( Invariant );
            return *(cur->payload);
        }

        bool operator ==( const Iterator &x ) { return cur == x.cur; }
        bool operator !=( const Iterator &x ) { return cur != x.cur; }

        static Iterator &newRef( Node *n ) {
            return *( new Iterator(n) );
        }

    private:
        Iterator &next() {
            do {
                if ( cur->one ) {
                    cur = cur->one;
                    while ( cur->zero )
                        cur = cur->zero;
                }
                else if ( cur->parent ) {
                    while ( cur->parent && cur->parent->one == cur )
                        cur = cur->parent;
                    if ( cur->parent )
                        cur = cur->parent;
                    else
                        cur = 0;
                }
                else {
                    cur = 0;
                }
            } while ( cur && !cur->payload );
            return *this;
        }
        Iterator &prev() {
            do {
                if ( cur->zero ) {
                    cur = cur->zero;
                    while ( cur->one )
                        cur = cur->one;
                }
                else if ( cur->parent ) {
                    while ( cur->parent && cur->parent->zero == cur )
                        cur = cur->parent;
                    if ( cur->parent )
                        cur = cur->parent;
                    else
                        cur = 0;
                }
                else {
                    cur = 0;
                }
            } while ( cur && !cur->payload );
            return *this;
        }

        void ok() {
            if ( !cur )
                die( Invariant );
        }

        Node *cur;
    };

    Node * first() const {
        Node * n = root;
        while ( n && n->zero )
            n = n->zero;
        return n;
    }

private:
    virtual Node * node( uint x ) {
        return new ( x ) Node;
    }
    virtual void free( Node * n ) {
        Allocator::dealloc( n );
    }

    Node * best( char * k, uint l ) const {
        Node * n = root;
        Node * p = n;
        while ( n && n->length < l ) {
            p = n;
            if ( k[n->length / 8] & ( 128 >> ( n->length % 8 ) ) )
                n = n->one;
            else
                n = n->zero;
        }
        if ( n )
            return n;
        return p;
    }

    Node * ifMatch( Node * n, char * k, uint l ) const {
        if ( !n )
            return 0;
        if ( n->length != l )
            return 0;
        uint i = 0;
        while ( i < l / 8 ) {
            if ( n->key[i] != k[i] )
                return 0;
            i++;
        }
        return n;
    }

    Node * locate( char * k, uint l ) const {
        return ifMatch( best( k, l ), k, l );
    }

private:
    Node * root;
};


#endif