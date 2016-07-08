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
#ifndef jsoncvt_ptrvec_h
#define jsoncvt_ptrvec_h
#pragma once
#include <stddef.h>

/** ptrvec is just used to make creating pointer-to-pointer lists
 *  (like an argv) easy to build. It automatically manages its memory,
 *  reallocating when necessary, and so on.
 *
 *  Expected usage is something like
 *
 *  1. Obtain new ptrvec via pvnew(), or initialize one to all zeroes.
 *
 *  2. Use pvadd() to add pointers to the vector. The underlying
 *  vector is always null terminated, even while building, so you can
 *  access the in-progress vector safely via p.
 *
 *  3. Use pvdup() to create a new void** that is exactly the size
 *  needed for the resulting string, or use pvfinal() below. The
 *  elements of the vector are just copied; only the vector itself is
 *  allocated anew.
 *
 *  4. Free up any current space space via pvclear(), resetting things
 *  to "empty" again. Set a hard size via pvsize().
 *
 *  5. if you called pvnew() earlier, call pvdel() to free it. If you
 *  just want to free up the memory it uses but not the ptrvec itself,
 *  call pvclear(). pvfinal() combines both pvdup() and pvclear(). */
typedef struct ptrvec {
    /** A table of pointers to anything living here. The number of
     * actual pointers allocated is tracked in sz, and the number of
     * active pointers is tracked in len. */
    void **p;

    /** How many of the pointers at p are in use? */
    size_t len;

    /** How many pointers have been allocated at p? */
    size_t sz;
} ptrvec;

extern ptrvec *pvnew();
extern ptrvec *pvclear( ptrvec * );
extern void **pvfinal( ptrvec * );
extern void pvdel( ptrvec * );
extern void **pvdup( const ptrvec * );
extern ptrvec *pvsize( ptrvec *, size_t );
extern ptrvec *pvadd( ptrvec *, void * );

#endif
