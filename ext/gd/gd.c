/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_01.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Rasmus Lerdorf <rasmus@lerdorf.on.ca>                       |
   |          Stig Bakken <ssb@guardian.no>                               |
   |          Jim Winstead <jimw@php.net>                                 |
   +----------------------------------------------------------------------+
 */

/* $Id$ */

/* gd 1.2 is copyright 1994, 1995, Quest Protein Database Center, 
   Cold Spring Harbor Labs. */

/* Note that there is no code from the gd package in this file */

#include "php.h"
#include "ext/standard/head.h"
#include <math.h>
#include "SAPI.h"
#include "php_gd.h"

#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if WIN32|WINNT
# include <io.h>
# include <fcntl.h>
#endif

#if HAVE_LIBGD
#include <gd.h>
#include <gdfontt.h>  /* 1 Tiny font */
#include <gdfonts.h>  /* 2 Small font */
#include <gdfontmb.h> /* 3 Medium bold font */
#include <gdfontl.h>  /* 4 Large font */
#include <gdfontg.h>  /* 5 Giant font */
#ifdef ENABLE_GD_TTF
# include "gdttf.h"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef ENABLE_GD_TTF
static void php_imagettftext_common(INTERNAL_FUNCTION_PARAMETERS, int);
#endif

#ifdef THREAD_SAFE
DWORD GDlibTls;
static int numthreads=0;
void *gdlib_mutex=NULL;

/*
typedef struct gdlib_global_struct{
	int le_gd;
	int le_gd_font;
#if HAVE_LIBT1
	int le_ps_font;
	int le_ps_enc;
#endif
} gdlib_global_struct;

# define GD_GLOBAL(a) gdlib_globals->a
# define GD_TLS_VARS gdlib_global_struct *gdlib_globals = TlsGetValue(GDlibTls);

#else
#  define GD_GLOBAL(a) a
#  define GD_TLS_VARS
int le_gd;
int le_gd_font;
#if HAVE_LIBT1
int le_ps_font;
int le_ps_enc;
#endif
*/
#endif

function_entry gd_functions[] = {
	PHP_FE(imagearc,								NULL)
	PHP_FE(imagechar,								NULL)
	PHP_FE(imagecharup,								NULL)
	PHP_FE(imagecolorallocate,						NULL)
	PHP_FE(imagecolorat,							NULL)
	PHP_FE(imagecolorclosest,						NULL)
	PHP_FE(imagecolordeallocate,					NULL)
	PHP_FE(imagecolorresolve,						NULL)
	PHP_FE(imagecolorexact,							NULL)
	PHP_FE(imagecolorset,							NULL)
	PHP_FE(imagecolortransparent,					NULL)
	PHP_FE(imagecolorstotal,						NULL)
	PHP_FE(imagecolorsforindex,						NULL)
	PHP_FE(imagecopy,								NULL)
	PHP_FE(imagecopyresized,						NULL)
	PHP_FE(imagecreate,								NULL)
#ifdef HAVE_GD_PNG
	PHP_FE(imagecreatefrompng,						NULL)
	PHP_FE(imagepng,								NULL)
#endif
#ifdef HAVE_GD_GIF
	PHP_FE(imagecreatefromgif,						NULL)
	PHP_FE(imagegif,								NULL)
#endif
	PHP_FE(imagedestroy,							NULL)
	PHP_FE(imagefill,								NULL)
	PHP_FE(imagefilledpolygon,						NULL)
	PHP_FE(imagefilledrectangle,					NULL)
	PHP_FE(imagefilltoborder,						NULL)
	PHP_FE(imagefontwidth,							NULL)
	PHP_FE(imagefontheight,							NULL)
	PHP_FE(imageinterlace,							NULL)
	PHP_FE(imageline,								NULL)
	PHP_FE(imageloadfont,							NULL)
	PHP_FE(imagepolygon,							NULL)
	PHP_FE(imagerectangle,							NULL)
	PHP_FE(imagesetpixel,							NULL)
	PHP_FE(imagestring,								NULL)
	PHP_FE(imagestringup,							NULL)
	PHP_FE(imagesx,									NULL)
	PHP_FE(imagesy,									NULL)
	PHP_FE(imagedashedline,							NULL)
#ifdef ENABLE_GD_TTF
	PHP_FE(imagettfbbox,							NULL)
	PHP_FE(imagettftext,							NULL)
#endif
#if HAVE_LIBT1
	PHP_FE(imagepsloadfont,							NULL)
	/*
	PHP_FE(imagepscopyfont,							NULL)
	*/
	PHP_FE(imagepsfreefont,							NULL)
	PHP_FE(imagepsencodefont,						NULL)
	PHP_FE(imagepsextendfont,						NULL)
	PHP_FE(imagepsslantfont,						NULL)
	PHP_FE(imagepstext,								NULL)
	PHP_FE(imagepsbbox,								NULL)
#endif
	{NULL, NULL, NULL}
};

zend_module_entry gd_module_entry = {
	"gd", gd_functions, PHP_MINIT(gd), PHP_MSHUTDOWN(gd), NULL, NULL, PHP_MINFO(gd), STANDARD_MODULE_PROPERTIES
};

#ifdef ZTS
int gd_globals_id;
#else
static php_gd_globals gd_globals;
#endif

#ifdef COMPILE_DL_GD
# include "dl/phpdl.h"
DLEXPORT zend_module_entry *get_module(void) { return &gd_module_entry; }
#endif


#define PolyMaxPoints 256

static void php_free_gd_font(gdFontPtr fp)
{
	if (fp->data) {
		efree(fp->data);
	}
	efree(fp);
}



PHP_MINIT_FUNCTION(gd)
{
	GDLS_FETCH();

#if defined(THREAD_SAFE)
	gdlib_global_struct *gdlib_globals;
	PHP_MUTEX_ALLOC(gdlib_mutex);
	PHP_MUTEX_LOCK(gdlib_mutex);
	numthreads++;
	if (numthreads==1){
		if (!PHP3_TLS_PROC_STARTUP(GDlibTls)){
			PHP_MUTEX_UNLOCK(gdlib_mutex);
			PHP_MUTEX_FREE(gdlib_mutex);
			return FAILURE;
		}
	}
	PHP_MUTEX_UNLOCK(gdlib_mutex);
	if(!PHP3_TLS_THREAD_INIT(GDlibTls,gdlib_globals,gdlib_global_struct)){
		PHP_MUTEX_FREE(gdlib_mutex);
		return FAILURE;
	}
#endif
	GDG(le_gd) = register_list_destructors(gdImageDestroy, NULL);
	GDG(le_gd_font) = register_list_destructors(php_free_gd_font, NULL);
#if HAVE_LIBT1
	T1_SetBitmapPad(8);
	T1_InitLib(NO_LOGFILE|IGNORE_CONFIGFILE|IGNORE_FONTDATABASE);
	T1_SetLogLevel(T1LOG_DEBUG);
	GDG(le_ps_font) = register_list_destructors(php_free_ps_font, NULL);
	GDG(le_ps_enc) = register_list_destructors(php_free_ps_enc, NULL);
#endif
	return SUCCESS;
}

PHP_MINFO_FUNCTION(gd)
{
	/* need to use a PHPAPI function here because it is external module in windows */
#if HAVE_GDIMAGECOLORRESOLVE
	php_printf("Version 1.6.2 or higher");
#elif HAVE_LIBGD13
	php_printf("Version between 1.3 and 1.6.1");
#else
	php_printf("Version 1.2");
#endif

#ifdef ENABLE_GD_TTF
	php_printf(" with FreeType support");
#if HAVE_LIBFREETYPE
	php_printf(" (linked with freetype)");
#elif HAVE_LIBTTF
	php_printf(" (linked with ttf library)");
#else
	php_printf(" (linked with unknown library)");
#endif
#endif

   php_printf(" which supports:");
   
#ifdef HAVE_GD_GIF
	php_printf(" GIF");
#endif
#ifdef HAVE_GD_PNG
	php_printf(" PNG");
#endif
}

PHP_MSHUTDOWN_FUNCTION(gd)
{
	GDLS_FETCH();

#ifdef THREAD_SAFE
	PHP3_TLS_THREAD_FREE(gdlib_globals);
	PHP_MUTEX_LOCK(gdlib_mutex);
	numthreads--;
	if (numthreads<1) {
		PHP3_TLS_PROC_SHUTDOWN(GDlibTls);
		PHP_MUTEX_UNLOCK(gdlib_mutex);
		PHP_MUTEX_FREE(gdlib_mutex);
		return SUCCESS;
	}
	PHP_MUTEX_UNLOCK(gdlib_mutex);
#endif
	return SUCCESS;
}

