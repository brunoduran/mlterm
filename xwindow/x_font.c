/*
 *	$Id$
 */

#include  "x_font.h"

#include  <string.h>		/* memset/strncasecmp */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* kik_snprintf */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* kik_str_sep/kik_str_to_int */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN/K_MIN */
#include  <kiklib/kik_locale.h>	/* kik_get_lang() */

#include  "ml_char_encoding.h"	/* x_convert_to_xft_ucs4 */


#define  FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p) \
	for( (font_encoding_p) = &csinfo->encoding_names[0] ; *(font_encoding_p) ; (font_encoding_p) ++)
#define  DIVIDE_ROUNDING(a,b)  ( ((int)((a)*10 + (b)*5)) / ((int)((b)*10)) )
#define  DIVIDE_ROUNDINGUP(a,b) ( ((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)) )

#if  0
#define  __DEBUG
#endif


typedef struct  cs_info
{
	mkf_charset_t  cs ;

	/*
	 * default encoding.
	 *
	 * !! Notice !!
	 * the last element must be NULL.
	 */
	char *   encoding_names[3] ;

} cs_info_t ;


/* --- static variables --- */

/*
 * If this table is changed, x_font_config.c:cs_table and mc_font.c:cs_info_table
 * shoule be also changed.
 */
static cs_info_t  cs_info_table[] =
{
	{ ISO10646_UCS4_1 , { "iso10646-1" , NULL , NULL , } , } ,
	{ ISO10646_UCS2_1 , { "iso10646-1" , NULL , NULL , } , } ,

	{ DEC_SPECIAL , { "iso8859-1" , NULL , NULL , } , } ,
	{ ISO8859_1_R , { "iso8859-1" , NULL , NULL , } , } ,
	{ ISO8859_2_R , { "iso8859-2" , NULL , NULL , } , } ,
	{ ISO8859_3_R , { "iso8859-3" , NULL , NULL , } , } ,
	{ ISO8859_4_R , { "iso8859-4" , NULL , NULL , } , } ,
	{ ISO8859_5_R , { "iso8859-5" , NULL , NULL , } , } ,
	{ ISO8859_6_R , { "iso8859-6" , NULL , NULL , } , } ,
	{ ISO8859_7_R , { "iso8859-7" , NULL , NULL , } , } ,
	{ ISO8859_8_R , { "iso8859-8" , NULL , NULL , } , } ,
	{ ISO8859_9_R , { "iso8859-9" , NULL , NULL , } , } ,
	{ ISO8859_10_R , { "iso8859-10" , NULL , NULL , } , } ,
	{ TIS620_2533 , { "tis620.2533-1" , "tis620.2529-1" , NULL , } , } ,
	{ ISO8859_13_R , { "iso8859-13" , NULL , NULL , } , } ,
	{ ISO8859_14_R , { "iso8859-14" , NULL , NULL , } , } ,
	{ ISO8859_15_R , { "iso8859-15" , NULL , NULL , } , } ,
	{ ISO8859_16_R , { "iso8859-16" , NULL , NULL , } , } ,

	/*
	 * XXX
	 * The encoding of TCVN font is iso8859-1 , and its font family is
	 * .VnTime or .VnTimeH ...
	 * How to deal with it ?
	 */
	{ TCVN5712_3_1993 , { NULL , NULL , NULL , } , } ,

	{ ISCII , { NULL , NULL , NULL , } , } ,
	{ VISCII , { "viscii-1" , NULL , NULL , } , } ,
	{ KOI8_R , { "koi8-r" , NULL , NULL , } , } ,
	{ KOI8_U , { "koi8-u" , NULL , NULL , } , } ,

#if  0
	/*
	 * XXX
	 * KOI8_T, GEORGIAN_PS and CP125X charset can be shown by unicode font only.
	 */
	{ KOI8_T , { NULL , NULL , NULL , } , } ,
	{ GEORGIAN_PS , { NULL , NULL , NULL , } , } ,
	{ CP1250 , { NULL , NULL , NULL , } , } ,
	{ CP1251 , { NULL , NULL , NULL , } , } ,
	{ CP1252 , { NULL , NULL , NULL , } , } ,
	{ CP1253 , { NULL , NULL , NULL , } , } ,
	{ CP1254 , { NULL , NULL , NULL , } , } ,
	{ CP1255 , { NULL , NULL , NULL , } , } ,
	{ CP1256 , { NULL , NULL , NULL , } , } ,
	{ CP1257 , { NULL , NULL , NULL , } , } ,
	{ CP1258 , { NULL , NULL , NULL , } , } ,
#endif

	{ JISX0201_KATA , { "jisx0201.1976-0" , NULL , NULL , } , } ,
	{ JISX0201_ROMAN , { "jisx0201.1976-0" , NULL , NULL , } , } ,
	{ JISC6226_1978 , { "jisx0208.1978-0" , "jisx0208.1983-0" , NULL , } , } ,
	{ JISX0208_1983 , { "jisx0208.1983-0" , "jisx0208.1990-0" , NULL , } , } ,
	{ JISX0208_1990 , { "jisx0208.1990-0" , "jisx0208.1983-0" , NULL , } , } ,
	{ JISX0212_1990 , { "jisx0212.1990-0" , NULL , NULL , } , } ,
	{ JISX0213_2000_1 , { "jisx0213.2000-1" , NULL , NULL , } , } ,
	{ JISX0213_2000_2 , { "jisx0213.2000-2" , NULL , NULL , } , } ,
	{ KSC5601_1987 , { "ksc5601.1987-0" , "ksx1001.1997-0" , NULL , } , } ,

#if  0
	/*
	 * XXX
	 * UHC and JOHAB fonts are not used at the present time.
	 * see ml_vt100_parser.c:ml_parse_vt100_sequence().
	 */
	{ UHC , { NULL , NULL , NULL , } , } ,
	{ JOHAB , { "johabsh-1" , /* "johabs-1" , */ "johab-1" , NULL , } , } ,
#endif

	{ GB2312_80 , { "gb2312.1980-0" , NULL , NULL , } , } ,
	{ GBK , { "gbk-0" , NULL , NULL , } , } ,
	{ BIG5 , { "big5.eten-0" , "big5.hku-0" , NULL , } , } ,
	{ HKSCS , { "big5hkscs-0" , "big5-0" , NULL , } , } ,
	{ CNS11643_1992_1 , { "cns11643.1992-1" , "cns11643.1992.1-0" , NULL , } , } ,
	{ CNS11643_1992_2 , { "cns11643.1992-2" , "cns11643.1992.2-0" , NULL , } , } ,
	{ CNS11643_1992_3 , { "cns11643.1992-3" , "cns11643.1992.3-0" , NULL , } , } ,
	{ CNS11643_1992_4 , { "cns11643.1992-4" , "cns11643.1992.4-0" , NULL , } , } ,
	{ CNS11643_1992_5 , { "cns11643.1992-5" , "cns11643.1992.5-0" , NULL , } , } ,
	{ CNS11643_1992_6 , { "cns11643.1992-6" , "cns11643.1992.6-0" , NULL , } , } ,
	{ CNS11643_1992_7 , { "cns11643.1992-7" , "cns11643.1992.7-0" , NULL , } , } ,

} ;

