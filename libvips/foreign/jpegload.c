/* load jpeg from a file
 *
 * 24/11/11
 * 	- wrap a class around the jpeg writer
 * 29/11/11
 * 	- split to make load, load from buffer and load from file
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/
#include <vips/intl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <vips/vips.h>
#include <vips/buf.h>
#include <vips/internal.h>

#include "pforeign.h"

#ifdef HAVE_JPEG

#ifdef HAVE_EXIF
#ifdef UNTAGGED_EXIF
#include <exif-data.h>
#include <exif-loader.h>
#include <exif-ifd.h>
#include <exif-utils.h>
#else /*!UNTAGGED_EXIF*/
#include <libexif/exif-data.h>
#include <libexif/exif-loader.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-utils.h>
#endif /*UNTAGGED_EXIF*/
#endif /*HAVE_EXIF*/

typedef struct _VipsForeignLoadJpeg {
	VipsForeignLoad parent_object;

	/* Shrink by this much during load.
	 */
	int shrink;

	/* Autorotate using exif orientation tag.
	 */
	gboolean autorotate;

} VipsForeignLoadJpeg;

typedef VipsForeignLoadClass VipsForeignLoadJpegClass;

G_DEFINE_ABSTRACT_TYPE( VipsForeignLoadJpeg, vips_foreign_load_jpeg, 
	VIPS_TYPE_FOREIGN_LOAD );

static VipsForeignFlags
vips_foreign_load_jpeg_get_flags( VipsForeignLoad *load )
{
	/* The jpeg reader supports sequential read.
	 */
	return( VIPS_FOREIGN_SEQUENTIAL );
}

static int
vips_foreign_load_jpeg_build( VipsObject *object )
{
	VipsForeignLoadJpeg *jpeg = (VipsForeignLoadJpeg *) object;

	if( jpeg->shrink != 1 && 
		jpeg->shrink != 2 && 
		jpeg->shrink != 4 && 
		jpeg->shrink != 8 ) {
		vips_error( "VipsFormatLoadJpeg", 
			_( "bad shrink factor %d" ), jpeg->shrink );
		return( -1 );
	}

	if( VIPS_OBJECT_CLASS( vips_foreign_load_jpeg_parent_class )->
		build( object ) )
		return( -1 );

	return( 0 );
}

static void
vips_foreign_load_jpeg_class_init( VipsForeignLoadJpegClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *object_class = (VipsObjectClass *) class;
	VipsForeignLoadClass *load_class = (VipsForeignLoadClass *) class;

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	object_class->nickname = "jpegload_base";
	object_class->description = _( "load jpeg" );
	object_class->build = vips_foreign_load_jpeg_build;

	load_class->get_flags = vips_foreign_load_jpeg_get_flags;

	VIPS_ARG_INT( class, "shrink", 20, 
		_( "Shrink" ), 
		_( "Shrink factor on load" ),
		VIPS_ARGUMENT_OPTIONAL_INPUT,
		G_STRUCT_OFFSET( VipsForeignLoadJpeg, shrink ),
		1, 16, 1 );

	VIPS_ARG_BOOL( class, "autorotate", 21, 
		_( "Autorotate" ), 
		_( "Rotate image using exif orientation" ),
		VIPS_ARGUMENT_OPTIONAL_INPUT,
		G_STRUCT_OFFSET( VipsForeignLoadJpeg, autorotate ),
		FALSE );

}

static void
vips_foreign_load_jpeg_init( VipsForeignLoadJpeg *jpeg )
{
	jpeg->shrink = 1;
}

typedef struct _VipsForeignLoadJpegStream {
	VipsForeignLoadJpeg parent_object;

	VipsStreami *streami;

} VipsForeignLoadJpegStream;

typedef VipsForeignLoadJpegClass VipsForeignLoadJpegStreamClass;

G_DEFINE_TYPE( VipsForeignLoadJpegStream, vips_foreign_load_jpeg_stream, 
	vips_foreign_load_jpeg_get_type() );

static int
vips_foreign_load_jpeg_stream_header( VipsForeignLoad *load )
{
	VipsForeignLoadJpeg *jpeg = (VipsForeignLoadJpeg *) load;
	VipsForeignLoadJpegStream *stream = (VipsForeignLoadJpegStream *) load;

	if( vips__jpeg_read_stream( stream->streami, 
		load->out, TRUE, jpeg->shrink, load->fail, jpeg->autorotate ) )
		return( -1 );

	return( 0 );
}