/* Need this for cpdf. See also comment in file.c php3i_get_le_fp() */
PHPAPI int phpi_get_le_gd(void)
{
	GDLS_FETCH();

	return GDG(le_gd);
}

#ifndef HAVE_GDIMAGECOLORRESOLVE

/********************************************************************/
/* gdImageColorResolve is a replacement for the old fragment:       */
/*                                                                  */
/*      if ((color=gdImageColorExact(im,R,G,B)) < 0)                */
/*        if ((color=gdImageColorAllocate(im,R,G,B)) < 0)           */
/*          color=gdImageColorClosest(im,R,G,B);                    */
/*                                                                  */
/* in a single function                                             */

int
gdImageColorResolve(gdImagePtr im, int r, int g, int b)
{
	int c;
	int ct = -1;
	int op = -1;
	long rd, gd, bd, dist;
	long mindist = 3*255*255;  /* init to max poss dist */

	for (c = 0; c < im->colorsTotal; c++) {
		if (im->open[c]) {
			op = c;             /* Save open slot */
			continue;           /* Color not in use */
		}
		rd = (long)(im->red  [c] - r);
		gd = (long)(im->green[c] - g);
		bd = (long)(im->blue [c] - b);
		dist = rd * rd + gd * gd + bd * bd;
		if (dist < mindist) {
			if (dist == 0) {
				return c;       /* Return exact match color */
			}
			mindist = dist;
			ct = c;
		}
	}
	/* no exact match.  We now know closest, but first try to allocate exact */
	if (op == -1) {
		op = im->colorsTotal;
		if (op == gdMaxColors) {    /* No room for more colors */
			return ct;          /* Return closest available color */
		}
		im->colorsTotal++;
	}
	im->red  [op] = r;
	im->green[op] = g;
	im->blue [op] = b;
	im->open [op] = 0;
	return op;                  /* Return newly allocated color */
}

#endif

/* {{{ proto int imageloadfont(string filename)
   Load a new font */
