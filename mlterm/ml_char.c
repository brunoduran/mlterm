/*
 *	$Id$
 */

#include  "ml_char.h"

#include  <string.h>		/* memset/memcpy */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>	/* K_MIN */

#include  "ml_char_encoding.h"	/* IS_BIWIDTH_CS */


/*
 * !! Notice !!
 * Internal size representation is as follows.
 * 0x0 = size 1 , 0x1 = size 2 , 0x2 = size 3 , 0x3 = size 4.
 */
#define  SIZE(attr) ((((attr) >> 27) & 0x3) + 1)

#define  CHARSET(attr)  (((attr) >> 16) & 0x7ff)

#define  IS_BIWIDTH(attr)  (((attr) >> 15) & 0x1)

#define  COMB_SIZE(attr)  (((attr) >> 12) & 0x7)

#define  SET_COMB_SIZE(attr,size)  ((attr) = (((attr) & 0xffff8fff) | ((size) << 12)))

#define  IS_COMB(attr)  (((attr) >> 11) & 0x1)

/*
 * !! Notice !!
 * Internal color representation is as follows.
 * ML_BLACK - ML_WHITE: as they are
 * ML_FG_COLOR        : 0x8
 * ML_BG_COLOR        : 0x9
 */
#define  INTERN_COLOR(color) \
	((color) = ((color) == ML_FG_COLOR ? 0x8 : \
		(color) == ML_BG_COLOR ? 0x9 : (color) & ~ML_BOLD_COLOR_MASK))

#define  EXTERN_COLOR(color) \
	((color) = ((color) == 0x8 ? ML_FG_COLOR : \
		(color) == 0x9 ? ML_BG_COLOR : (color)))
	
#define  FG_COLOR(attr)  (((attr) >> 7) & 0xf)

#define  SET_FG_COLOR(attr,color)  ((attr) = (((attr) & 0xfffff87f) | ((color) << 7)))

#define  BG_COLOR(attr)  (((attr) >> 3) & 0xf)

#define  SET_BG_COLOR(attr,color)  ((attr) = (((attr) & 0xffffff87) | ((color) << 3)))

#define  IS_BOLD(attr)  (((attr) >> 2) & 0x1)

#define  IS_UNDERLINED(attr)  (((attr) >> 1) & 0x1)

#define  IS_REVERSED(attr)  ((attr) & 0x1)

#define  REVERSE_COLOR(attr) ((attr) |= 0x1)

#define  RESTORE_COLOR(attr) ((attr) &= ~0x1)

#define  COMPOUND_ATTR(size,charset,is_biwidth,comb_size,is_comb,fg_color,bg_color,is_bold,is_underlined,is_reversed) \
	( ((size - 1) << 27) | ((charset) << 16) | ((is_biwidth) << 15) | \
	((comb_size) << 12) | ((is_comb) << 11) | ((fg_color) << 7) | \
	((bg_color) << 3) | ((is_bold) << 2) | ((is_underlined) << 1) | (is_reversed) )


/* --- static variables --- */

static int  use_char_combining = 1 ;
static int  use_multi_col_char = 1 ;


/* --- global functions --- */

int
ml_use_char_combining(void)
{
	use_char_combining = 1 ;

	return  1 ;
}

int
ml_unuse_char_combining(void)
{
	use_char_combining = 0 ;

	return  1 ;
}

int
ml_use_multi_col_char(void)
{
	use_multi_col_char = 1 ;

	return  1 ;
}

int
ml_unuse_multi_col_char(void)
{
	use_multi_col_char = 0 ;

	return  1 ;
}


/*
 * string functions
 */
 
int
ml_str_init(
	ml_char_t *  str ,
	u_int  size
	)
{
	memset( str , 0 , sizeof( ml_char_t) * size) ;

	return  1 ;
}

ml_char_t *
__ml_str_init(
	ml_char_t *  str ,	/* alloca()-ed memory (see ml_char.h) */
	u_int  size
	)
{
	if( str == NULL)
	{
		/* alloca() failed. */
		
		return  NULL ;
	}

	if( ! ( ml_str_init( str , size)))
	{
		return  NULL ;
	}

	return  str ;
}
	