static int
vips_foreign_load_jpeg_stream_load( VipsForeignLoad *load )
{
	VipsForeignLoadJpeg *jpeg = (VipsForeignLoadJpeg *) load;
	VipsForeignLoadJpegStream *stream = (VipsForeignLoadJpegStream *) load;

	if( vips__jpeg_read_stream( stream->streami,
		load->real, FALSE, jpeg->shrink, load->fail, 
		jpeg->autorotate ) )
		return( -1 );

	return( 0 );
}

static gboolean
vips_foreign_load_jpeg_stream_is_a( VipsStreami *streami )
{
	return( vips__isjpeg_stream( streami ) );
}

static void
vips_foreign_load_jpeg_stream_class_init( 
	VipsForeignLoadJpegStreamClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *object_class = (VipsObjectClass *) class;
	VipsForeignLoadClass *load_class = (VipsForeignLoadClass *) class;

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	object_class->nickname = "jpegload_stream";
	object_class->description = _( "load image from jpeg stream" );

	load_class->is_a_stream = vips_foreign_load_jpeg_stream_is_a;
	load_class->header = vips_foreign_load_jpeg_stream_header;
	load_class->load = vips_foreign_load_jpeg_stream_load;

	VIPS_ARG_OBJECT( class, "streami", 1,
		_( "Streami" ),
		_( "Stream to load from" ),
		VIPS_ARGUMENT_REQUIRED_INPUT, 
		G_STRUCT_OFFSET( VipsForeignLoadJpegStream, streami ),
		VIPS_TYPE_STREAMI );

}

static void
vips_foreign_load_jpeg_stream_init( VipsForeignLoadJpegStream *stream )
{
}

typedef struct _VipsForeignLoadJpegFile {
	VipsForeignLoadJpeg parent_object;

	char *filename; 

} VipsForeignLoadJpegFile;

typedef VipsForeignLoadJpegClass VipsForeignLoadJpegFileClass;

G_DEFINE_TYPE( VipsForeignLoadJpegFile, vips_foreign_load_jpeg_file, 
	vips_foreign_load_jpeg_get_type() );

static gboolean
vips_foreign_load_jpeg_file_is_a( const char *filename )
{
	VipsStreami *streami;
	gboolean result;

	if( !(streami = vips_streami_new_from_file( filename )) ) 
		return( FALSE );
	result = vips__isjpeg_stream( streami );
	VIPS_UNREF( streami );

	return( result );
}

static int
vips_foreign_load_jpeg_file_header( VipsForeignLoad *load )
{
	VipsForeignLoadJpeg *jpeg = (VipsForeignLoadJpeg *) load;
	VipsForeignLoadJpegFile *file = (VipsForeignLoadJpegFile *) load;

	VipsStreami *streami;

	if( !(streami = vips_streami_new_from_file( file->filename )) ) 
		return( -1 );
	if( vips__jpeg_read_stream( streami, load->out, 
		TRUE, jpeg->shrink, load->fail, jpeg->autorotate ) ) {
		VIPS_UNREF( streami );
		return( -1 );
	}
	VIPS_UNREF( streami );

	return( 0 );
}

static int
vips_foreign_load_jpeg_file_load( VipsForeignLoad *load )
{
	VipsForeignLoadJpeg *jpeg = (VipsForeignLoadJpeg *) load;
	VipsForeignLoadJpegFile *file = (VipsForeignLoadJpegFile *) load;

	VipsStreami *streami;

	if( !(streami = vips_streami_new_from_file( file->filename )) ) 
		return( -1 );
	if( vips__jpeg_read_stream( streami, load->real, 
		FALSE, jpeg->shrink, load->fail, jpeg->autorotate ) ) {
		VIPS_UNREF( streami );
		return( -1 );
	}
	VIPS_UNREF( streami );

	return( 0 );
}