static int  compose_dec_special_font ;
#ifdef  USE_TYPE_XFT
static char *  xft_size_type = XFT_PIXEL_SIZE ;
static double  dpi_for_xft ;
#endif


/* --- static functions --- */

static cs_info_t *
get_cs_info(
	mkf_charset_t  cs
	)
{
	int  count ;

	for( count = 0 ; count < sizeof( cs_info_table) / sizeof( cs_info_t) ;
		count ++)
	{
		if( cs_info_table[count].cs == cs)
		{
			return  &cs_info_table[count] ;
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " not supported cs(%x).\n" , cs) ;
#endif

	return  NULL ;
}

static int
set_decsp_font(
	x_font_t *  font
	)
{
	/*
	 * freeing font->xfont or font->xft_font
	 */
#ifdef  USE_TYPE_XFT
	if( font->xft_font)
	{
		XftFontClose( font->display , font->xft_font) ;
		font->xft_font = NULL ;
	}
#endif
#ifdef  USE_TYPE_XCORE
	if( font->xfont)
	{
		XFreeFont( font->display , font->xfont) ;
		font->xfont = NULL ;
	}
#endif

	if( ( font->decsp_font = x_decsp_font_new( font->display , font->width , font->height ,
					font->height_to_baseline)) == NULL)
	{
		return  0 ;
	}

	/* decsp_font is impossible to draw double with. */
	font->is_double_drawing = 0 ;

	/* decsp_font is always fixed pitch. */
	font->is_proportional = 0 ;

	return  1 ;
}

#ifdef  USE_TYPE_XFT

static u_int
xft_calculate_char_width(
	Display *  display ,
	XftFont *  xfont ,
	const u_char *  ch ,		/* US-ASCII or Unicode */
	size_t  len
	)
{
	XGlyphInfo  extents ;

	if( len == 1)
	{
		XftTextExtents8( display , xfont , ch , 1 , &extents) ;
	}
	else if( len == 2)
	{
		XftChar16  xch ;

		xch = ((ch[0] << 8) & 0xff00) | (ch[1] & 0xff) ;

		XftTextExtents16( display , xfont , &xch , 1 , &extents) ;
	}
	else if( len == 4)
	{
		XftChar32  xch ;

		xch = ((ch[0] << 24) & 0xff000000) | ((ch[1] << 16) & 0xff0000) |
			((ch[2] << 8) & 0xff00) | (ch[3] & 0xff) ;

		XftTextExtents32( display , xfont , &xch , 1 , &extents) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " char size %d is too large.\n" , len) ;
	#endif

		return  0 ;
	}

	return  extents.xOff ;
}

static int
parse_xft_font_name(
	char **  font_family ,
	int *  font_weight ,	/* if weight is not specified in font_name , not changed. */
	int *  font_slant ,	/* if slant is not specified in font_name , not changed. */
	double *  font_size ,	/* if size is not specified in font_name , not changed. */
	char **  font_encoding ,/* if encoding is not specified in font_name , not changed. */
	u_int *  percent ,	/* if percent is not specified in font_name , not changed. */
	char *  font_name	/* modified by this function. */
	)
{
	char *  p ;
	size_t  len ;
	
	/*
	 * XftFont format.
	 * [Family]( [WEIGHT] [SLANT] [SIZE]-[Encoding]:[Percentage])
	 */

	*font_family = font_name ;
	
	p = font_name ;
	while( 1)
	{
		if( *p == '\\' && *(p + 1))
		{
			/*
			 * It seems that XftFont format allows hyphens to be escaped.
			 * (e.g. Foo\-Bold-iso10646-1)
			 */

			/* skip backslash */
			p ++ ;
		}
		else if( *p == '\0')
		{
			/* encoding and percentage is not specified. */
			
			*font_name = '\0' ;
			
			break ;
		}
		else if( *p == '-')
		{
			/* Parsing "-[Encoding]:[Percentage]" */
			
			*font_name = '\0' ;

			*font_encoding = ++p ;
			
			kik_str_sep( &p , ":") ;
			if( p)
			{
				if( ! kik_str_to_uint( percent , p))
				{
				#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG
						" Percentage(%s) is illegal.\n" , p) ;
				#endif
				}
			}
			
			break ;
		}
		else if( *p == ':')
		{
			/* Parsing ":[Percentage]" */
			
			*font_name = '\0' ;
			
			if( ! kik_str_to_uint( percent , p + 1))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" Percentage(%s) is illegal.\n" , p + 1) ;
			#endif
			}
			
			break ;
		}

		*(font_name++) = *(p++) ;
	}

	/*
	 * Parsing "[Family] [WEIGHT] [SLANT] [SIZE]".
	 * Following is the same as x_font_win32.c:parse_font_name()
	 * except XFT_*.
	 */

