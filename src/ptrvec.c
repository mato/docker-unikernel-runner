/*
 * This file comes from the jsoncvt-1.0.9 distribution, and is covered by
 * the following license:
 *
 * Copyright ⓒ 2014, 2015 Robert S. Krzaczek.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * “Software”), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#define _POSIX_C_SOURCE 200112L
#include <err.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ptrvec.h"

enum {
    /** Ptrvecs start out with space for this many pointers. The number
     *  is pretty much arbitrary; if you think all of your ptrvecs are
     *  going to be extensive, free free to bump this value up to a
     *  bigger initial size to reduce the load on realloc(3). */
    pv_initial_size = 8
};

/** Allocate a new ptrvec from the heap, initialize it as zero, and
 *  return a pointer to it. */
ptrvec *
pvnew()
{
    ptrvec *p = malloc( sizeof( *p ));
    *p = (ptrvec){0};
    return p;
}

/** If the supplied ptrvec has any storage allocated, return it to the
 *  heap. The ptrvec itself is not freed. Whereas pvdel() is useful for
 *  entirely heap-based objects (typically obtained from pvnew()),
 *  pvclear() is useful at tne end of functions that use a stack-based
 *  ptrvec object. */
ptrvec *
pvclear( ptrvec *pv )
{
    if( !pv )
	return 0;
    if( pv->p )
	free( pv->p );
    *pv = (ptrvec){ 0 };
    return pv;
}

/** Return a new void** that is a copy of the one we've been building
 *  in our ptrvec. Unlike our member p, this one will be allocated from
 *  the heap and contains just enough space to hold the current
 *  contents of p including its terminating null. */
void **
pvdup( const ptrvec *pv )
{
    void **v;

    if( !pv ) {
	v = malloc( sizeof( *v ));
	*v = 0;
    } else {
	size_t nb = sizeof( void* ) * ( pv->len + 1 );
	v = malloc( nb );
	if( pv->p )
	    memcpy( v, pv->p, nb );
	else
	    memset( v, 0, nb );
    }

    return v;
}

/** A wrapper for the common case at the end of working with a ptrvec.
 *  Return a null terminated void** ready for storage somewhere, and
 *  kill our own storage so that the next thing to come along can use
 *  our memory. */
void **
pvfinal( ptrvec *pv )
{
    void **v = pvdup( pv );
    pvclear( pv );
    return v;
}

/** Return a ptrvec and its pointers to the head. Once called, the
 *  supplied pointer is <em>no longer valid.</em> Memory at this old
 *  ptrvec is zeroed prior to being freed. */
void
pvdel( ptrvec *pv )
{
    if( pv )
	free( pvclear( pv ));
}

/** Force the supplied ptrvec to contain exactly some number of
 *  pointers. */
ptrvec *
pvsize( ptrvec *pv, size_t sz )
{
    if( !sz )
	return pvclear( pv );

    pv->p = realloc( pv->p, ( pv->sz = sz ) * sizeof( *pv->p ));
    if( pv->len >= pv->sz ) {
	pv->len = pv->sz - 1;
	pv->p[ pv->len ] = 0;
    }
    return pv;
}

/** Ensures that the supplied ptrvec has at least some number of
 *  pointers. If it doesn't, the region of pointers in the ptrvec are
 *  reallocated. Unlike pvsize(), pvensure() grows the ptrvec in a way
 *  that hopefully avoids constant reallocations. */
ptrvec *
pvensure( ptrvec *pv, size_t sz )
{
    size_t newsz;
    
    if( !pv )
	return 0;
    else if( !sz || sz <= pv->sz )
	return pv;
    else if( !pv->sz && sz <= pv_initial_size )
	return pvsize( pv, pv_initial_size );

    /* Choose the next size up for this ptrvec as either 150% of its
       current size, or if that's not big enough, 150% of the
       requested size. Either is meant to add enough padding so that
       we hopefully don't come back here too soon. */
    newsz = pv->sz * 3 / 2;
    if( newsz < sz )
	newsz = sz * 3 / 2;

    /* Imperfect, but should catch most overflows, when newsz has
       rolled past SIZE_MAX. */
    if( newsz < pv->sz )
	errx( 1, "ptrvec overflow" );

    return pvsize( pv, newsz );
}

/** Add a pointer to the end of the set of pointers managed in this
 *  ptrvec. The size of the region is managed. The sz might grow a lot,
 *  but len will only ever grow by one. */
ptrvec *
pvadd( ptrvec *pv, void *v )
{
    pvensure( pv, pv->len + 2 );
    pv->p[ pv->len++ ] = v;
    pv->p[ pv->len ] = 0;
    return pv;
}