PHP_FUNCTION(imageloadfont) {
	zval **file;
	int hdr_size = sizeof(gdFont) - sizeof(char *);
	int ind, body_size, n=0, b;
	gdFontPtr font;
	FILE *fp;
	int issock=0, socketd=0;
	GDLS_FETCH();


	if (ARG_COUNT(ht) != 1 || zend_get_parameters_ex(1, &file) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(file);

#if WIN32|WINNT
	fp = fopen((*file)->value.str.val, "rb");
#else
	fp = php_fopen_wrapper((*file)->value.str.val, "r", IGNORE_PATH|IGNORE_URL_WIN, &issock, &socketd, NULL);
#endif
	if (fp == NULL) {
		php_error(E_WARNING, "ImageFontLoad: unable to open file");
		RETURN_FALSE;
	}

	/* Only supports a architecture-dependent binary dump format
	 * at the moment.
	 * The file format is like this on machines with 32-byte integers:
	 *
	 * byte 0-3:   (int) number of characters in the font
	 * byte 4-7:   (int) value of first character in the font (often 32, space)
	 * byte 8-11:  (int) pixel width of each character
	 * byte 12-15: (int) pixel height of each character
	 * bytes 16-:  (char) array with character data, one byte per pixel
	 *                    in each character, for a total of 
	 *                    (nchars*width*height) bytes.
	 */
	font = (gdFontPtr)emalloc(sizeof(gdFont));
	b = 0;
	while (b < hdr_size && (n = fread(&font[b], 1, hdr_size - b, fp)))
		b += n;
	if (!n) {
		fclose(fp);
		efree(font);
		if (feof(fp)) {
			php_error(E_WARNING, "ImageFontLoad: end of file while reading header");
		} else {
			php_error(E_WARNING, "ImageFontLoad: error while reading header");
		}
		RETURN_FALSE;
	}
	body_size = font->w * font->h * font->nchars;
	font->data = emalloc(body_size);
	b = 0;
	while (b < body_size && (n = fread(&font->data[b], 1, body_size - b, fp)))
		b += n;
	if (!n) {
		fclose(fp);
		efree(font->data);
		efree(font);
		if (feof(fp)) {
			php_error(E_WARNING, "ImageFontLoad: end of file while reading body");
		} else {
			php_error(E_WARNING, "ImageFontLoad: error while reading body");
		}
		RETURN_FALSE;
	}
	fclose(fp);

	/* Adding 5 to the font index so we will never have font indices
	 * that overlap with the old fonts (with indices 1-5).  The first
	 * list index given out is always 1.
	 */
	ind = 5 + zend_list_insert(font, GDG(le_gd_font));

	RETURN_LONG(ind);
}
/* }}} */

/* {{{ proto int imagecreate(int x_size, int y_size)
   Create a new image */
PHP_FUNCTION(imagecreate)
{
	zval **x_size, **y_size;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 2 || zend_get_parameters_ex(2, &x_size, &y_size) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long_ex(x_size);
	convert_to_long_ex(y_size);

	im = gdImageCreate((*x_size)->value.lval, (*y_size)->value.lval);

	ZEND_REGISTER_RESOURCE(return_value, im, GDG(le_gd));
}
/* }}} */

#ifdef HAVE_GD_PNG

/* {{{ proto int imagecreatefrompng(string filename)
   Create a new image from file or URL */
PHP_FUNCTION(imagecreatefrompng)
{
	zval **file;
	gdImagePtr im;
	char *fn=NULL;
	FILE *fp;
	int issock=0, socketd=0;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 1 || zend_get_parameters_ex(1, &file) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_string_ex(file);
	fn = (*file)->value.str.val;
#if WIN32|WINNT
	fp = fopen((*file)->value.str.val, "rb");
#else
	fp = php_fopen_wrapper((*file)->value.str.val, "r", IGNORE_PATH|IGNORE_URL_WIN, &issock, &socketd, NULL);
#endif
	if (!fp) {
		php_strip_url_passwd(fn);
		php_error(E_WARNING,
				  "ImageCreateFromPng: Unable to open %s for reading", fn);
		RETURN_FALSE;
	}
	im = gdImageCreateFromPng(fp);
	fflush(fp);
	fclose(fp);

	ZEND_REGISTER_RESOURCE(return_value, im, GDG(le_gd));
}
/* }}} */

/* {{{ proto int imagepng(int im [, string filename])
   Output image to browser or file */
PHP_FUNCTION(imagepng)
{
	zval **imgind, **file;
	gdImagePtr im;
	char *fn=NULL;
	FILE *fp;
	int argc;
	int output=1;
	GDLS_FETCH();

	argc = ARG_COUNT(ht);
	if (argc < 1 || argc > 2 || zend_get_parameters_ex(argc, &imgind, &file) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	if (argc == 2) {
		convert_to_string_ex(file);
		fn = (*file)->value.str.val;
		if (!fn || fn == empty_string || php_check_open_basedir(fn)) {
			php_error(E_WARNING, "ImagePng: Invalid filename");
			RETURN_FALSE;
		}
	}

	if (argc == 2) {
		fp = fopen(fn, "wb");
		if (!fp) {
			php_error(E_WARNING, "ImagePng: unable to open %s for writing", fn);
			RETURN_FALSE;
		}
		gdImagePng(im,fp);
		fflush(fp);
		fclose(fp);
	}
	else {
		int   b;
		FILE *tmp;
		char  buf[4096];
		tmp = tmpfile();
		if (tmp == NULL) {
			php_error(E_WARNING, "Unable to open temporary file");
			RETURN_FALSE;
		}
		output = php_header();
		if (output) {
			gdImagePng(im, tmp);
            fseek(tmp, 0, SEEK_SET);
#if APACHE && defined(CHARSET_EBCDIC)
			SLS_FETCH();
            /* This is a binary file already: avoid EBCDIC->ASCII conversion */
            ap_bsetflag(php3_rqst->connection->client, B_EBCDIC2ASCII, 0);
#endif
            while ((b = fread(buf, 1, sizeof(buf), tmp)) > 0) {
                php_write(buf, b);
            }
        }
        fclose(tmp);
        /* the temporary file is automatically deleted */
    }
    RETURN_TRUE;
}
/* }}} */

#endif /* HAVE_GD_PNG */

#ifdef HAVE_GD_GIF

/* {{{ proto int imagecreatefromgif(string filename)
   Create a new image from file or URL */
PHP_FUNCTION(imagecreatefromgif )
{
	zval **file;
	gdImagePtr im;
	char *fn=NULL;
	FILE *fp;
	int issock=0, socketd=0;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 1 || zend_get_parameters_ex(1, &file) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(file);

	fn = (*file)->value.str.val;

#if WIN32|WINNT
	fp = fopen((*file)->value.str.val, "rb");
#else
	fp = php_fopen_wrapper((*file)->value.str.val, "r", IGNORE_PATH|IGNORE_URL_WIN, &issock, &socketd, NULL);
#endif
	if (!fp) {
		php_strip_url_passwd(fn);
		php_error(E_WARNING,
				  "ImageCreateFromGif: Unable to open %s for reading", fn);
		RETURN_FALSE;
	}

	im = gdImageCreateFromGif(fp);
	
	fflush(fp);
	fclose(fp);

	ZEND_REGISTER_RESOURCE(return_value, im, GDG(le_gd));
}
/* }}} */

/* {{{ proto int imagegif(int im [, string filename])
   Output image to browser or file */
PHP_FUNCTION(imagegif)
{
	zval **imgind, **file;
	gdImagePtr im;
	char *fn=NULL;
	FILE *fp;
	int argc;
	int output=1;
	GDLS_FETCH();

	argc = ARG_COUNT(ht);
	if (argc < 1 || argc > 2 || zend_get_parameters_ex(argc, &imgind, &file) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	if (argc == 2) {
		convert_to_string_ex(file);
		fn = (*file)->value.str.val;
		if (!fn || fn == empty_string || php_check_open_basedir(fn)) {
			php_error(E_WARNING, "ImageGif: Invalid filename");
			RETURN_FALSE;
		}
	}

	if (argc == 2) {
		fp = fopen(fn, "wb");
		if (!fp) {
			php_error(E_WARNING, "ImageGif: unable to open %s for writing", fn);
			RETURN_FALSE;
		}
		gdImageGif(im, fp);
		fflush(fp);
		fclose(fp);
	}
	else {
		int   b;
		FILE *tmp;
		char  buf[4096];

		tmp = tmpfile();
		if (tmp == NULL) {
			php_error(E_WARNING, "Unable to open temporary file");
			RETURN_FALSE;
		}

		output = php_header();

		if (output) {
			gdImageGif(im, tmp);
			fseek(tmp, 0, SEEK_SET);
#if APACHE && defined(CHARSET_EBCDIC)
			{
				SLS_FETCH();
				/* This is a binary file already: avoid EBCDIC->ASCII conversion */
				ap_bsetflag(((request_rec *) SG(server_context))->connection->client, B_EBCDIC2ASCII, 0);
			}
#endif
			while ((b = fread(buf, 1, sizeof(buf), tmp)) > 0) {
				php_write(buf, b);
			}
		}

		fclose(tmp);
		/* the temporary file is automatically deleted */
	}

	RETURN_TRUE;
}
/* }}} */

#endif /* HAVE_GD_GIF */

/* {{{ proto int imagedestroy(int im)
   Destroy an image */
PHP_FUNCTION(imagedestroy)
{
	zval **imgind;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 1 || zend_get_parameters_ex(1, &imgind) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	zend_list_delete((*imgind)->value.lval);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int imagecolorallocate(int im, int red, int green, int blue)
   Allocate a color for an image */
PHP_FUNCTION(imagecolorallocate)
{
	zval **imgind, **red, **green, **blue;
	int col;
	int r, g, b;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 4 || zend_get_parameters_ex(4, &imgind, &red,
											&green, &blue) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	convert_to_long_ex(red);
	convert_to_long_ex(green);
	convert_to_long_ex(blue);
	
	r = (*red)->value.lval;
	g = (*green)->value.lval;
	b = (*blue)->value.lval;
	
	col = gdImageColorAllocate(im, r, g, b);
	RETURN_LONG(col);
}
/* }}} */

/* im, x, y */
/* {{{ proto int imagecolorat(int im, int x, int y)
   Get the index of the color of a pixel */
PHP_FUNCTION(imagecolorat)
{
	zval **imgind, **x, **y;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 3 || zend_get_parameters_ex(3, &imgind, &x, &y) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));
	convert_to_long_ex(x);
	convert_to_long_ex(y);

	if (gdImageBoundsSafe(im, (*x)->value.lval, (*y)->value.lval)) {
#if HAVE_LIBGD13
		RETURN_LONG(im->pixels[(*y)->value.lval][(*x)->value.lval]);
#else
		RETURN_LONG(im->pixels[(*x)->value.lval][(*y)->value.lval]);
#endif
	}
	else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto int imagecolorclosest(int im, int red, int green, int blue)
   Get the index of the closest color to the specified color */
PHP_FUNCTION(imagecolorclosest)
{
	zval **imgind, **red, **green, **blue;
	int col;
	int r, g, b;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 4 || zend_get_parameters_ex(4, &imgind, &red,
													 &green, &blue) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	convert_to_long_ex(red);
	convert_to_long_ex(green);
	convert_to_long_ex(blue);
	
	r = (*red)->value.lval;
	g = (*green)->value.lval;
	b = (*blue)->value.lval;
	
	col = gdImageColorClosest(im, r, g, b);
	RETURN_LONG(col);
}
/* }}} */

/* {{{ proto int imagecolordeallocate(int im, int index)
   De-allocate a color for an image */
PHP_FUNCTION(imagecolordeallocate)
{
	zval **imgind, **index;
	int col;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 2 || zend_get_parameters_ex(2, &imgind, &index) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	convert_to_long_ex(index);
	col = (*index)->value.lval;

	if (col >= 0 && col < gdImageColorsTotal(im)) {
		gdImageColorDeallocate(im, col);
		RETURN_TRUE;
	}
	else {
		php_error(E_WARNING, "Color index out of range");
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto int imagecolorresolve(int im, int red, int green, int blue)
   Get the index of the specified color or its closest possible alternative */
PHP_FUNCTION(imagecolorresolve)
{
	zval **imgind, **red, **green, **blue;
	int col;
	int r, g, b;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 4 || zend_get_parameters_ex(4, &imgind, &red,
													 &green, &blue) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	convert_to_long_ex(red);
	convert_to_long_ex(green);
	convert_to_long_ex(blue);
	
	r = (*red)->value.lval;
	g = (*green)->value.lval;
	b = (*blue)->value.lval;
	
	col = gdImageColorResolve(im, r, g, b);
	RETURN_LONG(col);
}
/* }}} */

/* {{{ proto int imagecolorexact(int im, int red, int green, int blue)
   Get the index of the specified color */
PHP_FUNCTION(imagecolorexact)
{
	zval **imgind, **red, **green, **blue;
	int col;
	int r, g, b;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 4 || zend_get_parameters_ex(4, &imgind, &red,
													 &green, &blue) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	convert_to_long_ex(red);
	convert_to_long_ex(green);
	convert_to_long_ex(blue);
	
	r = (*red)->value.lval;
	g = (*green)->value.lval;
	b = (*blue)->value.lval;
	
	col = gdImageColorExact(im, r, g, b);
	RETURN_LONG(col);
}
/* }}} */

/* {{{ proto int imagecolorset(int im, int col, int red, int green, int blue)
   Set the color for the specified palette index */
PHP_FUNCTION(imagecolorset)
{
	zval **imgind, **color, **red, **green, **blue;
	int col;
	int r, g, b;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 5 || zend_get_parameters_ex(5, &imgind, &color, &red, &green, &blue) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	convert_to_long_ex(color);
	convert_to_long_ex(red);
	convert_to_long_ex(green);
	convert_to_long_ex(blue);
	
	col = (*color)->value.lval;
	r = (*red)->value.lval;
	g = (*green)->value.lval;
	b = (*blue)->value.lval;
	
	if (col >= 0 && col < gdImageColorsTotal(im)) {
		im->red[col] = r;
		im->green[col] = g;
		im->blue[col] = b;
	}
	else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array imagecolorsforindex(int im, int col)
   Get the colors for an index */
PHP_FUNCTION(imagecolorsforindex)
{
	zval **imgind, **index;
	int col;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 2 || zend_get_parameters_ex(2, &imgind, &index) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	convert_to_long_ex(index);
	col = (*index)->value.lval;
	
	if (col >= 0 && col < gdImageColorsTotal(im)) {
		if (array_init(return_value) == FAILURE) {
			RETURN_FALSE;
		}
		add_assoc_long(return_value,"red",im->red[col]);
		add_assoc_long(return_value,"green",im->green[col]);
		add_assoc_long(return_value,"blue",im->blue[col]);
	}
	else {
		php_error(E_WARNING, "Color index out of range");
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto int imagesetpixel(int im, int x, int y, int col)
   Set a single pixel */
PHP_FUNCTION(imagesetpixel)
{
	zval **imgind, **xarg, **yarg, **colarg;
	gdImagePtr im;
	int col, y, x;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 4 ||
		zend_get_parameters_ex(4, &imgind, &xarg, &yarg, &colarg) == FAILURE)
	{
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, imgind, -1, "Image", GDG(le_gd));

	convert_to_long_ex(xarg);
	convert_to_long_ex(yarg);
	convert_to_long_ex(colarg);

	col = (*colarg)->value.lval;
	y = (*yarg)->value.lval;
	x = (*xarg)->value.lval;

	gdImageSetPixel(im,x,y,col);

	RETURN_TRUE;
}
/* }}} */	

/* im, x1, y1, x2, y2, col */
/* {{{ proto int imageline(int im, int x1, int y1, int x2, int y2, int col)
   Draw a line */
PHP_FUNCTION(imageline)
{
	zval **IM, **COL, **X1, **Y1, **X2, **Y2;
	gdImagePtr im;
	int col, y2, x2, y1, x1;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 6 ||
		zend_get_parameters_ex(6, &IM, &X1, &Y1, &X2, &Y2, &COL) == FAILURE)
	{
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(X1);
	convert_to_long_ex(Y1);
	convert_to_long_ex(X2);
	convert_to_long_ex(Y2);
	convert_to_long_ex(COL);

	x1 = (*X1)->value.lval;
	y1 = (*Y1)->value.lval;
	x2 = (*X2)->value.lval;
	y2 = (*Y2)->value.lval;
	col = (*COL)->value.lval;

	gdImageLine(im,x1,y1,x2,y2,col);
	RETURN_TRUE;
}
/* }}} */	

/* {{{ proto int imagedashedline(int im, int x1, int y1, int x2, int y2, int col)
   Draw a dashed line */
PHP_FUNCTION(imagedashedline)
{
	zval **IM, **COL, **X1, **Y1, **X2, **Y2;
	gdImagePtr im;
	int col, y2, x2, y1, x1;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 6 || zend_get_parameters_ex(6, &IM, &X1, &Y1, &X2, &Y2, &COL) == FAILURE)
	{
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(X1);
	convert_to_long_ex(Y1);
	convert_to_long_ex(X2);
	convert_to_long_ex(Y2);
	convert_to_long_ex(COL);

	x1 = (*X1)->value.lval;
	y1 = (*Y1)->value.lval;
	x2 = (*X2)->value.lval;
	y2 = (*Y2)->value.lval;
	col = (*COL)->value.lval;

	gdImageDashedLine(im,x1,y1,x2,y2,col);
	RETURN_TRUE;
}
/* }}} */

/* im, x1, y1, x2, y2, col */
/* {{{ proto int imagerectangle(int im, int x1, int y1, int x2, int y2, int col)
   Draw a rectangle */
PHP_FUNCTION(imagerectangle)
{
	zval **IM, **COL, **X1, **Y1, **X2, **Y2;
	gdImagePtr im;
	int col, y2, x2, y1, x1;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 6 ||
		zend_get_parameters_ex(6, &IM, &X1, &Y1, &X2, &Y2, &COL) == FAILURE)
	{
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(IM);
	convert_to_long_ex(X1);
	convert_to_long_ex(Y1);
	convert_to_long_ex(X2);
	convert_to_long_ex(Y2);
	convert_to_long_ex(COL);

	x1 = (*X1)->value.lval;
	y1 = (*Y1)->value.lval;
	x2 = (*X2)->value.lval;
	y2 = (*Y2)->value.lval;
	col = (*COL)->value.lval;

	gdImageRectangle(im,x1,y1,x2,y2,col);
	RETURN_TRUE;
}
/* }}} */	

/* im, x1, y1, x2, y2, col */
/* {{{ proto int imagefilledrectangle(int im, int x1, int y1, int x2, int y2, int col)
   Draw a filled rectangle */
PHP_FUNCTION(imagefilledrectangle)
{
	zval **IM, **COL, **X1, **Y1, **X2, **Y2;
	gdImagePtr im;
	int col, y2, x2, y1, x1;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 6 ||
		zend_get_parameters_ex(6, &IM, &X1, &Y1, &X2, &Y2, &COL) == FAILURE)
	{
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(X1);
	convert_to_long_ex(Y1);
	convert_to_long_ex(X2);
	convert_to_long_ex(Y2);
	convert_to_long_ex(COL);

	x1 = (*X1)->value.lval;
	y1 = (*Y1)->value.lval;
	x2 = (*X2)->value.lval;
	y2 = (*Y2)->value.lval;
	col = (*COL)->value.lval;

	gdImageFilledRectangle(im,x1,y1,x2,y2,col);
	RETURN_TRUE;
}
/* }}} */	

/* {{{ proto int imagearc(int im, int cx, int cy, int w, int h, int s, int e, int col)
   Draw a partial ellipse */
PHP_FUNCTION(imagearc)
{
	zval **COL, **E, **ST, **H, **W, **CY, **CX, **IM;
	gdImagePtr im;
	int col, e, st, h, w, cy, cx;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 8 ||
		zend_get_parameters_ex(8, &IM, &CX, &CY, &W, &H, &ST, &E, &COL) == FAILURE)
	{
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(CX);
	convert_to_long_ex(CY);
	convert_to_long_ex(W);
	convert_to_long_ex(H);
	convert_to_long_ex(ST);
	convert_to_long_ex(E);
	convert_to_long_ex(COL);

	col = (*COL)->value.lval;
	e = (*E)->value.lval;
	st = (*ST)->value.lval;
	h = (*H)->value.lval;
	w = (*W)->value.lval;
	cy = (*CY)->value.lval;
	cx = (*CX)->value.lval;

	if (e < 0) {
		e %= 360;
	}
	if (st < 0) {
		st %= 360;
	}

	gdImageArc(im,cx,cy,w,h,st,e,col);
	RETURN_TRUE;
}
/* }}} */	

/* im, x, y, border, col */
/* {{{ proto int imagefilltoborder(int im, int x, int y, int border, int col)
   Flood fill to specific color */
PHP_FUNCTION(imagefilltoborder)
{
	zval **IM, **X, **Y, **BORDER, **COL;
	gdImagePtr im;
	int col, border, y, x;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 5 ||
		zend_get_parameters_ex(5, &IM, &X, &Y, &BORDER, &COL) == FAILURE)
	{
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(X);
	convert_to_long_ex(Y);
	convert_to_long_ex(BORDER);
	convert_to_long_ex(COL);

	col = (*COL)->value.lval;
	border = (*BORDER)->value.lval;
	y = (*Y)->value.lval;
	x = (*X)->value.lval;

	gdImageFillToBorder(im,x,y,border,col);
	RETURN_TRUE;
}
/* }}} */	

/* im, x, y, col */
/* {{{ proto int imagefill(int im, int x, int y, int col)
   Flood fill */
PHP_FUNCTION(imagefill)
{
	zval **IM, **X, **Y, **COL;
	gdImagePtr im;
	int col, y, x;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 4 ||
		zend_get_parameters_ex(4, &IM, &X, &Y, &COL) == FAILURE)
	{
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(X);
	convert_to_long_ex(Y);
	convert_to_long_ex(COL);

	col = (*COL)->value.lval;
	y = (*Y)->value.lval;
	x = (*X)->value.lval;

	gdImageFill(im,x,y,col);
	RETURN_TRUE;
}
/* }}} */	

/* {{{ proto int imagecolorstotal(int im)
   Find out the number of colors in an image's palette */
PHP_FUNCTION(imagecolorstotal)
{
	zval **IM;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 1 || zend_get_parameters_ex(1, &IM) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	RETURN_LONG(gdImageColorsTotal(im));
}
/* }}} */	

/* im, col */
/* {{{ proto int imagecolortransparent(int im [, int col])
   Define a color as transparent */
PHP_FUNCTION(imagecolortransparent)
{
	zval **IM, **COL = NULL;
	gdImagePtr im;
	int col;
	GDLS_FETCH();

	switch(ARG_COUNT(ht)) {
	case 1:
		if (zend_get_parameters_ex(1, &IM) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 2:
		if (zend_get_parameters_ex(2, &IM, &COL) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		convert_to_long_ex(COL);
		break;
	default:
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	if ((*COL) != NULL) {
		col = (*COL)->value.lval;
		gdImageColorTransparent(im,col);
	}
	col = gdImageGetTransparent(im);
	RETURN_LONG(col);
}
/* }}} */	

/* im, interlace */
/* {{{ proto int imageinterlace(int im [, int interlace])
   Enable or disable interlace */
PHP_FUNCTION(imageinterlace)
{
	zval **IM, **INT = NULL;
	gdImagePtr im;
	int interlace;
	GDLS_FETCH();

	switch(ARG_COUNT(ht)) {
	case 1:
		if (zend_get_parameters_ex(1, &IM) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		break;
	case 2:
		if (zend_get_parameters_ex(2, &IM, &INT) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		convert_to_long_ex(INT);
		break;
	default:
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	if ((*INT) != NULL) {
		interlace = (*INT)->value.lval;
		gdImageInterlace(im,interlace);
	}
	interlace = gdImageGetInterlaced(im);
	RETURN_LONG(interlace);
}
/* }}} */	

/* arg = 0  normal polygon
   arg = 1  filled polygon */
/* im, points, num_points, col */
static void php_imagepolygon(INTERNAL_FUNCTION_PARAMETERS, int filled) {
	zval **IM, **POINTS, **NPOINTS, **COL;
	pval **var = NULL;
	gdImagePtr im;
	gdPoint points[PolyMaxPoints];	
	int npoints, col, nelem, i;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 4 ||
		zend_get_parameters_ex(4, &IM, &POINTS, &NPOINTS, &COL) == FAILURE)
	{
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(NPOINTS);
	convert_to_long_ex(COL);

	npoints = (*NPOINTS)->value.lval;
	col = (*COL)->value.lval;

	if ((*POINTS)->type != IS_ARRAY) {
		php_error(E_WARNING, "2nd argument to imagepolygon not an array");
		RETURN_FALSE;
	}

/*
    ** we shouldn't need this check, should we? **

    if (!ParameterPassedByReference(ht, 2)) {
        php_error(E_WARNING, "2nd argument to imagepolygon not passed by reference");
		RETURN_FALSE;
    }
*/

	nelem = zend_hash_num_elements((*POINTS)->value.ht);
	if (nelem < 6) {
		php_error(E_WARNING,
					"you must have at least 3 points in your array");
		RETURN_FALSE;
	}

	if (nelem < npoints * 2) {
		php_error(E_WARNING,
					"trying to use %d points in array with only %d points",
					npoints, nelem/2);
		RETURN_FALSE;
	}

	if (npoints > PolyMaxPoints) {
		php_error(E_WARNING, "maximum %d points", PolyMaxPoints);
		RETURN_FALSE;
	}

	for (i = 0; i < npoints; i++) {
		if (zend_hash_index_find((*POINTS)->value.ht, (i * 2), (void **) &var) == SUCCESS) {
			SEPARATE_ZVAL((var));
			convert_to_long(*var);
			points[i].x = (*var)->value.lval;
		}
		if (zend_hash_index_find((*POINTS)->value.ht, (i * 2) + 1, (void **) &var) == SUCCESS) {
			SEPARATE_ZVAL(var);
			convert_to_long(*var);
			points[i].y = (*var)->value.lval;
		}
	}

	if (filled) {
		gdImageFilledPolygon(im, points, npoints, col);
	}
	else {
		gdImagePolygon(im, points, npoints, col);
	}

	RETURN_TRUE;
}


/* {{{ proto int imagepolygon(int im, array point, int num_points, int col)
   Draw a polygon */
PHP_FUNCTION(imagepolygon)
{
	php_imagepolygon(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto int imagefilledpolygon(int im, array point, int num_points, int col)
   Draw a filled polygon */
PHP_FUNCTION(imagefilledpolygon)
{
	php_imagepolygon(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */


static gdFontPtr php_find_gd_font(int size)
{
	gdFontPtr font;
	int ind_type;
	GDLS_FETCH();

	switch (size) {
    	case 1:
			 font = gdFontTiny;
			 break;
    	case 2:
			 font = gdFontSmall;
			 break;
    	case 3:
			 font = gdFontMediumBold;
			 break;
    	case 4:
			 font = gdFontLarge;
			 break;
    	case 5:
			 font = gdFontGiant;
			 break;
	    default:
			font = zend_list_find(size - 5, &ind_type);
			 if (!font || ind_type != GDG(le_gd_font)) {
				  if (size < 1) {
					   font = gdFontTiny;
				  } else {
					   font = gdFontGiant;
				  }
			 }
			 break;
	}

	return font;
}


/*
 * arg = 0  ImageFontWidth
 * arg = 1  ImageFontHeight
 */
static void php_imagefontsize(INTERNAL_FUNCTION_PARAMETERS, int arg)
{
	pval *SIZE;
	gdFontPtr font;

	if (ARG_COUNT(ht) != 1 || zend_get_parameters(ht, 1, &SIZE) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	convert_to_long(SIZE);
	font = php_find_gd_font(SIZE->value.lval);
	RETURN_LONG(arg ? font->h : font->w);
}


/* {{{ proto int imagefontwidth(int font)
   Get font width */
PHP_FUNCTION(imagefontwidth)
{
	php_imagefontsize(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto int imagefontheight(int font)
   Get font height */
PHP_FUNCTION(imagefontheight)
{
	php_imagefontsize(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */


/* workaround for a bug in gd 1.2 */
void php_gdimagecharup(gdImagePtr im, gdFontPtr f, int x, int y, int c,
						 int color)
{
	int cx, cy, px, py, fline;
	cx = 0;
	cy = 0;
	if ((c < f->offset) || (c >= (f->offset + f->nchars))) {
		return;
	}
	fline = (c - f->offset) * f->h * f->w;
	for (py = y; (py > (y - f->w)); py--) {
		for (px = x; (px < (x + f->h)); px++) {
			if (f->data[fline + cy * f->w + cx]) {
				gdImageSetPixel(im, px, py, color);	
			}
			cy++;
		}
		cy = 0;
		cx++;
	}
}

/*
 * arg = 0  ImageChar
 * arg = 1  ImageCharUp
 * arg = 2  ImageString
 * arg = 3  ImageStringUp
 */
static void php_imagechar(INTERNAL_FUNCTION_PARAMETERS, int mode) {
	zval **IM, **SIZE, **X, **Y, **C, **COL;
	gdImagePtr im;
	int ch = 0, col, x, y, size, i, l = 0;
	unsigned char *str = NULL;
	gdFontPtr font;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 6 ||
		zend_get_parameters_ex(6, &IM, &SIZE, &X, &Y, &C, &COL) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(SIZE);
	convert_to_long_ex(X);
	convert_to_long_ex(Y);
	convert_to_string_ex(C);
	convert_to_long_ex(COL);

	col = (*COL)->value.lval;

	if (mode < 2) {
		ch = (int)((unsigned char)*((*C)->value.str.val));
	} else {
		str = (unsigned char *) estrndup((*C)->value.str.val, (*C)->value.str.len);
		l = strlen(str);
	}

	y = (*Y)->value.lval;
	x = (*X)->value.lval;
	size = (*SIZE)->value.lval;

	font = php_find_gd_font(size);

	switch(mode) {
    	case 0:
			gdImageChar(im, font, x, y, ch, col);
			break;
    	case 1:
			php_gdimagecharup(im, font, x, y, ch, col);
			break;
    	case 2:
			for (i = 0; (i < l); i++) {
				gdImageChar(im, font, x, y, (int)((unsigned char)str[i]),
							col);
				x += font->w;
			}
			break;
    	case 3: {
			for (i = 0; (i < l); i++) {
				/* php_gdimagecharup(im, font, x, y, (int)str[i], col); */
				gdImageCharUp(im, font, x, y, (int)str[i], col);
				y -= font->w;
			}
			break;
		}
	}
	if (str) {
		efree(str);
	}
	RETURN_TRUE;
}	

/* {{{ proto int imagechar(int im, int font, int x, int y, string c, int col)
   Draw a character */ 
PHP_FUNCTION(imagechar) {
	php_imagechar(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto int imagecharup(int im, int font, int x, int y, string c, int col)
   Draw a character rotated 90 degrees counter-clockwise */
PHP_FUNCTION(imagecharup) {
	php_imagechar(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto int imagestring(int im, int font, int x, int y, string str, int col)
   Draw a string horizontally */
PHP_FUNCTION(imagestring) {
	php_imagechar(INTERNAL_FUNCTION_PARAM_PASSTHRU, 2);
}
/* }}} */

/* {{{ proto int imagestringup(int im, int font, int x, int y, string str, int col)
   Draw a string vertically - rotated 90 degrees counter-clockwise */
PHP_FUNCTION(imagestringup) {
	php_imagechar(INTERNAL_FUNCTION_PARAM_PASSTHRU, 3);
}
/* }}} */

/* {{{ proto int imagecopy(int dst_im, int src_im, int dstX, int dstY, int srcX, int srcY, int srcW, int srcH)
   Copy part of an image */ 
PHP_FUNCTION(imagecopy)
{
	zval **SIM, **DIM, **SX, **SY, **SW, **SH, **DX, **DY;
	gdImagePtr im_dst;
	gdImagePtr im_src;
	int srcH, srcW, srcY, srcX, dstY, dstX;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 8 ||
		zend_get_parameters_ex(8, &DIM, &SIM, &DX, &DY, &SX, &SY, &SW, &SH)
						 == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im_src, gdImagePtr, SIM, -1, "Image", GDG(le_gd));
	ZEND_FETCH_RESOURCE(im_dst, gdImagePtr, DIM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(SX);
	convert_to_long_ex(SY);
	convert_to_long_ex(SW);
	convert_to_long_ex(SH);
	convert_to_long_ex(DX);
	convert_to_long_ex(DY);

	srcX = (*SX)->value.lval;
	srcY = (*SY)->value.lval;
	srcH = (*SH)->value.lval;
	srcW = (*SW)->value.lval;
	dstX = (*DX)->value.lval;
	dstY = (*DY)->value.lval;

	gdImageCopy(im_dst, im_src, dstX, dstY, srcX, srcY, srcW, srcH);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int imagecopyresized(int dst_im, int src_im, int dstX, int dstY, int srcX, int srcY, int dstW, int dstH, int srcW, int srcH);
   Copy and resize part of an image */
PHP_FUNCTION(imagecopyresized)
{
	zval **SIM, **DIM, **SX, **SY, **SW, **SH, **DX, **DY, **DW, **DH;
	gdImagePtr im_dst;
	gdImagePtr im_src;
	int srcH, srcW, dstH, dstW, srcY, srcX, dstY, dstX;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 10 ||
		zend_get_parameters_ex(10, &DIM, &SIM, &DX, &DY, &SX, &SY, &DW, &DH,
							   &SW, &SH) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im_dst, gdImagePtr, DIM, -1, "Image", GDG(le_gd));
	ZEND_FETCH_RESOURCE(im_src, gdImagePtr, SIM, -1, "Image", GDG(le_gd));

	convert_to_long_ex(SX);
	convert_to_long_ex(SY);
	convert_to_long_ex(SW);
	convert_to_long_ex(SH);
	convert_to_long_ex(DX);
	convert_to_long_ex(DY);
	convert_to_long_ex(DW);
	convert_to_long_ex(DH);

	srcX = (*SX)->value.lval;
	srcY = (*SY)->value.lval;
	srcH = (*SH)->value.lval;
	srcW = (*SW)->value.lval;
	dstX = (*DX)->value.lval;
	dstY = (*DY)->value.lval;
	dstH = (*DH)->value.lval;
	dstW = (*DW)->value.lval;

	gdImageCopyResized(im_dst, im_src, dstX, dstY, srcX, srcY, dstW, dstH,
					   srcW, srcH);
	RETURN_TRUE;
}
/* }}} */	

/* {{{ proto int imagesx(int im)
   Get image width */
PHP_FUNCTION(imagesx)
{
	zval **IM;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 1 || zend_get_parameters_ex(1, &IM) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	RETURN_LONG(gdImageSX(im));
}
/* }}} */

/* {{{ proto int imagesy(int im)
   Get image height */
PHP_FUNCTION(imagesy)
{
	zval **IM;
	gdImagePtr im;
	GDLS_FETCH();

	if (ARG_COUNT(ht) != 1 || zend_get_parameters_ex(1, &IM) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));

	RETURN_LONG(gdImageSY(im));
}
/* }}} */

#ifdef ENABLE_GD_TTF

#define TTFTEXT_DRAW 0
#define TTFTEXT_BBOX 1

/* {{{ proto array imagettfbbox(int size, int angle, string font_file, string text)
   Give the bounding box of a text using TrueType fonts */
PHP_FUNCTION(imagettfbbox)
{
	php_imagettftext_common(INTERNAL_FUNCTION_PARAM_PASSTHRU, TTFTEXT_BBOX);
}
/* }}} */

/* {{{ proto array imagettftext(int im, int size, int angle, int x, int y, int col, string font_file, string text)
   Write text to the image using a TrueType font */
PHP_FUNCTION(imagettftext)
{
	php_imagettftext_common(INTERNAL_FUNCTION_PARAM_PASSTHRU, TTFTEXT_DRAW);
}
/* }}} */

static
void php_imagettftext_common(INTERNAL_FUNCTION_PARAMETERS, int mode)
{
	zval **IM, **PTSIZE, **ANGLE, **X, **Y, **C, **FONTNAME, **COL;
	gdImagePtr im;
	int col, x, y, l=0, i;
	int brect[8];
	double ptsize, angle;
	unsigned char *str = NULL, *fontname = NULL;
	char *error;
	GDLS_FETCH();

	if (mode == TTFTEXT_BBOX) {
		if (ARG_COUNT(ht) != 4 || zend_get_parameters_ex(4, &PTSIZE, &ANGLE, &FONTNAME, &C) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
	} else {
		if (ARG_COUNT(ht) != 8 || zend_get_parameters_ex(8, &IM, &PTSIZE, &ANGLE, &X, &Y, &COL, &FONTNAME, &C) == FAILURE) {
			WRONG_PARAM_COUNT;
		}
		ZEND_FETCH_RESOURCE(im, gdImagePtr, IM, -1, "Image", GDG(le_gd));
	}

	convert_to_double_ex(PTSIZE);
	convert_to_double_ex(ANGLE);
	convert_to_string_ex(FONTNAME);
	convert_to_string_ex(C);
	if (mode == TTFTEXT_BBOX) {
              im = NULL;
		col = x = y = -1;
	} else {
		convert_to_long_ex(X);
		convert_to_long_ex(Y);
		convert_to_long_ex(COL);
		col = (*COL)->value.lval;
		y = (*Y)->value.lval;
		x = (*X)->value.lval;
	}

	ptsize = (*PTSIZE)->value.dval;
	angle = (*ANGLE)->value.dval * (M_PI/180); /* convert to radians */

	str = (unsigned char *) (*C)->value.str.val;
	l = strlen(str);
	fontname = (unsigned char *) (*FONTNAME)->value.str.val;

	error = gdttf(im, brect, col, fontname, ptsize, angle, x, y, str);

	if (error) {
		php_error(E_WARNING, error);
		RETURN_FALSE;
	}

	
	if (array_init(return_value) == FAILURE) {
		RETURN_FALSE;
	}

	/* return array with the text's bounding box */
	for (i = 0; i < 8; i++) {
		add_next_index_long(return_value, brect[i]);
	}
}
#endif	/* ENABLE_GD_TTF */

#if HAVE_LIBT1

void php_free_ps_font(int *font)
{
	T1_DeleteFont(*font);
	efree(font);
}

void php_free_ps_enc(char **enc)
{
	T1_DeleteEncoding(enc);
}

/* {{{ proto int imagepsloadfont(string pathname)
   Load a new font from specified file */
PHP_FUNCTION(imagepsloadfont)
{
	zval **file;
	int f_ind;
	int *font;

	if (ARG_COUNT(ht) != 1 || zend_get_parameters_ex(1, &file) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(file);

	f_ind = T1_AddFont((*file)->value.str.val);

	if (f_ind < 0) {
		switch (f_ind) {
		case -1:
			php_error(E_WARNING, "Couldn't find the font file");
			RETURN_FALSE;
			break;
		case -2:
		case -3:
			php_error(E_WARNING, "Memory allocation fault in t1lib");
			RETURN_FALSE;
			break;
		default:
			php_error(E_WARNING, "An unknown error occurred in t1lib");
			RETURN_FALSE;
			break;
		}
	}

	T1_LoadFont(f_ind);

	font = (int *) emalloc(sizeof(int));
	*font = f_ind;
	ZEND_REGISTER_RESOURCE(return_value, font, GDG(le_ps_font));
}
/* }}} */

/* {{{ The function in t1lib which this function uses seem to be buggy...
proto int imagepscopyfont(int font_index)
Make a copy of a font for purposes like extending or reenconding */
/*
PHP_FUNCTION(imagepscopyfont)
{
	pval *fnt;
	int l_ind, type;
	gd_ps_font *nf_ind, *of_ind;

	if (ARG_COUNT(ht) != 1 || zend_get_parameters(ht, 1, &fnt) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long(fnt);

	of_ind = zend_list_find(fnt->value.lval, &type);

	if (type != GDG(le_ps_font)) {
		php_error(E_WARNING, "%d is not a Type 1 font index", fnt->value.lval);
		RETURN_FALSE;
	}

	nf_ind = emalloc(sizeof(gd_ps_font));
	nf_ind->font_id = T1_CopyFont(of_ind->font_id);

	if (nf_ind->font_id < 0) {
		l_ind = nf_ind->font_id;
		efree(nf_ind);
		switch (l_ind) {
		case -1:
			php_error(E_WARNING, "FontID %d is not loaded in memory", l_ind);
			RETURN_FALSE;
			break;
		case -2:
			php_error(E_WARNING, "Tried to copy a logical font");
			RETURN_FALSE;
			break;
		case -3:
			php_error(E_WARNING, "Memory allocation fault in t1lib");
			RETURN_FALSE;
			break;
		default:
			php_error(E_WARNING, "An unknown error occurred in t1lib");
			RETURN_FALSE;
			break;
		}
	}

	nf_ind->extend = 1;
	l_ind = zend_list_insert(nf_ind, GDG(le_ps_font));
	RETURN_LONG(l_ind);
}
*/
/* }}} */

/* {{{ proto bool imagepsfreefont(int font_index)
   Free memory used by a font */
PHP_FUNCTION(imagepsfreefont)
{
	zval **fnt;
	int *f_ind;

	if (ARG_COUNT(ht) != 1 || zend_get_parameters_ex(1, &fnt) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(f_ind, int *, fnt, -1, "Type 1 font", GDG(le_ps_font));

	zend_list_delete((*fnt)->value.lval);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool imagepsencodefont(int font_index, string filename)
   To change a fonts character encoding vector */
PHP_FUNCTION(imagepsencodefont)
{
	zval **fnt, **enc;
	char **enc_vector;
	int *f_ind;

	if (ARG_COUNT(ht) != 2 || zend_get_parameters_ex(2, &fnt, &enc) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(enc);

	ZEND_FETCH_RESOURCE(f_ind, int *, fnt, -1, "Type 1 font", GDG(le_ps_font));

	if ((enc_vector = T1_LoadEncoding((*enc)->value.str.val)) == NULL) {
		php_error(E_WARNING, "Couldn't load encoding vector from %s", (*enc)->value.str.val);
		RETURN_FALSE;
	}

	T1_DeleteAllSizes(*f_ind);
	if (T1_ReencodeFont(*f_ind, enc_vector)) {
		T1_DeleteEncoding(enc_vector);
		php_error(E_WARNING, "Couldn't reencode font");
		RETURN_FALSE;
	}
	zend_list_insert(enc_vector, GDG(le_ps_enc));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool imagepsextendfont(int font_index, double extend)
   Extend or or condense (if extend < 1) a font */
PHP_FUNCTION(imagepsextendfont)
{
	zval **fnt, **ext;
	int *f_ind;

	if (ARG_COUNT(ht) != 2 || zend_get_parameters_ex(2, &fnt, &ext) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_double_ex(ext);

	ZEND_FETCH_RESOURCE(f_ind, int *, fnt, -1, "Type 1 font", GDG(le_ps_font));

	if (T1_ExtendFont(*f_ind, (*ext)->value.dval) != 0) RETURN_FALSE;

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool imagepsslantfont(int font_index, double slant)
   Slant a font */
PHP_FUNCTION(imagepsslantfont)
{
	zval **fnt, **slt;
	int *f_ind;

	if (ARG_COUNT(ht) != 2 || zend_get_parameters_ex(2, &fnt, &slt) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_double_ex(slt);

	ZEND_FETCH_RESOURCE(f_ind, int *, fnt, -1, "Type 1 font", GDG(le_ps_font));

	if (T1_SlantFont(*f_ind, (*slt)->value.dval) != 0) RETURN_FALSE;
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array imagepstext(int image, string text, int font, int size, int xcoord, int ycoord [, int space, int tightness, double angle, int antialias])
   Rasterize a string over an image */
PHP_FUNCTION(imagepstext)
{
	zval **img, **str, **fnt, **sz, **fg, **bg, **sp, **px, **py, **aas, **wd, **ang;
	int i, j, x, y;
	int space;
	int *f_ind;
	int h_lines, v_lines, c_ind;
	int rd, gr, bl, fg_rd, fg_gr, fg_bl, bg_rd, bg_gr, bg_bl;
	int aa[16], aa_steps;
	int width, amount_kern, add_width;
	double angle, extend;
	unsigned long aa_greys[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	gdImagePtr bg_img;
	GLYPH *str_img;
	T1_OUTLINE *char_path, *str_path;
	T1_TMATRIX *transform = NULL;

	switch(ARG_COUNT(ht)) {
	case 8:
		if (zend_get_parameters_ex(8, &img, &str, &fnt, &sz, &fg, &bg, &px, &py) == FAILURE) {
			RETURN_FALSE;
		}
		convert_to_string_ex(str);
		convert_to_long_ex(sz);
		convert_to_long_ex(fg);
		convert_to_long_ex(bg);
		convert_to_long_ex(px);
		convert_to_long_ex(py);
		x = (*px)->value.lval;
		y = (*py)->value.lval;
		space = 0;
		aa_steps = 4;
		width = 0;
		angle = 0;
		break;
	case 12:
		if (zend_get_parameters_ex(12, &img, &str, &fnt, &sz, &fg, &bg, &px, &py, &sp, &wd, &ang, &aas) == FAILURE) {
			RETURN_FALSE;
		}
		convert_to_string_ex(str);
		convert_to_long_ex(sz);
		convert_to_long_ex(sp);
		convert_to_long_ex(fg);
		convert_to_long_ex(bg);
		convert_to_long_ex(px);
		convert_to_long_ex(py);
		x = (*px)->value.lval;
		y = (*py)->value.lval;
		convert_to_long_ex(sp);
		space = (*sp)->value.lval;
		convert_to_long_ex(aas);
		aa_steps = (*aas)->value.lval;
		convert_to_long_ex(wd);
		width = (*wd)->value.lval;
		convert_to_double_ex(ang);
		angle = (*ang)->value.dval;
		break;
	default:
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(bg_img, gdImagePtr, img, -1, "Image", GDG(le_gd));
	ZEND_FETCH_RESOURCE(f_ind, int *, fnt, -1, "Type 1 font", GDG(le_ps_font));

	fg_rd = gdImageRed(bg_img, (*fg)->value.lval);
	fg_gr = gdImageGreen(bg_img, (*fg)->value.lval);
	fg_bl = gdImageBlue(bg_img, (*fg)->value.lval);
	bg_rd = gdImageRed(bg_img, (*bg)->value.lval);
	bg_gr = gdImageGreen(bg_img, (*bg)->value.lval);
	bg_bl = gdImageBlue(bg_img, (*bg)->value.lval);

	for (i = 0; i < aa_steps; i++) {
		rd = bg_rd+(double)(fg_rd-bg_rd)/aa_steps*(i+1);
		gr = bg_gr+(double)(fg_gr-bg_gr)/aa_steps*(i+1);
		bl = bg_bl+(double)(fg_bl-bg_bl)/aa_steps*(i+1);
		aa[i] = gdImageColorResolve(bg_img, rd, gr, bl);
	}

	T1_AASetBitsPerPixel(8);

	switch (aa_steps) {
	case 4:
		T1_AASetGrayValues(0, 1, 2, 3, 4);
		T1_AASetLevel(T1_AA_LOW);
		break;
	case 16:
		T1_AAHSetGrayValues(aa_greys);
		T1_AASetLevel(T1_AA_HIGH);
		break;
	default:
		php_error(E_WARNING, "Invalid value %d as number of steps for antialiasing", aa_steps);
		RETURN_FALSE;
	}

	if (angle) {
		transform = T1_RotateMatrix(NULL, angle);
	}

	if (width) {
		extend = T1_GetExtend(*f_ind);
		str_path = T1_GetCharOutline(*f_ind, (*str)->value.str.val[0], (*sz)->value.lval, transform);

		for (i = 1; i < (*str)->value.str.len; i++) {
			amount_kern = (int) T1_GetKerning(*f_ind, (*str)->value.str.val[i-1], (*str)->value.str.val[i]);
			amount_kern += (*str)->value.str.val[i-1] == ' ' ? space : 0;
			add_width = (int) (amount_kern+width)/extend;

			char_path = T1_GetMoveOutline(*f_ind, add_width, 0, 0, (*sz)->value.lval, transform);
			str_path = T1_ConcatOutlines(str_path, char_path);

			char_path = T1_GetCharOutline(*f_ind, (*str)->value.str.val[i], (*sz)->value.lval, transform);
			str_path = T1_ConcatOutlines(str_path, char_path);
		}
		str_img = T1_AAFillOutline(str_path, 0);
	} else {
		str_img = T1_AASetString(*f_ind, (*str)->value.str.val,  (*str)->value.str.len,
								 space, T1_KERNING, (*sz)->value.lval, transform);
	}

	if (T1_errno) RETURN_FALSE;

	h_lines = str_img->metrics.ascent -  str_img->metrics.descent;
	v_lines = str_img->metrics.rightSideBearing - str_img->metrics.leftSideBearing;

	for (i = 0; i < v_lines; i++) {
		for (j = 0; j < h_lines; j++) {
			switch (str_img->bits[j*v_lines+i]) {
			case 0:
				break;
			default:
				c_ind = aa[str_img->bits[j*v_lines+i]-1];
				gdImageSetPixel(bg_img, x+str_img->metrics.leftSideBearing+i, y-str_img->metrics.ascent+j, c_ind);
			}
		}
	}

	if (array_init(return_value) == FAILURE) {
		RETURN_FALSE;
	}

	add_next_index_long(return_value, str_img->metrics.leftSideBearing);
	add_next_index_long(return_value, str_img->metrics.descent);
	add_next_index_long(return_value, str_img->metrics.rightSideBearing);
	add_next_index_long(return_value, str_img->metrics.ascent);

}
/* }}} */

/* {{{ proto array imagepsbbox(string text, int font, int size [, int space, int tightness, int angle])
   Return the bounding box needed by a string if rasterized */
PHP_FUNCTION(imagepsbbox)
{
	zval **str, **fnt, **sz, **sp, **wd, **ang;
	int i, space, add_width, char_width, amount_kern;
	int cur_x, cur_y, dx, dy;
	int x1, y1, x2, y2, x3, y3, x4, y4;
	int *f_ind;
	int per_char = 0;
	double angle, sin_a = 0, cos_a = 0;
	BBox char_bbox, str_bbox = {0, 0, 0, 0};

	switch(ARG_COUNT(ht)) {
	case 3:
		if (zend_get_parameters_ex(3, &str, &fnt, &sz) == FAILURE) {
			RETURN_FALSE;
		}
		convert_to_string_ex(str);
		convert_to_long_ex(sz);
		space = 0;
		break;
	case 6:
		if (zend_get_parameters_ex(6, &str, &fnt, &sz, &sp, &wd, &ang) == FAILURE) {
			RETURN_FALSE;
		}
		convert_to_string_ex(str);
		convert_to_long_ex(sz);
		convert_to_long_ex(sp);
		space = (*sp)->value.lval;
		convert_to_long_ex(wd);
		add_width = (*wd)->value.lval;
		convert_to_double_ex(ang);
		angle = (*ang)->value.dval * M_PI / 180;
		sin_a = sin(angle);
		cos_a = cos(angle);
		per_char =  add_width || angle ? 1 : 0;
		break;
	default:
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(f_ind, int *, fnt, -1, "Type 1 font", GDG(le_ps_font));

#define max(a, b) (a > b ? a : b)
#define min(a, b) (a < b ? a : b)
#define new_x(a, b) (int) ((a) * cos_a - (b) * sin_a)
#define new_y(a, b) (int) ((a) * sin_a + (b) * cos_a)

	if (per_char) {
		space += T1_GetCharWidth(*f_ind, ' ');
		cur_x = cur_y = 0;

		for (i = 0; i < (*str)->value.str.len; i++) {
			if ((*str)->value.str.val[i] == ' ') {
				char_bbox.llx = char_bbox.lly = char_bbox.ury = 0;
				char_bbox.urx = char_width = space;
			} else {
				char_bbox = T1_GetCharBBox(*f_ind, (*str)->value.str.val[i]);
				char_width = T1_GetCharWidth(*f_ind, (*str)->value.str.val[i]);
			}
			amount_kern = i ? T1_GetKerning(*f_ind, (*str)->value.str.val[i-1], (*str)->value.str.val[i]) : 0;

			/* Transfer character bounding box to right place */
			x1 = new_x(char_bbox.llx, char_bbox.lly) + cur_x;
			y1 = new_y(char_bbox.llx, char_bbox.lly) + cur_y;
			x2 = new_x(char_bbox.llx, char_bbox.ury) + cur_x;
			y2 = new_y(char_bbox.llx, char_bbox.ury) + cur_y;
			x3 = new_x(char_bbox.urx, char_bbox.ury) + cur_x;
			y3 = new_y(char_bbox.urx, char_bbox.ury) + cur_y;
			x4 = new_x(char_bbox.urx, char_bbox.lly) + cur_x;
			y4 = new_y(char_bbox.urx, char_bbox.lly) + cur_y;

			/* Find min & max values and compare them with current bounding box */
			str_bbox.llx = min(str_bbox.llx, min(x1, min(x2, min(x3, x4))));
			str_bbox.lly = min(str_bbox.lly, min(y1, min(y2, min(y3, y4))));
			str_bbox.urx = max(str_bbox.urx, max(x1, max(x2, max(x3, x4))));
			str_bbox.ury = max(str_bbox.ury, max(y1, max(y2, max(y3, y4))));

			/* Move to the next base point */
			dx = new_x(char_width + add_width + amount_kern, 0);
			dy = new_y(char_width + add_width + amount_kern, 0);
			cur_x += dx;
			cur_y += dy;
			/*
			printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", x1, y1, x2, y2, x3, y3, x4, y4, char_bbox.llx, char_bbox.lly, char_bbox.urx, char_bbox.ury, char_width, amount_kern, cur_x, cur_y, dx, dy);
			*/
		}

	} else {
		str_bbox = T1_GetStringBBox(*f_ind, (*str)->value.str.val, (*str)->value.str.len, space, T1_KERNING);
	}
	if (T1_errno) RETURN_FALSE;
	
	if (array_init(return_value) == FAILURE) {
		RETURN_FALSE;
	}
	/*
	printf("%d %d %d %d\n", str_bbox.llx, str_bbox.lly, str_bbox.urx, str_bbox.ury);
	*/
	add_next_index_long(return_value, (int) ceil(((double) str_bbox.llx)*(*sz)->value.lval/1000));
	add_next_index_long(return_value, (int) ceil(((double) str_bbox.lly)*(*sz)->value.lval/1000));
	add_next_index_long(return_value, (int) ceil(((double) str_bbox.urx)*(*sz)->value.lval/1000));
	add_next_index_long(return_value, (int) ceil(((double) str_bbox.ury)*(*sz)->value.lval/1000));
}
/* }}} */

#endif	/* HAVE_LIBT1 */

#endif	/* HAVE_LIBGD */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