#if  0
	kik_debug_printf( "Parsing %s for [Family] [Weight] [Slant]\n" , *font_family) ;
#endif

	p = kik_str_chop_spaces( *font_family) ;
	len = strlen( p) ;
	while( len > 0)
	{
		size_t  step = 0 ;
		
		if( *p == ' ')
		{
			char *  orig_p ;

			orig_p = p ;
			do
			{
				p ++ ;
				len -- ;
			}
			while( *p == ' ') ;

			if( len == 0)
			{
				*orig_p = '\0' ;
				
				break ;
			}
			else
			{
				int  count ;
				struct
				{
					char *  style ;
					int  weight ;
					int  slant ;

				} styles[] =
				{
					/*
					 * Portable styles.
					 */
					/* slant */
					{ "italic" , 0 , XFT_SLANT_ITALIC , } ,
					/* weight */
					{ "bold" , XFT_WEIGHT_BOLD , 0 , } ,

					/*
					 * Hack for styles which can be returned by
					 * gtk_font_selection_dialog_get_font_name().
					 */
					/* slant */
					{ "oblique" , 0 , XFT_SLANT_OBLIQUE , } ,
					/* weight */
					{ "light" , /* e.g. "Bookman Old Style Light" */
						XFT_WEIGHT_LIGHT , 0 , } ,
					{ "semi-bold" , XFT_WEIGHT_DEMIBOLD , 0 , } ,
					{ "heavy" , /* e.g. "Arial Black Heavy" */
						XFT_WEIGHT_BLACK , 0 , }	,
					/* other */
					{ "semi-condensed" , /* XXX This style is ignored. */
						0 , 0 , } ,
				} ;
				float  size_f ;
				
				for( count = 0 ; count < sizeof(styles) / sizeof(styles[0]) ;
					count ++)
				{
					size_t  len_v ;

					len_v = strlen( styles[count].style) ;
					
					/* XXX strncasecmp is not portable? */
					if( len >= len_v &&
					    strncasecmp( p , styles[count].style , len_v) == 0)
					{
						*orig_p = '\0' ;
						step = len_v ;
						if( styles[count].weight)
						{
							*font_weight = styles[count].weight ;
						}
						else if( styles[count].slant)
						{
							*font_slant = styles[count].slant ;
						}

						goto  next_char ;
					}
				}
				
				if( sscanf( p , "%f" , &size_f) == 1)
				{
					/* If matched with %f, p has no more parameters. */

					*orig_p = '\0' ;
					*font_size = size_f ;

					break ;
				}
				else
				{
					step = 1 ;
				}
			}
		}
		else
		{
			step = 1 ;
		}

next_char:
		p += step ;
		len -= step ;
	}
	
	return  1 ;
}

static u_int
get_xft_col_width(
	x_font_t *  font ,
	double  fontsize_d ,
	u_int  percent ,
	u_int  letter_space
	)
{
#if  0
	XftFont *  xfont ;

	/*
	 * XXX
	 * DefaultScreen() should not be used , but ...
	 */
	if( ( xfont = XftFontOpen( font->display , DefaultScreen( font->display) ,
		XFT_FAMILY , XftTypeString , family ,
		xft_size_type , XftTypeDouble , fontsize ,
		XFT_WEIGHT , XftTypeInteger , weight ,
		XFT_SLANT , XftTypeInteger , slant ,
		XFT_ENCODING , XftTypeString , "iso8859-1" ,
		XFT_SPACING , XftTypeInteger , XFT_PROPORTIONAL , NULL)))
	{
		u_int  w_width ;

		w_width = xft_calculate_char_width( font->display , xfont , "W" , 1)
				+ letter_space ;

		XftFontClose( font->display , xfont) ;

	#if  0
		kik_debug_printf( "%s(%f): Max width is %d\n" , family , fontsize , w_width) ;
	#endif
	
		if( w_width > 0)
		{
			return  w_width ;
		}
	}
#endif

	if( percent > 0)
	{
		return  DIVIDE_ROUNDING(fontsize_d * font->cols * percent , 100 * 2) +
			letter_space ;
	}
	else if( font->is_var_col_width)
	{
		return  0 ;
	}
	else if( strcmp( xft_size_type , XFT_SIZE) == 0)
	{
		double  widthpix ;
		double  widthmm ;
		double  dpi ;

		widthpix = DisplayWidth( font->display , DefaultScreen(font->display)) ;
		widthmm = DisplayWidthMM( font->display , DefaultScreen(font->display)) ;

		if( dpi_for_xft)
		{
			dpi = dpi_for_xft ;
		}
		else
		{
			dpi = (widthpix * 254) / (widthmm * 10) ;
		}

		return  DIVIDE_ROUNDINGUP(dpi * fontsize_d * font->cols , 72 * 2) + letter_space ;
	}
	else
	{
		return  DIVIDE_ROUNDINGUP(fontsize_d * font->cols , 2) + letter_space ;
	}
}

