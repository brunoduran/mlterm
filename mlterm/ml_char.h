/*
 *	$Id$
 */

#ifndef  __ML_CHAR_H__
#define  __ML_CHAR_H__


#include  <kiklib/kik_types.h>
#include  <kiklib/kik_def.h>	/* WORDS_BIGENDIAN */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */

#include  "ml_font.h"
#include  "ml_color.h"


#define  MAX_COMB_SIZE	7	/* Used in ml_shape.c,x_screen.c */
#define  UTF_MAX_SIZE	6
/*
 * XXX
 * char prefixes are max 4 bytes.
 * additional 3 bytes + cs name len ("viscii1.1-1" is max 11 bytes) = 14 bytes for iso2022
 * extension.
 * char length is max 2 bytes.
 * (total 20 bytes)
 */
#define  XCT_MAX_SIZE   20
#define  MLCHAR_UTF_MAX_SIZE  (UTF_MAX_SIZE * (MAX_COMB_SIZE + 1))
#define  MLCHAR_XCT_MAX_SIZE  (XCT_MAX_SIZE * (MAX_COMB_SIZE + 1))


/* For inline pictures (see x_picture.c) */
#define  PICTURE_CHARSET  0x1ff


/*
 * This object size should be kept as small as possible.
 * (ILP32: 64bit) (LP64: 64bit)
 *
 * If LSB of ml_char_t.u.ch.attr is 0,
 * ml_char_t.u.ch is invalid.
 * ml_char_t.u.multi_ch -> ml_char_t [main char]
 *                      -> ml_char_t [first combining char]
 *                      -> ml_char_t [second combining char]
 *                      .....
 */
typedef struct ml_char
{
	union
	{
		struct
		{
			/*
			 * attr member contents.
			 * Total 23 bit
			 * 3 bit : not used
			 * 3 bit : unicode area(0x0 - 0x7)
			 * 1 bit : is_italic(0 or 1)
			 * 1 bit : is_bold(0 or 1)
			 * 1 bit : is_fullwidth(0 or 1)
			 * 9 bit : charset(0x0 - 0x1ff)
			 * 1 bit : is_reversed(0 or 1)	... used for X Selection
			 * 1 bit : is_underlined(0 or 1)
			 * 1 bit : is_comb(0 or 1)
			 * 1 bit : is_comb_trailing(0 or 1)
			 * ---
			 * 1 bit : is_single_ch(0 or 1)
			 */
		#ifdef  WORDS_BIGENDIAN
			u_int  code: 23 ;
			u_int  fg_color: 9 ;
			u_int  bg_color: 9 ;
			u_int  attr: 23 ;
		#else
			u_int  attr: 23 ;
			u_int  fg_color: 9 ;
			u_int  bg_color: 9 ;
			u_int  code: 23 ;
		#endif
		} ch ;

		/*
		 * 32 bits(on ILP32) or 64 bits(on LP64).
		 * LSB(used for is_single_ch) is considered 0.
		 */
		struct ml_char *  multi_ch ;

	} u ;
	
} ml_char_t ;


int  ml_set_use_char_combining( int  use_it) ;

int  ml_set_use_multi_col_char( int  use_it) ;

ml_font_t  ml_char_add_unicode_area_font( u_int32_t  min , u_int32_t  max) ;

int  ml_char_init( ml_char_t *  ch) ;

int  ml_char_final( ml_char_t *  ch) ;

int  ml_char_set( ml_char_t *  ch , u_int32_t  code , mkf_charset_t  cs ,
	int  is_fullwidth , int  is_comb , ml_color_t  fg_color , ml_color_t  bg_color ,
	int  is_bold , int  is_italic , int  is_underlined) ;

int  ml_char_combine( ml_char_t *  ch , u_int32_t  code , mkf_charset_t  cs ,
	int  is_fullwidth , int  is_comb , ml_color_t  fg_color , ml_color_t  bg_color ,
	int  is_bold , int  is_italic , int  is_underlined) ;

int  ml_char_combine_simple( ml_char_t *  ch , ml_char_t *  comb) ;

ml_char_t *  ml_get_base_char( ml_char_t *  ch) ;

ml_char_t *  ml_get_combining_chars( ml_char_t *  ch , u_int *  size) ;

#if  0
/*
 * Not used for now.
 */
int  ml_char_move( ml_char_t *  dst , ml_char_t *  src) ;
#endif

int  ml_char_copy( ml_char_t *  dst , ml_char_t *  src) ;

u_int32_t  ml_char_code( ml_char_t *  ch) ;

int  ml_char_set_code( ml_char_t *  ch , u_int32_t  code) ;

mkf_charset_t  ml_char_cs( ml_char_t *  ch) ;

int  ml_char_is_comb( ml_char_t *  ch) ;

ml_font_t  ml_char_font( ml_char_t *  ch) ;

u_int  ml_char_cols( ml_char_t *  ch) ;

u_int  ml_char_is_fullwidth( ml_char_t *  ch) ;

ml_color_t  ml_char_fg_color( ml_char_t *  ch) ;

int  ml_char_set_fg_color( ml_char_t *  ch , ml_color_t  color) ;

ml_color_t  ml_char_bg_color( ml_char_t *  ch) ;

int  ml_char_set_bg_color( ml_char_t *  ch , ml_color_t  color) ;

int  ml_char_is_underlined( ml_char_t *  ch) ;

int  ml_char_reverse_color( ml_char_t *  ch) ;

int  ml_char_restore_color( ml_char_t *  ch) ;

int  ml_char_copy_color_reversed_flag( ml_char_t *  dst , ml_char_t *  src) ;

int  ml_char_is_null( ml_char_t *  ch) ;

int  ml_char_equal( ml_char_t *  ch1 , ml_char_t *  ch2) ;

int  ml_char_code_is( ml_char_t *  ch , u_int32_t  code , mkf_charset_t  cs) ;

int  ml_char_code_equal( ml_char_t *  ch1 , ml_char_t *  ch2) ;

ml_char_t *  ml_sp_ch(void) ;

ml_char_t *  ml_nl_ch(void) ;

#ifdef  DEBUG

void  ml_char_dump( ml_char_t *  ch) ;

#endif


#endif