static void
vips_foreign_load_jpeg_file_class_init( VipsForeignLoadJpegFileClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *object_class = (VipsObjectClass *) class;
	VipsForeignClass *foreign_class = (VipsForeignClass *) class;
	VipsForeignLoadClass *load_class = (VipsForeignLoadClass *) class;

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	object_class->nickname = "jpegload";
	object_class->description = _( "load jpeg from file" );

	foreign_class->suffs = vips__jpeg_suffs;

	/* We are fast at is_a(), so high priority.
	 */
	foreign_class->priority = 50;

	load_class->is_a = vips_foreign_load_jpeg_file_is_a;
	load_class->header = vips_foreign_load_jpeg_file_header;
	load_class->load = vips_foreign_load_jpeg_file_load;

	VIPS_ARG_STRING( class, "filename", 1, 
		_( "Filename" ),
		_( "Filename to load from" ),
		VIPS_ARGUMENT_REQUIRED_INPUT, 
		G_STRUCT_OFFSET( VipsForeignLoadJpegFile, filename ),
		NULL );
}

static void
vips_foreign_load_jpeg_file_init( VipsForeignLoadJpegFile *file )
{
}

typedef struct _VipsForeignLoadJpegBuffer {
	VipsForeignLoadJpeg parent_object;

	VipsBlob *blob;

} VipsForeignLoadJpegBuffer;

typedef VipsForeignLoadJpegClass VipsForeignLoadJpegBufferClass;

G_DEFINE_TYPE( VipsForeignLoadJpegBuffer, vips_foreign_load_jpeg_buffer, 
	vips_foreign_load_jpeg_get_type() );

static gboolean
vips_foreign_load_jpeg_buffer_is_a_buffer( const void *buf, size_t len )
{
	VipsStreami *streami;
	gboolean result;

	if( !(streami = vips_streami_new_from_memory( buf, len )) ) 
		return( FALSE );
	result = vips__isjpeg_stream( streami );
	VIPS_UNREF( streami );

	return( result );
}

static int
vips_foreign_load_jpeg_buffer_header( VipsForeignLoad *load )
{
	VipsForeignLoadJpeg *jpeg = (VipsForeignLoadJpeg *) load;
	VipsForeignLoadJpegBuffer *buffer = (VipsForeignLoadJpegBuffer *) load;

	VipsStreami *streami;

	if( !(streami = vips_streami_new_from_blob( buffer->blob )) ) 
		return( -1 );
	if( vips__jpeg_read_stream( streami, load->out, 
		TRUE, jpeg->shrink, load->fail, jpeg->autorotate ) ) {
		VIPS_UNREF( streami );
		return( -1 );
	}
	VIPS_UNREF( streami );

	return( 0 );
}

static int
vips_foreign_load_jpeg_buffer_load( VipsForeignLoad *load )
{
	VipsForeignLoadJpeg *jpeg = (VipsForeignLoadJpeg *) load;
	VipsForeignLoadJpegBuffer *buffer = (VipsForeignLoadJpegBuffer *) load;

	VipsStreami *streami;

	if( !(streami = vips_streami_new_from_blob( buffer->blob )) ) 
		return( -1 );
	if( vips__jpeg_read_stream( streami, load->real, 
		FALSE, jpeg->shrink, load->fail, jpeg->autorotate ) ) {
		VIPS_UNREF( streami );
		return( -1 );
	}
	VIPS_UNREF( streami );

	return( 0 );
}

static void
vips_foreign_load_jpeg_buffer_class_init( 
	VipsForeignLoadJpegBufferClass *class )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( class );
	VipsObjectClass *object_class = (VipsObjectClass *) class;
	VipsForeignLoadClass *load_class = (VipsForeignLoadClass *) class;

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	object_class->nickname = "jpegload_buffer";
	object_class->description = _( "load jpeg from buffer" );

	load_class->is_a_buffer = vips_foreign_load_jpeg_buffer_is_a_buffer;
	load_class->header = vips_foreign_load_jpeg_buffer_header;
	load_class->load = vips_foreign_load_jpeg_buffer_load;

	VIPS_ARG_BOXED( class, "buffer", 1, 
		_( "Buffer" ),
		_( "Buffer to load from" ),
		VIPS_ARGUMENT_REQUIRED_INPUT, 
		G_STRUCT_OFFSET( VipsForeignLoadJpegBuffer, blob ),
		VIPS_TYPE_BLOB );
}

static void
vips_foreign_load_jpeg_buffer_init( VipsForeignLoadJpegBuffer *buffer )
{
}

#endif /*HAVE_JPEG*/