static int
set_xft_font(
	x_font_t *  font ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set. */
	int  use_medium_for_bold ,	/* Not used for now. */
	u_int  letter_space ,
	int  aa_opt		/* 0 = default , 1 = enable , -1 = disable */
	)
{
	int  weight ;
	int  slant ;
	u_int  ch_width ;
	XftFont *  xfont ;
	char *  iso10646 ;
	
	iso10646 = "iso10646-1" ;

	/*
	 * weight and slant can be modified in parse_xft_font_name().
	 */
	 
	if( font->id & FONT_BOLD)
	{
		weight = XFT_WEIGHT_BOLD ;
	}
	else
	{
		weight = XFT_WEIGHT_MEDIUM ;
	}

	slant = XFT_SLANT_ROMAN ;

	if( fontname)
	{
		char *  p ;
		char *  font_family ;
		double  fontsize_d ;
		char *  font_encoding ;
		u_int  percent ;

		if( ( p = kik_str_alloca_dup( fontname)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
		#endif

			return  0 ;
		}

		fontsize_d = (double)fontsize ;
		font_encoding = iso10646 ;
		percent = 0 ;
		if( parse_xft_font_name( &font_family , &weight , &slant , &fontsize_d ,
						&font_encoding , &percent , p))
		{
			if( col_width == 0)
			{
				/* basic font (e.g. usascii) width */

				ch_width = get_xft_col_width( font , fontsize_d , percent ,
								letter_space) ;

				if( font->is_vertical)
				{
					/*
					 * !! Notice !!
					 * The width of full and half character font is the same.
					 */
					ch_width *= 2 ;
				}
			}
			else
			{
				if( font->is_vertical)
				{
					/*
					 * !! Notice !!
					 * The width of full and half character font is the same.
					 */
					ch_width = col_width ;
				}
				else
				{
					ch_width = col_width * font->cols ;
				}
			}

		#ifdef  DEBUG
			kik_debug_printf( "Loading font %s%s%s %f %d\n" , font_family ,
				weight == XFT_WEIGHT_BOLD ? ":Bold" :
					weight == XFT_WEIGHT_LIGHT ? " Light" : "" ,
				slant == XFT_SLANT_ITALIC ? ":Italic" : "" ,
				fontsize_d , font->is_var_col_width) ;
		#endif
		
			if( ch_width == 0)
			{
				/* Proportional (font->is_var_col_width is true) */

				if( ( xfont = XftFontOpen( font->display ,
						DefaultScreen( font->display) ,
						XFT_FAMILY , XftTypeString , font_family ,
						xft_size_type , XftTypeDouble , fontsize_d ,
						XFT_ENCODING , XftTypeString , font_encoding ,
						XFT_WEIGHT , XftTypeInteger , weight ,
						XFT_SLANT , XftTypeInteger , slant ,
						XFT_SPACING , XftTypeInteger , XFT_PROPORTIONAL ,
						aa_opt ? XFT_ANTIALIAS : NULL , XftTypeBool ,
							aa_opt == 1 ? True : False , NULL)))
				{
					/* letter_space is ignored in variable column width mode */
					ch_width = xft_calculate_char_width( font->display ,
							xfont , "W" , 1) ;

					goto  font_found ;
				}
			}
			else
			{
				if( ( xfont = XftFontOpen( font->display ,
						DefaultScreen( font->display) ,
						XFT_FAMILY , XftTypeString , font_family ,
						xft_size_type , XftTypeDouble , fontsize_d ,
						XFT_ENCODING , XftTypeString , font_encoding ,
						XFT_WEIGHT , XftTypeInteger , weight ,
						XFT_SLANT , XftTypeInteger , slant ,
						XFT_SPACING , XftTypeInteger , XFT_MONO ,
						XFT_CHAR_WIDTH , XftTypeInteger , ch_width ,
						aa_opt ? XFT_ANTIALIAS : NULL , XftTypeBool ,
							aa_opt == 1 ? True : False , NULL)) )
				{
					goto  font_found ;
				}
			}
		}

		kik_warn_printf( "Font %s (for size %f) couldn't be loaded.\n" ,
			fontname , fontsize_d) ;
	}

	if( col_width == 0)
	{
		/* basic font (e.g. usascii) width */

		ch_width = get_xft_col_width( font , (double)fontsize , 0 , letter_space) ;
	
		if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */
			ch_width *= 2 ;
		}
	}
	else
	{
		if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */
			ch_width = col_width ;
		}
		else
		{
			ch_width = col_width * font->cols ;
		}
	}

	if( ch_width == 0)
	{
		/* Proportional (font->is_var_col_width is true) */

		if( ( xfont = XftFontOpen( font->display , DefaultScreen( font->display) ,
				xft_size_type , XftTypeDouble , (double)fontsize ,
				XFT_ENCODING , XftTypeString , iso10646 ,
				XFT_WEIGHT , XftTypeInteger , weight ,
				XFT_SLANT , XftTypeInteger , slant ,
				XFT_SPACING , XftTypeInteger , XFT_PROPORTIONAL ,
				aa_opt ? XFT_ANTIALIAS : NULL , XftTypeBool ,
					aa_opt == 1 ? True : False , NULL)))
		{
			/* letter_space is ignored in variable column width mode. */
			ch_width = xft_calculate_char_width( font->display ,
					xfont , "W" , 1) ;

			goto  font_found ;
		}
	}
	else
	{
		if( ( xfont = XftFontOpen( font->display , DefaultScreen( font->display) ,
				xft_size_type , XftTypeDouble , (double)fontsize ,
				XFT_ENCODING , XftTypeString , iso10646 ,
				XFT_WEIGHT , XftTypeInteger , weight ,
				XFT_SLANT , XftTypeInteger , slant ,
				XFT_SPACING , XftTypeInteger , XFT_MONO ,
				XFT_CHAR_WIDTH , XftTypeInteger , ch_width ,
				aa_opt ? XFT_ANTIALIAS : NULL , XftTypeBool ,
					aa_opt == 1 ? True : False , NULL)) )
		{
			goto  font_found ;
		}
	}

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " XftFontOpen(%s) failed.\n" , fontname) ;
#endif

	return  0 ;

