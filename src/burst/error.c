/*                         E R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file error.c
 *	Author:		Gary S. Moss
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
/*
 *			E R R O R
 *
 *  Ray Tracing library and Framebuffer library, error handling routines.
 *
 *  Functions -
 *	brst_log		Called to log RT library events.
 *	fb_log		Called to log FB library events.
 *
 *	Idea originated by Mike John Muuss
 */

#include <stdio.h>
#include <signal.h>
#include "./Sc.h"
#include "./extern.h"


/*
 *  		B R S T _  L O G
 *
 *  Log an RT library event
 */
/* VARARGS */
void
#if defined(HAVE_STDARG_H)
brst_log( char *fmt, ... )
	{
	va_list	ap;
	va_start( ap, fmt );
#else
brst_log( va_alist )
va_dcl
	{
	register char *fmt; /* picked up by va_arg() */
	va_list	ap;
	va_start( ap );
	fmt  = va_arg( ap, char * );
#endif
	if( tty && (errfile[0] == '\0' || ! strcmp( errfile, "/dev/tty" )) )
		{
		clr_Tabs( HmTtyFd );
		if( ScDL != NULL )
			{
			(void) ScMvCursor( 1, SCROLL_TOP );
			(void) ScDeleteLn();
			(void) ScMvCursor( 1, SCROLL_BTM );
			(void) ScClrEOL();
			(void) vprintf( fmt, ap );
			}
		else
		if( ScSetScrlReg( SCROLL_TOP, SCROLL_BTM+1 ) )
			{	char buf[LNBUFSZ];
				char *p;
			(void) ScMvCursor( 1, SCROLL_BTM+1 );
			(void) ScClrEOL();
			/* Work around for problem with vprintf(): it doesn't
				cause the screen to scroll, don't know why. */
			(void) vsprintf( buf, fmt, ap );
			/* Newline will cause double scroll. */
			p = buf+strlen(buf)-1;
			if( *p == '\n' )
				*p = '\0'; /* clobber newline */
			(void) puts( buf );
			/*(void) vprintf( fmt, ap );*/
			(void) ScMvCursor( 1, SCROLL_BTM+1 );
			(void) ScClrScrlReg();
			}
		else
			{
			(void) fprintf( stderr,
					"%s %s %s %s!\n",
					"This terminal has no delete line",
					"or scrolling region capability,",
					"please dump it somewhere and get",
					"a real terminal"
					);
			exit( 1 );
			}
		(void) fflush( stdout );
		}
	else
		{
		(void) vfprintf( stderr, fmt, ap );
		}
	va_end( ap );
	}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
