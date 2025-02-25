/* A Streami subclass with signals you can easily hook up to other input
 * sources.
 * 
 * J.Cupitt, 21/11/19
 */

/*

    This file is part of VIPS.
    
    VIPS is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA

 */

/*

    These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define VIPS_DEBUG
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /*HAVE_UNISTD_H*/
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <vips/vips.h>
#include <vips/internal.h>
#include <vips/debug.h>

#include "vipsmarshal.h"

G_DEFINE_TYPE( VipsStreamiu, vips_streamiu, VIPS_TYPE_STREAMI );

/* Our signals. 
 */
enum {
	SIG_SEEK,		
	SIG_READ,		
	SIG_LAST
};

static guint vips_streamiu_signals[SIG_LAST] = { 0 };

static ssize_t
vips_streamiu_read_real( VipsStreami *streami, 
	void *buffer, size_t length )
{
	gint64 bytes_read;

	VIPS_DEBUG_MSG( "vips_streamiu_read_real:\n" );

	/* Return this value (error) if there's no attached handler.
	 */
	bytes_read = 0;

	g_signal_emit( streami, vips_streamiu_signals[SIG_READ], 0,
		buffer, length, &bytes_read );

	VIPS_DEBUG_MSG( "  %zd\n", bytes_read );

	return( bytes_read );
}

static gint64
vips_streamiu_seek_real( VipsStreami *streami, 
	gint64 offset, int whence )
{
	gint64 new_position;

	VIPS_DEBUG_MSG( "vips_streamiu_seek_real:\n" );

	/* If there's no user action attached, we fail.
	 */
	new_position = -1;

	g_signal_emit( streami, vips_streamiu_signals[SIG_SEEK], 0,
		offset, whence, &new_position );

	VIPS_DEBUG_MSG( "  %zd\n", new_position );

	return( new_position );
}

static gint64
vips_streamiu_read_signal_real( VipsStreamiu *streamiu, 
	void *data, gint64 length )
{
	VIPS_DEBUG_MSG( "vips_streamiu_read_signal_real:\n" );

	return( 0 );
}

static gint64
vips_streamiu_seek_signal_real( VipsStreamiu *streamiu, 
	gint64 offset, int whence )
{
	VIPS_DEBUG_MSG( "vips_streamiu_seek_signal_real:\n" );

	return( -1 );
}

static void
vips_streamiu_class_init( VipsStreamiuClass *class )
{
	VipsObjectClass *object_class = VIPS_OBJECT_CLASS( class );
	VipsStreamiClass *streami_class = VIPS_STREAMI_CLASS( class );

	object_class->nickname = "streamiu";
	object_class->description = _( "input stream" );

	streami_class->read = vips_streamiu_read_real;
	streami_class->seek = vips_streamiu_seek_real;

	class->read = vips_streamiu_read_signal_real;
	class->seek = vips_streamiu_seek_signal_real;

	/**
	 * VipsStreamiu::read:
	 * @streamiu: the stream being operated on
	 * @buffer: %gpointer, buffer to fill
	 * @size: %gint64, size of buffer
	 *
	 * This signal is emitted to read bytes from the source into @buffer.
	 *
	 * Returns: the number of bytes read.
	 */
	vips_streamiu_signals[SIG_READ] = g_signal_new( "read",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_ACTION,
		G_STRUCT_OFFSET( VipsStreamiuClass, read ), 
		NULL, NULL,
		vips_INT64__INT64_INT,
		G_TYPE_INT64, 2,
		G_TYPE_INT64, G_TYPE_INT );

	/**
	 * VipsStreamiu::seek:
	 * @streamiu: the stream being operated on
	 * @offset: %gint64, seek offset
	 * @whence: %gint, seek origin
	 *
	 * This signal is emitted to seek the stream. The handler should
	 * change the stream position appropriately.
	 *
	 * The handler on an unseekable stream should always return -1.
	 *
	 * Returns: the new seek position.
	 */
	vips_streamiu_signals[SIG_SEEK] = g_signal_new( "seek",
		G_TYPE_FROM_CLASS( class ),
		G_SIGNAL_ACTION,
		G_STRUCT_OFFSET( VipsStreamiuClass, seek ), 
		NULL, NULL,
		vips_INT64__POINTER_INT64,
		G_TYPE_INT64, 2,
		G_TYPE_POINTER, G_TYPE_INT64 );

}

static void
vips_streamiu_init( VipsStreamiu *streamiu )
{
}

/**
 * vips_streamiu_new:
 *
 * Create a #VipsStreamiu. Attach signals to implement read and seek.
 *
 * Returns: a new #VipsStreamiu
 */
VipsStreamiu *
vips_streamiu_new( void )
{
	VipsStreamiu *streamiu;

	VIPS_DEBUG_MSG( "vips_streamiu_new:\n" );

	streamiu = VIPS_STREAMIU( g_object_new( VIPS_TYPE_STREAMIU, NULL ) );

	if( vips_object_build( VIPS_OBJECT( streamiu ) ) ) {
		VIPS_UNREF( streamiu );
		return( NULL );
	}

	return( streamiu ); 
}
