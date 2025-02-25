/* Some shared TIFF utilities.
 *
 * 14/10/16
 * 	- from vips2tiff.c
 *
 * 26/8/17
 * 	- add openout_read, to help tiffsave_buffer for pyramids
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
#define DEBUG
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#ifdef HAVE_TIFF

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /*HAVE_UNISTD_H*/
#include <string.h>

#include <vips/vips.h>
#include <vips/internal.h>

#include <tiffio.h>

#include "tiff.h"

/* Handle TIFF errors here. Shared with vips2tiff.c. These can be called from
 * more than one thread.
 */
static void
vips__thandler_error( const char *module, const char *fmt, va_list ap )
{
	vips_verror( module, fmt, ap );
}

/* It'd be nice to be able to support the @fail option for the tiff loader, but
 * there's no easy way to do this, since libtiff has a global warning handler.
 */
static void
vips__thandler_warning( const char *module, const char *fmt, va_list ap )
{
	g_logv( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, fmt, ap );
}

/* Called during library init.
 *
 * libtiff error and warning handlers may be called from other threads 
 * running in other libs. Other libs may install error handlers and capture 
 * messages caused by us.
 */
void
vips__tiff_init( void )
{
	TIFFSetErrorHandler( vips__thandler_error );
	TIFFSetWarningHandler( vips__thandler_warning );
}

/* Open TIFF for output.
 */
TIFF *
vips__tiff_openout( const char *path, gboolean bigtiff )
{
	TIFF *tif;
	const char *mode = bigtiff ? "w8" : "w";

#ifdef DEBUG
	printf( "vips__tiff_openout( \"%s\", \"%s\" )\n", path, mode );
#endif /*DEBUG*/

	/* Need the utf-16 version on Windows.
	 */
#ifdef OS_WIN32
{
	GError *error = NULL;
	wchar_t *path16;

	if( !(path16 = (wchar_t *)
		g_utf8_to_utf16( path, -1, NULL, NULL, &error )) ) {
		vips_g_error( &error );
		return( NULL );
	}

	tif = TIFFOpenW( path16, mode );

	g_free( path16 );
}
#else /*!OS_WIN32*/
	tif = TIFFOpen( path, mode );
#endif /*OS_WIN32*/

	if( !tif ) {
		vips_error( "tiff",
			_( "unable to open \"%s\" for output" ), path );
		return( NULL );
	}

	return( tif );
}

/* TIFF input from a vips stream.
 */

static tsize_t
openin_stream_read( thandle_t st, tdata_t data, tsize_t size )
{
	VipsStreami *streami = VIPS_STREAMI( st );

	return( vips_streami_read( streami, data, size ) );
}

static tsize_t
openin_stream_write( thandle_t st, tdata_t buffer, tsize_t size )
{
	g_assert_not_reached();

	return( 0 );
}

static toff_t
openin_stream_seek( thandle_t st, toff_t position, int whence )
{
	VipsStreami *streami = VIPS_STREAMI( st );

	return( vips_streami_seek( streami, position, whence ) );
}

static int
openin_stream_close( thandle_t st )
{
	VipsStreami *streami = VIPS_STREAMI( st );

	VIPS_UNREF( streami );

	return( 0 );
}

static toff_t
openin_stream_size( thandle_t st )
{
	VipsStreami *streami = VIPS_STREAMI( st );

	/* libtiff will use this to get file size if tags like StripByteCounts
	 * are missing.
	 */
	return( vips_streami_size( streami ) );
}

static int
openin_stream_map( thandle_t st, tdata_t *start, toff_t *len )
{
	g_assert_not_reached();

	return( 0 );
}

static void
openin_stream_unmap( thandle_t st, tdata_t start, toff_t len )
{
	g_assert_not_reached();

	return;
}

TIFF *
vips__tiff_openin_stream( VipsStreami *streami )
{
	TIFF *tiff;

#ifdef DEBUG
	printf( "vips__tiff_openin_stream:\n" );
#endif /*DEBUG*/

	if( vips_streami_rewind( streami ) )
		return( NULL );

	if( !(tiff = TIFFClientOpen( "stream input", "rm",
		(thandle_t) streami,
		openin_stream_read,
		openin_stream_write,
		openin_stream_seek,
		openin_stream_close,
		openin_stream_size,
		openin_stream_map,
		openin_stream_unmap )) ) {
		vips_error( "vips__tiff_openin_stream", "%s",
			_( "unable to open stream for input" ) );
		return( NULL );
	}

	/* Unreffed on close(), see above.
	 */
	g_object_ref( streami );

	return( tiff );
}