/**
 * vips_jpegload:
 * @filename: file to load
 * @out: (out): decompressed image
 * @...: %NULL-terminated list of optional named arguments
 *
 * Optional arguments:
 *
 * * @shrink: %gint, shrink by this much on load
 * * @fail: %gboolean, fail on errors
 * * @autorotate: %gboolean, rotate image upright during load 
 *
 * Read a JPEG file into a VIPS image. It can read most 8-bit JPEG images, 
 * including CMYK and YCbCr.
 *
 * @shrink means shrink by this integer factor during load.  Possible values 
 * are 1, 2, 4 and 8. Shrinking during read is very much faster than 
 * decompressing the whole image and then shrinking later.
 *
 * Setting @fail to %TRUE makes the JPEG reader fail on any errors. 
 * This can be useful for detecting truncated files, for example. Normally 
 * reading these produces a warning, but no fatal error.  
 *
 * Setting @autorotate to %TRUE will make the loader interpret the 
 * orientation tag and automatically rotate the image appropriately during
 * load. 
 *
 * If @autorotate is %FALSE, the metadata field #VIPS_META_ORIENTATION is set 
 * to the value of the orientation tag. Applications may read and interpret 
 * this field
 * as they wish later in processing. See vips_autorot(). Save
 * operations will use #VIPS_META_ORIENTATION, if present, to set the
 * orientation of output images. 
 *
 * Example:
 *
 * |[
 * vips_jpegload( "fred.jpg", &amp;out,
 * 	"shrink", 8,
 * 	"fail", TRUE,
 * 	NULL );
 * ]|
 *
 * Any embedded ICC profiles are ignored: you always just get the RGB from 
 * the file. Instead, the embedded profile will be attached to the image as 
 * #VIPS_META_ICC_NAME. You need to use something like 
 * vips_icc_import() to get CIE values from the file. 
 *
 * EXIF metadata is attached as #VIPS_META_EXIF_NAME, IPTC as
 * #VIPS_META_IPTC_NAME, and XMP as #VIPS_META_XMP_NAME.
 *
 * The int metadata item "jpeg-multiscan" is set to the result of 
 * jpeg_has_multiple_scans(). Interlaced jpeg images need a large amount of
 * memory to load, so this field gives callers a chance to handle these
 * images differently.
 *
 * The string-valued field "jpeg-chroma-subsample" gives the chroma subsample
 * in standard notation. 4:4:4 means no subsample, 4:2:0 means YCbCr with
 * Cb and Cr subsampled horizontally and vertically, 4:4:4:4 means a CMYK
 * image with no subsampling. 
 *
 * The EXIF thumbnail, if present, is attached to the image as 
 * "jpeg-thumbnail-data". See vips_image_get_blob().
 *
 * See also: vips_jpegload_buffer(), vips_image_new_from_file(), vips_autorot().
 *
 * Returns: 0 on success, -1 on error.
 */
int
vips_jpegload( const char *filename, VipsImage **out, ... )
{
	va_list ap;
	int result;

	va_start( ap, out );
	result = vips_call_split( "jpegload", ap, filename, out );
	va_end( ap );

	return( result );
}

/**
 * vips_jpegload_buffer:
 * @buf: (array length=len) (element-type guint8): memory area to load
 * @len: (type gsize): size of memory area
 * @out: (out): image to write
 * @...: %NULL-terminated list of optional named arguments
 *
 * Optional arguments:
 *
 * * @shrink: %gint, shrink by this much on load
 * * @fail: %gboolean, fail on errors
 * * @autorotate: %gboolean, use exif Orientation tag to rotate the image 
 *   during load
 *
 * Read a JPEG-formatted memory block into a VIPS image. Exactly as
 * vips_jpegload(), but read from a memory buffer. 
 *
 * You must not free the buffer while @out is active. The 
 * #VipsObject::postclose signal on @out is a good place to free. 
 *
 * See also: vips_jpegload().
 *
 * Returns: 0 on success, -1 on error.
 */
int
vips_jpegload_buffer( void *buf, size_t len, VipsImage **out, ... )
{
	va_list ap;
	VipsBlob *blob;
	int result;

	/* We don't take a copy of the data or free it.
	 */
	blob = vips_blob_new( NULL, buf, len );

	va_start( ap, out );
	result = vips_call_split( "jpegload_buffer", ap, blob, out );
	va_end( ap );

	vips_area_unref( VIPS_AREA( blob ) );

	return( result );
}