font_found:

#ifdef FC_EMBOLDEN /* Synthetic emboldening (fontconfig >= 2.3.0) */
	font->is_double_drawing = 0 ;
#else
	if( weight == XFT_WEIGHT_BOLD &&
		XftPatternGetInteger( xfont->pattern , XFT_WEIGHT , 0 , &weight) == XftResultMatch &&
		weight != XFT_WEIGHT_BOLD)
	{
		font->is_double_drawing = 1 ;
	}
	else
	{
		font->is_double_drawing = 0 ;
	}
#endif

	font->xft_font = xfont ;

	font->height = xfont->height ;
	font->height_to_baseline = xfont->ascent ;

	font->x_off = 0 ;
	font->width = ch_width ;

	font->is_proportional = font->is_var_col_width ;

	/*
	 * checking if font height/height_to_baseline member is sane.
	 * font width must be always sane.
	 */

	if( font->height == 0)
	{
		/* XXX this may be inaccurate. */
		font->height = fontsize ;
	}

	if( font->height_to_baseline == 0)
	{
		/* XXX this may be inaccurate. */
		font->height_to_baseline = fontsize ;
	}

	/*
	 * set_decsp_font() is called after dummy font is loaded to get font metrics.
	 * Since dummy font encoding is "iso8859-1", loading rarely fails.
	 */
	/* XXX dec specials must always be composed for now */
	if( /* compose_dec_special_font && */ FONT_CS(font->id) == DEC_SPECIAL)
	{
		return  set_decsp_font( font) ;
	}

	return  1 ;
}

#endif  /* USE_TYPE_XFT */

#ifdef  USE_TYPE_XCORE

static u_int
calculate_char_width(
	Display *  display ,
	XFontStruct *  xfont ,
	const u_char *  ch ,
	size_t  len
	)
{
	if( len == 1)
	{
		return  XTextWidth( xfont , ch , 1) ;
	}
	else if( len == 2)
	{
		XChar2b  c ;

		c.byte1 = ch[0] ;
		c.byte2 = ch[1] ;

		return  XTextWidth16( xfont , &c , 1) ;
	}
	else if( len == 4)
	{
		/* is UCS4 */

		XChar2b  c ;

		/* XXX dealing as UCS2 */

		c.byte1 = ch[2] ;
		c.byte2 = ch[3] ;

		return  XTextWidth16( xfont , &c , 1) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " char size %d is too large.\n" , len) ;
	#endif

		return  0 ;
	}
}

static int
parse_xfont_name(
	char **  font_xlfd ,
	char **  percent ,	/* NULL can be returned. */
	char *  font_name	/* Don't specify NULL. Broken in this function */
	)
{
	/*
	 * XftFont format.
	 * [Font XLFD](:[Percentage])
	 */

	/* kik_str_sep() never returns NULL because font_name isn't NULL. */
	*font_xlfd = kik_str_sep( &font_name , ":") ;
	
	/* may be NULL */
	*percent = font_name ;

	return  1 ;
}

static XFontStruct *
load_xfont(
	Display *  display ,
	const char *  family ,
	const char *  weight ,
	const char *  slant ,
	const char *  width ,
	u_int  fontsize ,
	const char *  spacing ,
	const char *  encoding
	)
{
	XFontStruct *  xfont ;
	char *  fontname ;
	size_t  max_len ;

	/* "+ 20" means the num of '-' , '*'(19byte) and null chars. */
	max_len = 3 /* gnu */ + strlen(family) + 7 /* unifont */ + strlen( weight) +
		strlen( slant) + strlen( width) + 2 /* lang */ + DIGIT_STR_LEN(fontsize) +
		strlen( spacing) + strlen( encoding) + 20 ;

	if( ( fontname = alloca( max_len)) == NULL)
	{
		return  NULL ;
	}

	kik_snprintf( fontname , max_len , "-*-%s-%s-%s-%s--%d-*-*-*-%s-*-%s" ,
		family , weight , slant , width , fontsize , spacing , encoding) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
#endif

	if( ( xfont = XLoadQueryFont( display , fontname)))
	{
		return  xfont ;
	}

	if( strcmp( encoding , "iso10646-1") == 0 && strcmp( family , "biwidth") == 0)
	{
		/* XFree86 Unicode font */

		kik_snprintf( fontname , max_len , "-*-*-%s-%s-%s-%s-%d-*-*-*-%s-*-%s" ,
			weight , slant , width , kik_get_lang() , fontsize , spacing , encoding) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
	#endif

		if( ( xfont = XLoadQueryFont( display , fontname)))
		{
			return  xfont ;
		}

		if( strcmp( kik_get_lang() , "ja") != 0)
		{
			kik_snprintf( fontname , max_len , "-*-*-%s-%s-%s-ja-%d-*-*-*-%s-*-%s" ,
				weight , slant , width , fontsize , spacing , encoding) ;

		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
		#endif

			if( ( xfont = XLoadQueryFont( display , fontname)))
			{
				return  xfont ;
			}
		}

		/* GNU Unifont */

		kik_snprintf( fontname , max_len , "-gnu-unifont-%s-%s-%s--%d-*-*-*-%s-*-%s" ,
			weight , slant , width , fontsize , spacing , encoding) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " loading %s.\n" , fontname) ;
	#endif

		return  XLoadQueryFont( display , fontname) ;
	}
	else
	{
		return  NULL ;
	}
}