ml_char_t *
ml_str_new(
	u_int  size
	)
{
	ml_char_t *  str ;

	if( ( str = malloc( sizeof( ml_char_t) * size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif
	
		return  NULL ;
	}

	if( ml_str_init( str , size) == 0)
	{
		free( str) ;
		
		return  NULL ;
	}

	return  str ;
}

int
ml_str_final(
	ml_char_t *  str ,
	u_int  size
	)
{
	int  count ;

	for( count = 0 ; count < size ; count ++)
	{
		ml_char_final( &str[count]) ;
	}

	return  1 ;
}

int
ml_str_delete(
	ml_char_t *  str ,
	u_int  size
	)
{
	if( ml_str_final( str , size))
	{
		free( str) ;

		return  1 ;
	}
	else
	{
		free( str) ;

		return  0 ;
	}
}

/*
 * dst and src may overlap.
 */
inline int
ml_str_copy(
	ml_char_t *  dst ,
	ml_char_t *  src ,
	u_int  size
	)
{
	int  count ;

	if( size == 0 || dst == src)
	{
		return  0 ;
	}

	if( dst < src)
	{
		for( count = 0 ; count < size ; count ++)
		{
			ml_char_copy( dst++ , src++) ;
		}
	}
	else if( dst > src)
	{
		dst += size ;
		src += size ;
		for( count = 0 ; count < size ; count ++)
		{
			ml_char_copy( --dst  , --src) ;
		}
	}

	return  1 ;
}

inline u_int
ml_str_cols(
	ml_char_t *  chars ,
	u_int  len
	)
{
	int  count ;
	u_int  cols ;

	cols = 0 ;

	for( count = 0 ; count < len ; count ++)
	{
		cols += ml_char_cols( &chars[count]) ;
	}

	return  cols ;
}

inline int
ml_str_bytes_equal(
	ml_char_t *  str1 ,
	ml_char_t *  str2 ,
	u_int  len
	)
{
	int  count ;

	for( count = 0 ; count < len ; count ++)
	{
		if( ! ml_char_bytes_equal( &str1[count] , &str2[count]))
		{
			return  0 ;
		}
	}

	return  1 ;
}

#ifdef  DEBUG

void
ml_str_dump(
	ml_char_t *  chars ,
	u_int  len
	)
{
	int  count ;

	for( count = 0 ; count < len ; count ++)
	{
		ml_char_dump( &chars[count]) ;
	}

	kik_msg_printf( "\n") ;
}

#endif


/*
 * character functions
 */

inline int
ml_char_init(
	ml_char_t *  ch
	)
{
	memset( ch , 0 , sizeof( ml_char_t)) ;

	return  1 ;
}

inline int
ml_char_final(
	ml_char_t *  ch
	)
{
	if( COMB_SIZE(ch->attr) > 0)
	{
		free( ch->u.multi_ch) ;
	}

	return  1 ;
}

inline int
ml_char_set(
	ml_char_t *  ch ,
	u_char *  bytes ,
	size_t  size ,
	mkf_charset_t  cs ,
	int  is_biwidth ,
	int  is_comb ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_bold ,
	int  is_underlined
	)
{
	ml_char_final( ch) ;

	memcpy( ch->u.bytes , bytes , size) ;

	INTERN_COLOR(fg_color) ;
	INTERN_COLOR(bg_color) ;
	
	ch->attr = COMPOUND_ATTR(size,cs,is_biwidth,0,is_comb,fg_color,bg_color,is_bold,is_underlined,0) ;

	return  1 ;
}

inline int
ml_char_combine(
	ml_char_t *  ch ,
	u_char *  bytes ,
	size_t  size ,
	mkf_charset_t  cs ,
	int  is_biwidth ,
	int  is_comb ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_bold ,
	int  is_underlined
	)
{
	ml_char_t *  multi_ch ;
	u_int  comb_size ;

	if( ! use_char_combining || COMB_SIZE(ch->attr) >= MAX_COMB_SIZE)
	{
		return  0 ;
	}
	
	if( COMB_SIZE(ch->attr) == 0)
	{
		if( ( multi_ch = malloc( sizeof( ml_char_t) * 2)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
		#endif

			return  0 ;
		}

		ml_str_init( multi_ch , 2) ;

		ml_char_copy( &multi_ch[0] , ch) ;

		if( ml_char_set( &multi_ch[1] , bytes , size , cs , is_biwidth , is_comb ,
			fg_color , bg_color , is_bold , is_underlined) == 0)
		{
			return  0 ;
		}

		comb_size = 1 ;
	}
	else
	{
		comb_size = COMB_SIZE(ch->attr) ;
		
		if( ( multi_ch = realloc( ch->u.multi_ch , sizeof( ml_char_t) * (comb_size + 2))) == NULL)
		{
			return  0 ;
		}

		ml_char_init( &multi_ch[comb_size + 1]) ;

		if( ml_char_set( &multi_ch[comb_size + 1] , bytes , size , cs , is_biwidth , is_comb ,
			fg_color , bg_color , is_bold , is_underlined) == 0)
		{
			return  0 ;
		}

		comb_size ++ ;
	}

	ch->u.multi_ch = multi_ch ;

	SET_COMB_SIZE(ch->attr,comb_size) ;

	return  1 ;
}

inline int
ml_combine_chars(
	ml_char_t *  ch ,
	ml_char_t *  comb
	)
{
	return  ml_char_combine( ch , ml_char_bytes( comb) , SIZE(comb->attr) , CHARSET(comb->attr) ,
			IS_BIWIDTH(comb->attr) , IS_COMB(comb->attr) ,
			FG_COLOR(comb->attr) , BG_COLOR(comb->attr) ,
			IS_BOLD(comb->attr) , IS_UNDERLINED(comb->attr)) ;
}

inline ml_char_t *
ml_get_base_char(
	ml_char_t *  ch
	)
{
	if( COMB_SIZE(ch->attr) > 0)
	{
		return  &ch->u.multi_ch[0] ;
	}
	else
	{
		return  ch ;
	}
}

inline ml_char_t *
ml_get_combining_chars(
	ml_char_t *  ch ,
	u_int *  size
	)
{
	if( ( *size = COMB_SIZE(ch->attr)) > 0)
	{
		return  &ch->u.multi_ch[1] ;
	}
	else
	{
		return  NULL ;
	}
}

inline int
ml_char_move(
	ml_char_t *  dst ,
	ml_char_t *  src
	)
{
	if( dst == src)
	{
		return  0 ;
	}
	
	ml_char_final( dst) ;

	memcpy( dst , src , sizeof( ml_char_t)) ;
	memset( src , 0 , sizeof( ml_char_t)) ;

	return  1 ;
}

inline int
ml_char_copy(
	ml_char_t *  dst ,
	ml_char_t *  src
	)
{
	if( dst == src)
	{
		return  0 ;
	}
	
	ml_char_final( dst) ;

	memcpy( dst , src , sizeof( ml_char_t)) ;

	if( COMB_SIZE(src->attr) > 0)
	{
		ml_char_t *  multi_ch ;
		
		if( ( multi_ch = malloc( sizeof( ml_char_t) * (COMB_SIZE(src->attr) + 1))) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " failed to malloc.\n") ;
		#endif

			return  0 ;
		}

		memcpy( multi_ch , src->u.multi_ch , sizeof( ml_char_t) * (COMB_SIZE(src->attr) + 1)) ;

		dst->u.multi_ch = multi_ch ;
	}
	
	return  1 ;
}

inline u_char *
ml_char_bytes(
	ml_char_t *  ch
	)
{
	if( COMB_SIZE(ch->attr) > 0)
	{
		return  ch->u.multi_ch[0].u.bytes ;
	}
	else
	{
		return  ch->u.bytes ;
	}
}

inline int
ml_char_set_bytes(
	ml_char_t *  ch ,
	u_char *  bytes
	)
{
	if( COMB_SIZE(ch->attr) > 0)
	{
		ml_char_set_bytes( &ch->u.multi_ch[0] , bytes) ;
	}
	else
	{
		memcpy( ch->u.bytes , bytes , SIZE(ch->attr)) ;
	}
	
	return  1 ;
}

inline size_t
ml_char_size(
	ml_char_t *  ch
	)
{
	return  SIZE(ch->attr) ;
}

inline mkf_charset_t
ml_char_cs(
	ml_char_t *  ch
	)
{
	return  CHARSET(ch->attr) ;
}

inline ml_font_t
ml_char_font(
	ml_char_t *  ch
	)
{
	ml_font_t  font ;

	font = CHARSET(ch->attr) ;

	if( IS_BOLD(ch->attr))
	{
		font |= FONT_BOLD ;
	}

	if( IS_BIWIDTH(ch->attr))
	{
		font |= FONT_BIWIDTH ;
	}

	return  font ;
}

inline u_int
ml_char_cols(
	ml_char_t *  ch
	)
{
	if( use_multi_col_char &&
		(IS_BIWIDTH(ch->attr) || IS_BIWIDTH_CS( CHARSET(ch->attr))))
	{
		return  2 ;
	}
	else
	{
		return  1 ;
	}
}

inline int
ml_char_is_comb(
	ml_char_t *  ch
	)
{
	return  IS_COMB(ch->attr) ;
}

inline ml_color_t
ml_char_fg_color(
	ml_char_t *  ch
	)
{
	ml_color_t  color ;
	
	if( IS_REVERSED(ch->attr))
	{
		color = BG_COLOR(ch->attr) ;
	}
	else
	{
		color = FG_COLOR(ch->attr) ;
	}

	EXTERN_COLOR(color) ;

	if( color < MAX_VT_COLORS && IS_BOLD(ch->attr))
	{
		color |= ML_BOLD_COLOR_MASK ;
	}

	return  color ;
}

inline int
ml_char_set_fg_color(
	ml_char_t *  ch ,
	ml_color_t  color
	)
{
	if( COMB_SIZE(ch->attr) > 0)
	{
		int  count ;

		for( count = 0 ; count < COMB_SIZE(ch->attr) + 1 ; count ++)
		{
			ml_char_set_fg_color( &ch->u.multi_ch[count] , color) ;
		}
	}

	INTERN_COLOR(color) ;
	
	SET_FG_COLOR(ch->attr,color) ;

	return  1 ;
}

inline ml_color_t
ml_char_bg_color(
	ml_char_t *  ch
	)
{
	ml_color_t  color ;
	
	if( IS_REVERSED(ch->attr))
	{
		color = FG_COLOR(ch->attr) ;
	}
	else
	{
		color = BG_COLOR(ch->attr) ;
	}

	EXTERN_COLOR(color) ;
	
	return  color ;
}

inline int
ml_char_set_bg_color(
	ml_char_t *  ch ,
	ml_color_t  color
	)
{
	if( COMB_SIZE(ch->attr) > 0)
	{
		int  count ;

		for( count = 0 ; count < COMB_SIZE(ch->attr) + 1 ; count ++)
		{
			ml_char_set_bg_color( &ch->u.multi_ch[count] , color) ;
		}
	}

	INTERN_COLOR(color) ;

	SET_BG_COLOR(ch->attr,color) ;

	return  1 ;
}

inline int
ml_char_is_underlined(
	ml_char_t *  ch
	)
{
	return  IS_UNDERLINED(ch->attr) ;
}

inline int
ml_char_reverse_color(
	ml_char_t *  ch
	)
{
	if( IS_REVERSED(ch->attr))
	{
		return  0 ;
	}

	if( COMB_SIZE(ch->attr) > 0)
	{
		int  count ;

		for( count = 0 ; count < COMB_SIZE(ch->attr) + 1 ; count ++)
		{
			ml_char_reverse_color( &ch->u.multi_ch[count]) ;
		}
	}
	
	REVERSE_COLOR(ch->attr) ;
		
	return  1 ;
}

inline int
ml_char_restore_color(
	ml_char_t *  ch
	)
{
	if( ! IS_REVERSED(ch->attr))
	{
		return  0 ;
	}
	
	if( COMB_SIZE(ch->attr) > 0)
	{
		int  count ;

		for( count = 0 ; count < COMB_SIZE(ch->attr) + 1 ; count ++)
		{
			ml_char_restore_color( &ch->u.multi_ch[count]) ;
		}
	}

	RESTORE_COLOR(ch->attr) ;
	
	return  1 ;
}

int
ml_char_is_null(
	ml_char_t *  ch
	)
{
	return  (SIZE(ch->attr) == 1 && ch->u.bytes[0] == '\0') ;
}

inline int
ml_char_bytes_is(
	ml_char_t *  ch ,
	char *  bytes ,
	size_t  size ,
	mkf_charset_t  cs
	)
{
	if( CHARSET(ch->attr) == cs &&
		SIZE(ch->attr) == size &&
		memcmp( ml_char_bytes( ch) , bytes , size) == 0)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

inline int
ml_char_bytes_equal(
	ml_char_t *  ch1 ,
	ml_char_t *  ch2
	)
{
	u_int  size1 ;
	u_int  size2 ;
	ml_char_t *  comb1 ;
	ml_char_t *  comb2 ;
	u_int  comb1_size ;
	u_int  comb2_size ;
	int  count ;

	size1 = SIZE( ch1->attr) ;
	size2 = SIZE( ch2->attr) ;

	if( size1 != size2)
	{
		return  0 ;
	}

	if( memcmp( ml_char_bytes( ch1) , ml_char_bytes( ch2) , size1) != 0)
	{
		return  0 ;
	}
	
	comb1 = ml_get_combining_chars( ch1 , &comb1_size) ;
	comb2 = ml_get_combining_chars( ch2 , &comb2_size) ;

	if( comb1_size != comb2_size)
	{
		return  0 ;
	}

	for( count = 0 ; count < comb1_size ; count ++)
	{
		if( ! ml_char_bytes_equal( &comb1[count] , &comb2[count]))
		{
			return  0 ;
		}
	}

	return  1 ;
}

#ifdef  DEBUG

#if  0
#define  DUMP_HEX
#endif

/*
 * for debugging.
 */
void
ml_char_dump(
	ml_char_t *  ch
	)
{
	u_int  comb_size ;
	ml_char_t *  comb_chars ;
	
#ifdef  DUMP_HEX
	int  i ;
	
	kik_msg_printf( "[") ;
	for( i = 0 ; i < ml_char_size(ch) ; i ++)
	{
		kik_msg_printf( "%.2x" , ml_char_bytes(ch)[i]) ;
	}
	kik_msg_printf( "]") ;
#else
	if( ml_char_size(ch) == 2)
	{
		if( ml_char_cs(ch) == JISX0208_1983)
		{
			/* only eucjp */

			kik_msg_printf( "%c%c" , ml_char_bytes(ch)[0] | 0x80 ,
				ml_char_bytes(ch)[1] | 0x80) ;
		}
		else
		{
			kik_msg_printf( "**") ;
		}
	}
	else if( ml_char_size(ch) == 1)
	{
		kik_msg_printf( "%c" , ml_char_bytes(ch)[0]) ;
	}
	else
	{
		kik_msg_printf( "!!! unsupported char[0x%.2x len %d] !!!" ,
			ml_char_bytes(ch)[0] , SIZE(ch->attr)) ;
	}
#endif

	if( ( comb_chars = ml_get_combining_chars( ch , &comb_size)) != NULL)
	{
		int  count ;

		for( count = 0 ; count < comb_size ; count ++)
		{
			ml_char_dump( &comb_chars[count]) ;
		}
	}
}

#endif