/* TIFF output to a memory buffer.
 */

typedef struct _VipsTiffOpenoutBuffer {
	VipsDbuf dbuf;

	/* On close, consolidate and write the output here.
	 */
	void **out_data;
	size_t *out_length;
} VipsTiffOpenoutBuffer;

static tsize_t
openout_buffer_read( thandle_t st, tdata_t data, tsize_t size )
{
	VipsTiffOpenoutBuffer *buffer = (VipsTiffOpenoutBuffer *) st;

#ifdef DEBUG
	printf( "openout_buffer_read: %zd bytes\n", size );
#endif /*DEBUG*/

	return( vips_dbuf_read( &buffer->dbuf, data, size ) );
}

static tsize_t
openout_buffer_write( thandle_t st, tdata_t data, tsize_t size )
{
	VipsTiffOpenoutBuffer *buffer = (VipsTiffOpenoutBuffer *) st;

#ifdef DEBUG
	printf( "openout_buffer_write: %zd bytes\n", size );
#endif /*DEBUG*/

	vips_dbuf_write( &buffer->dbuf, data, size );

	return( size );
}

static int
openout_buffer_close( thandle_t st )
{
	VipsTiffOpenoutBuffer *buffer = (VipsTiffOpenoutBuffer *) st;

	*(buffer->out_data) = vips_dbuf_steal( &buffer->dbuf,
		buffer->out_length);

	return( 0 );
}

static toff_t
openout_buffer_seek( thandle_t st, toff_t position, int whence )
{
	VipsTiffOpenoutBuffer *buffer = (VipsTiffOpenoutBuffer *) st;

#ifdef DEBUG
	printf( "openout_buffer_seek: position %zd, whence %d ",
		position, whence );
	switch( whence ) {
	case SEEK_SET:
		printf( "set" ); 
		break;

	case SEEK_END:
		printf( "end" ); 
		break;

	case SEEK_CUR:
		printf( "cur" ); 
		break;

	default:
		printf( "unknown" ); 
		break;
	}
	printf( "\n" ); 
#endif /*DEBUG*/

	vips_dbuf_seek( &buffer->dbuf, position, whence );

	return( vips_dbuf_tell( &buffer->dbuf ) );
}

static toff_t
openout_buffer_size( thandle_t st )
{
	g_assert_not_reached();

	return( 0 );
}

static int
openout_buffer_map( thandle_t st, tdata_t *start, toff_t *len )
{
	g_assert_not_reached();

	return( 0 );
}

static void
openout_buffer_unmap( thandle_t st, tdata_t start, toff_t len )
{
	g_assert_not_reached();

	return;
}

/* On TIFFClose(), @data and @length are set to point to the output buffer.
 */
TIFF *
vips__tiff_openout_buffer( VipsImage *image,
	gboolean bigtiff, void **out_data, size_t *out_length )
{
	const char *mode = bigtiff ? "w8" : "w";

	VipsTiffOpenoutBuffer *buffer;
	TIFF *tiff;

#ifdef DEBUG
	printf( "vips__tiff_openout_buffer:\n" );
#endif /*DEBUG*/

	g_assert( out_data );
	g_assert( out_length );

	buffer = VIPS_NEW( image, VipsTiffOpenoutBuffer );
	vips_dbuf_init( &buffer->dbuf );
	buffer->out_data = out_data;
	buffer->out_length = out_length;

	if( !(tiff = TIFFClientOpen( "memory output", mode,
		(thandle_t) buffer,
		openout_buffer_read,
		openout_buffer_write,
		openout_buffer_seek,
		openout_buffer_close,
		openout_buffer_size,
		openout_buffer_map,
		openout_buffer_unmap )) ) {
		vips_error( "vips__tiff_openout_buffer", "%s",
			_( "unable to open memory buffer for output" ) );
		return( NULL );
	}

	return( tiff );
}

#endif /*HAVE_TIFF*/