static int
set_xfont(
	x_font_t *  font ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,	/* if usascii font wants to be set , 0 will be set */
	int  use_medium_for_bold ,
	u_int  letter_space
	)
{
	XFontStruct *  xfont ;
	char *  weight ;
	char *  slant ;
	char *  width ;
	char *  family ;
	cs_info_t *  csinfo ;
	char **  font_encoding_p ;
	u_int  percent ;

	if( ( csinfo = get_cs_info( FONT_CS(font->id))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " get_cs_info(cs %x(id %x)) failed.\n" ,
			FONT_CS(font->id) , font->id) ;
	#endif

		return  0 ;
	}

	if( use_medium_for_bold)
	{
		font->is_double_drawing = 1 ;
	}
	else
	{
		font->is_double_drawing = 0 ;
	}

	if( fontname)
	{
		char *  p ;
		char *  font_xlfd ;
		char *  percent_str ;

		if( ( p = kik_str_alloca_dup( fontname)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
		#endif

			return  0 ;
		}

		if( parse_xfont_name( &font_xlfd , &percent_str , p))
		{
		#ifdef __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " loading %s font.\n" , font_xlfd) ;
		#endif

			if( ( xfont = XLoadQueryFont( font->display , font_xlfd)))
			{
				if( percent_str == NULL || ! kik_str_to_uint( &percent , percent_str))
				{
					percent = 0 ;
				}

				goto  font_found ;
			}

			kik_warn_printf( "Font %s couldn't be loaded.\n" , font_xlfd) ;
		}
	}

	percent = 0 ;

	/*
	 * searching apropriate font by using font info.
	 */

#ifdef  __DEBUG
	kik_debug_printf( "font for id %x will be loaded.\n" , font->id) ;
#endif

	if( font->id & FONT_BOLD)
	{
		weight = "bold" ;
	}
	else
	{
		weight = "medium" ;
	}

	slant = "r" ;

	width = "normal" ;

	if( font->id & FONT_BIWIDTH)
	{
		family = "biwidth" ;
	}
	else
	{
		family = "*" ;
	}

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = load_xfont( font->display , family , weight , slant ,
			width , fontsize , "c" , *font_encoding_p)))
		{
			goto  font_found ;
		}
	}

	if( font->id & FONT_BOLD)
	{
		FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
		{
			if( ( xfont = load_xfont( font->display , family , weight , "*" , "*" ,
				fontsize , "c" , *font_encoding_p)))
			{
				goto  font_found ;
			}
		}

		/*
		 * loading variable width font :(
		 */

		FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
		{
			if( ( xfont = load_xfont( font->display , family , weight , "*" , "*" ,
				fontsize , "m" , *font_encoding_p)))
			{
				goto   font_found ;
			}
		}

		FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
		{
			if( ( xfont = load_xfont( font->display , family , weight , "*" , "*" ,
				fontsize , "p" , *font_encoding_p)))
			{
				goto   font_found ;
			}
		}

		/* no bold font is found. */
		font->is_double_drawing = 1 ;
	}

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = load_xfont( font->display , family , "*" , "*" , "*" , fontsize ,
			"c" , *font_encoding_p)))
		{
			goto   font_found ;
		}
	}

	/*
	 * loading variable width font :(
	 */

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = load_xfont( font->display , family , "*" , "*" , "*" , fontsize ,
			"m" , *font_encoding_p)))
		{
			goto   font_found ;
		}
	}

	FOREACH_FONT_ENCODINGS(csinfo,font_encoding_p)
	{
		if( ( xfont = load_xfont( font->display , family , "*" , "*" , "*" , fontsize ,
			"p" , *font_encoding_p)))
		{
			goto   font_found ;
		}
	}

	return  0 ;

font_found:

	font->xfont = xfont ;

	font->height = xfont->ascent + xfont->descent ;
	font->height_to_baseline = xfont->ascent ;

	/*
	 * calculating actual font glyph width.
	 */
	if( ( FONT_CS(font->id) == ISO10646_UCS4_1 && ! (font->id & FONT_BIWIDTH)) ||
		FONT_CS(font->id) == TIS620_2533)
	{
		/*
		 * XXX hack
		 * a font including combining chars or an half width unicode font
		 * (which may include both half and full width glyphs).
		 * in this case , whether the font is proportional or not cannot be
		 * determined by comparing min_bounds and max_bounds.
		 * so , if `i' and `W' chars have different width , the font is regarded
		 * as proportional(and `W' width is used as font->width)
		 */

		u_int  w_width ;
		u_int  i_width ;

		w_width = calculate_char_width( font->display , font->xfont , "W" , 1) ;
		i_width = calculate_char_width( font->display , font->xfont , "i" , 1) ;

		if( w_width == 0)
		{
			font->is_proportional = 1 ;
			font->width = xfont->max_bounds.width ;
		}
		else if( i_width == 0 || w_width != i_width)
		{
			font->is_proportional = 1 ;
			font->width = w_width ;
		}
		else
		{
			font->is_proportional = 0 ;
			font->width = w_width ;
		}
	}
	else if( FONT_CS(font->id) == ISO10646_UCS4_1 && font->id & FONT_BIWIDTH)
	{
		/*
		 * XXX
		 * at the present time , all full width unicode fonts (which may include both
		 * half width and full width glyphs) are regarded as fixed.
		 * since I don't know what chars to be compared to determine font proportion
		 * and width.
		 */

		font->is_proportional = 0 ;
		font->width = xfont->max_bounds.width ;
	}
	else
	{
		if( xfont->max_bounds.width != xfont->min_bounds.width)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" max font width(%d) and min one(%d) are mismatched.\n" ,
				xfont->max_bounds.width , xfont->min_bounds.width) ;
		#endif

			font->is_proportional = 1 ;
		}
		else
		{
			font->is_proportional = 0 ;
		}

		font->width = xfont->max_bounds.width ;
	}

	font->x_off = 0 ;

	if( col_width == 0)
	{
		/* standard(usascii) font */

		if( percent > 0)
		{
			u_int  ch_width ;

			if( font->is_vertical)
			{
				/*
				 * !! Notice !!
				 * The width of full and half character font is the same.
				 */
				ch_width = DIVIDE_ROUNDING( fontsize * percent , 100) ;
			}
			else
			{
				ch_width = DIVIDE_ROUNDING( fontsize * percent , 200) ;
			}

			if( font->width != ch_width)
			{
				font->is_proportional = 1 ;

				if( font->width < ch_width)
				{
					/*
					 * If width(2) of '1' doesn't match ch_width(4)
					 * x_off = (4-2)/2 = 1.
					 * It means that starting position of drawing '1' is 1
					 * as follows.
					 *
					 *  0123
					 * +----+
					 * | ** |
					 * |  * |
					 * |  * |
					 * +----+
					 */
					font->x_off = (ch_width - font->width) / 2 ;
				}

				font->width = ch_width ;
			}
		}
		else if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */

			font->is_proportional = 1 ;
			font->x_off = font->width / 2 ;
			font->width *= 2 ;
		}

		/* letter_space is ignored in variable column width mode. */
		if( ! font->is_var_col_width && letter_space > 0)
		{
			font->is_proportional = 1 ;
			font->width += letter_space ;
			font->x_off += (letter_space / 2) ;
		}
	}
	else
	{
		/* not a standard(usascii) font */

		/*
		 * XXX hack
		 * forcibly conforming non standard font width to standard font width.
		 */

		if( font->is_vertical)
		{
			/*
			 * !! Notice !!
			 * The width of full and half character font is the same.
			 */

			if( font->width != col_width)
			{
				font->is_proportional = 1 ;

				if( font->width < col_width)
				{
					font->x_off = (col_width - font->width) / 2 ;
				}

				font->width = col_width ;
			}
		}
		else
		{
			if( font->width != col_width * font->cols)
			{
				kik_warn_printf(
					"Font width(%d) is not matched with standard width(%d)."
					"Characters are drawn one by one in order to fit"
					"standard width.\n" ,
					font->width , col_width * font->cols) ;

				font->is_proportional = 1 ;

				if( font->width < col_width * font->cols)
				{
					font->x_off = (col_width * font->cols - font->width) / 2 ;
				}

				font->width = col_width * font->cols ;
			}
		}
	}


	/*
	 * checking if font width/height/height_to_baseline member is sane.
	 */

	if( font->width == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " font width is 0.\n") ;
	#endif

		font->is_proportional = 1 ;

		/* XXX this may be inaccurate. */
		font->width = DIVIDE_ROUNDINGUP( fontsize * font->cols , 2) ;
	}

	if( font->height == 0)
	{
		/* XXX this may be inaccurate. */
		font->height = fontsize ;
	}

	if( font->height_to_baseline == 0)
	{
		/* XXX this may be inaccurate. */
		font->height_to_baseline = fontsize ;
	}

	/*
	 * set_decsp_font() is called after dummy font is loaded to get font metrics.
	 * Since dummy font encoding is "iso8859-1", loading rarely fails.
	 */
	if( compose_dec_special_font && FONT_CS(font->id) == DEC_SPECIAL)
	{
		return  set_decsp_font( font) ;
	}

	return  1 ;
}

#endif	/* USE_TYPE_XCORE */


/* --- global functions --- */

int
x_compose_dec_special_font(void)
{
	compose_dec_special_font = 1 ;

	return  1 ;
}

x_font_t *
x_font_new(
	Display *  display ,
	ml_font_t  id ,
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present ,
	const char *  fontname ,
	u_int  fontsize ,
	u_int  col_width ,
	int  use_medium_for_bold ,
	u_int  letter_space		/* ignored in variable column width mode. */
	)
{
	x_font_t *  font ;

	if( ( font = malloc( sizeof( x_font_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  NULL ;
	}

	font->display = display ;
	font->id = id ;

	if( (font->id & FONT_BIWIDTH) || IS_BIWIDTH_CS(FONT_CS(font->id)))
	{
		font->cols = 2 ;
	}
	else
	{
		font->cols = 1 ;
	}

	if( font_present & FONT_VAR_WIDTH)
	{
		font->is_var_col_width = 1 ;
	}
	else
	{
		font->is_var_col_width = 0 ;
	}

	if( font_present & FONT_VERTICAL)
	{
		font->is_vertical = 1 ;
	}
	else
	{
		font->is_vertical = 0 ;
	}

#ifdef  USE_TYPE_XCORE
	font->xfont = NULL ;
#endif
#ifdef  USE_TYPE_XFT
	font->xft_font = NULL ;
#endif
	font->decsp_font = NULL ;

	switch( type_engine)
	{
	default:
		return  NULL ;

#ifdef  USE_TYPE_XFT
	case  TYPE_XFT:
		if( ! set_xft_font( font , fontname , fontsize , col_width , use_medium_for_bold ,
				letter_space ,
				(font_present & FONT_AA) == FONT_AA ?
					1 : ((font_present & FONT_NOAA) == FONT_NOAA ? -1 : 0)))
		{
			free( font) ;

			return  NULL ;
		}

		break ;
#endif

#ifdef  USE_TYPE_XCORE
	case  TYPE_XCORE:
		if( font_present & FONT_AA)
		{
			return  NULL ;
		}
		else if( ! set_xfont( font , fontname , fontsize , col_width ,
				use_medium_for_bold , letter_space))
		{
			free( font) ;

			return  NULL ;
		}

		break ;
#endif
	}

#ifdef  DEBUG
	x_font_dump( font) ;
#endif

	return  font ;
}

int
x_font_delete(
	x_font_t *  font
	)
{
#ifdef  USE_TYPE_XFT
	if( font->xft_font)
	{
		XftFontClose( font->display , font->xft_font) ;
		font->xft_font = NULL ;
	}
#endif
#ifdef  USE_TYPE_XCORE
	if( font->xfont)
	{
		XFreeFont( font->display , font->xfont) ;
		font->xfont = NULL ;
	}
#endif

	if( font->decsp_font)
	{
		x_decsp_font_delete( font->decsp_font , font->display) ;
		font->decsp_font = NULL ;
	}

	free( font) ;

	return  1 ;
}

int
x_change_font_cols(
	x_font_t *  font ,
	u_int  cols	/* 0 means default value */
	)
{
	if( cols == 0)
	{
		if( (font->id & FONT_BIWIDTH) || IS_BIWIDTH_CS(FONT_CS(font->id)))
		{
			font->cols = 2 ;
		}
		else
		{
			font->cols = 1 ;
		}
	}
	else
	{
		font->cols = cols ;
	}

	return  1 ;
}

u_int
x_calculate_char_width(
	x_font_t *  font ,
	const u_char *  ch ,
	size_t  len ,
	mkf_charset_t  cs
	)
{
	if( font->is_var_col_width && font->is_proportional && ! font->decsp_font)
	{
	#ifdef  USE_TYPE_XFT
		if( font->xft_font)
		{
			u_char  ucs4[4] ;

			if( cs != US_ASCII)
			{
				if( ! ml_convert_to_xft_ucs4( ucs4 , ch , len , cs))
				{
					return  0 ;
				}

				ch = ucs4 ;
				len = 4 ;
			}

			return  xft_calculate_char_width( font->display , font->xft_font ,
								ch , len) ;
		}
	#endif
	#ifdef  USE_TYPE_XCORE
		if( font->xfont)
		{
			return  calculate_char_width( font->display , font->xfont , ch , len) ;
		}
	#endif

		kik_error_printf( KIK_DEBUG_TAG " couldn't calculate correct font width.\n") ;
	}

	return  font->width ;
}

char **
x_font_get_encoding_names(
	mkf_charset_t  cs
	)
{
	cs_info_t *  info ;

	if( ( info = get_cs_info( cs)))
	{
		return  info->encoding_names ;
	}
	else
	{
		return  NULL ;
	}
}

/* For mlterm-libvte */
void
x_font_use_point_size_for_xft(
	int  bool
	)
{
#ifdef  USE_TYPE_XFT
	if( bool)
	{
		xft_size_type = XFT_SIZE ;
	}
	else
	{
		xft_size_type = XFT_PIXEL_SIZE ;
	}
#endif
}

void
x_font_set_dpi_for_xft(
	double  dpi
	)
{
#ifdef  USE_TYPE_XFT
	dpi_for_xft = dpi ;
#endif
}

#ifdef  DEBUG

int
x_font_dump(
	x_font_t *  font
	)
{
#ifdef  USE_TYPE_XCORE
	kik_msg_printf( "Font id %x: XFont %p" , font->id , font->xfont) ;
#endif
#ifdef  USE_TYPE_XFT
	kik_msg_printf( "Font id %x: XftFont %p" , font->id , font->xft_font) ;
#endif

	kik_msg_printf( " (width %d, height %d, height_to_baseline %d, x_off %d)" ,
		font->width , font->height , font->height_to_baseline , font->x_off) ;

	if( font->is_proportional)
	{
		kik_msg_printf( " (proportional)") ;
	}

	if( font->is_var_col_width)
	{
		kik_msg_printf( " (var col width)") ;
	}

	if( font->is_vertical)
	{
		kik_msg_printf( " (vertical)") ;
	}
	
	if( font->is_double_drawing)
	{
		kik_msg_printf( " (double drawing)") ;
	}

	kik_msg_printf( "\n") ;

	return  1 ;
}

#endif
