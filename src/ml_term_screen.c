/*
 *	$Id$
 */

#include  "ml_term_screen.h"

#include  <stdio.h>		/* sprintf */
#include  <unistd.h>            /* getcwd */
#include  <limits.h>            /* PATH_MAX */
#include  <X11/keysym.h>	/* XK_xxx */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_util.h>	/* K_MIN */
#include  <kiklib/kik_locale.h>	/* kik_get_locale */
#include  <mkf/mkf_xct_parser.h>
#include  <mkf/mkf_xct_conv.h>
#include  <mkf/mkf_utf8_conv.h>
#include  <mkf/mkf_utf8_parser.h>

#include  "ml_str_parser.h"
#include  "ml_xic.h"
#include  "ml_picture.h"


/*
 * XXX
 *
 * char length is max 8 bytes.
 *
 * combining chars may be max 3 per char.
 *
 * I think this is enough , but I'm not sure.
 */
#define  UTF8_MAX_CHAR_SIZE  (8 * 4)

/*
 * XXX
 *
 * char prefixes are max 4 bytes.
 * additional 3 bytes + cs name len ("viscii1.1-1" is max 11 bytes) = 14 bytes for iso2022
 * extension.
 * char length is max 2 bytes.
 * (total 20 bytes)
 *
 * combining chars is max 3 per char.
 *
 * I think this is enough , but I'm not sure.
 */
#define  XCT_MAX_CHAR_SIZE  (20 * 4)

/* the same as rxvt. if this size is too few , we may drop sequences from kinput2. */
#define  KEY_BUF_SIZE  512

#define  HAS_SYSTEM_LISTENER(termscr,function) \
	((termscr)->system_listener && (termscr)->system_listener->function)

#define  HAS_SCROLL_LISTENER(termscr,function) \
	((termscr)->screen_scroll_listener && (termscr)->screen_scroll_listener->function)


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static int
convert_row_to_y(
	ml_term_screen_t *  termscr ,
	int  row
	)
{
	/*
	 * !! Notice !!
	 * assumption: line hight is always the same!
	 */
	 
	return  ml_line_height( termscr->font_man) * row ;
}

static int
convert_y_to_row(
	ml_term_screen_t *  termscr ,
	u_int *  y_rest ,
	int  y
	)
{
	int  row ;

	/*
	 * !! Notice !!
	 * assumption: line hight is always the same!
	 */
	
	row = y / ml_line_height( termscr->font_man) ;

	if( y_rest)
	{
		*y_rest = y - row * ml_line_height( termscr->font_man) ;
	}

	return  row ;
}

static int
convert_char_index_to_x(
	ml_term_screen_t *  termscr ,
	ml_image_line_t *  line ,
	int  char_index
	)
{
	if( termscr->font_present & FONT_VAR_WIDTH) 
	{
		return  ml_convert_char_index_to_x( line , char_index , termscr->shape) ;
	}
	else
	{
		return  ml_convert_char_index_to_x( line , char_index , NULL) ;
	}
}

static int
convert_x_to_char_index(
	ml_term_screen_t *  termscr ,
	ml_image_line_t *  line ,
	u_int *  x_rest ,
	int  x
	)
{
	if( termscr->font_present & FONT_VAR_WIDTH)
	{
		return  ml_convert_x_to_char_index( line , x_rest , x , termscr->shape) ;
	}
	else
	{
		return  ml_convert_x_to_char_index( line , x_rest , x , NULL) ;
	}
}

static u_int
screen_width(
	ml_term_screen_t *  termscr
	)
{
	u_int  width ;
	
	/*
	 * logical cols/rows => visual width/height.
	 */
	 
	if( termscr->vertical_mode)
	{
		width = ml_term_model_get_logical_rows( termscr->model) *
				ml_col_width( termscr->font_man) ;
	}
	else
	{
		width = ml_term_model_get_logical_cols( termscr->model) *
				ml_col_width( termscr->font_man) ;
	}
	
	return  (width * termscr->screen_width_ratio) / 100 ;
}

static u_int
screen_height(
	ml_term_screen_t *  termscr
	)
{
	u_int  height ;
	
	/*
	 * logical cols/rows => visual width/height.
	 */
	 
	if( termscr->vertical_mode)
	{
		height = ml_term_model_get_logical_cols( termscr->model) *
				ml_line_height( termscr->font_man) ;
	}
	else
	{
		height = ml_term_model_get_logical_rows( termscr->model) *
				ml_line_height( termscr->font_man) ;
	}

	return  (height * termscr->screen_height_ratio) / 100 ;
}


/*
 * drawing screen functions.
 */
 
static int
draw_line(
	ml_term_screen_t *  termscr ,
	ml_image_line_t *  line ,
	int  y
	)
{
	if( ml_imgline_is_empty( line))
	{
		ml_window_clear( &termscr->window , 0 , y , termscr->window.width ,
			ml_line_height(termscr->font_man)) ;
	}
	else
	{
		int  beg_char_index ;
		int  beg_x ;
		u_int  num_of_redrawn ;
		ml_image_line_t *  orig ;

		if( termscr->shape)
		{
			if( ( orig = ml_imgline_shape( line , termscr->shape)) == NULL)
			{
				return  0 ;
			}
		}
		else
		{
			orig = NULL ;
		}
		
		beg_char_index = ml_imgline_get_beg_of_modified( line) ;
		
		/* 3rd argument is NULL since line is already shaped */
		beg_x = ml_convert_char_index_to_x( line , beg_char_index , NULL) ;

		if( termscr->font_present & FONT_VAR_WIDTH)
		{
			num_of_redrawn = line->num_of_filled_chars - beg_char_index ;
		}
		else
		{
			num_of_redrawn = ml_imgline_get_num_of_redrawn_chars( line) ;
		}

		if( ml_imgline_is_cleared_to_end( line) || (termscr->font_present & FONT_VAR_WIDTH))
		{
			if( ! ml_window_draw_str_to_eol( &termscr->window , &line->chars[beg_char_index] ,
				num_of_redrawn , beg_x , y ,
				ml_line_height( termscr->font_man) ,
				ml_line_height_to_baseline( termscr->font_man)))
			{
				return  0 ;
			}
		}
		else
		{
			if( ! ml_window_draw_str( &termscr->window , &line->chars[beg_char_index] ,
				num_of_redrawn , beg_x , y ,
				ml_line_height( termscr->font_man) ,
				ml_line_height_to_baseline( termscr->font_man)))
			{
				return  0 ;
			}
		}

		if( orig)
		{
			ml_imgline_unshape( line , orig) ;
		}
	}

	return  1 ;
}

static int
draw_cursor(
	ml_term_screen_t *  termscr ,
	int  restore
	)
{
	int  row ;
	int  x ;
	int  y ;
	ml_image_line_t *  line ;
	ml_image_line_t *  orig ;
	ml_char_t *  ch ;

	if( ( row = ml_convert_row_to_scr_row( termscr->model , ml_term_model_cursor_row( termscr->model)))
		== -1)
	{
		return  0 ;
	}
	
	y = convert_row_to_y( termscr , row) ;
	
	if( ( line = ml_term_model_cursor_line( termscr->model)) == NULL || ml_imgline_is_empty( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " cursor line doesn't exist.\n") ;
	#endif

		return  0 ;
	}

	if( termscr->shape)
	{
		if( ( orig = ml_imgline_shape( line , termscr->shape)) == NULL)
		{
			return  0 ;
		}
	}
	else
	{
		orig = NULL ;
	}
	
	/* 3rd argument is NULL since line is already shaped */
	x = ml_convert_char_index_to_x( line , ml_term_model_cursor_char_index( termscr->model) , NULL) ;
	
	ch = &line->chars[ ml_term_model_cursor_char_index( termscr->model)] ;

	if( restore)
	{
		ml_window_draw_str( &termscr->window , ch , 1 , x , y ,
			ml_line_height( termscr->font_man) ,
			ml_line_height_to_baseline( termscr->font_man)) ;
	}
	else
	{
		if( termscr->is_focused)
		{
			ml_window_draw_cursor( &termscr->window , ch , x , y ,
				ml_line_height( termscr->font_man) ,
				ml_line_height_to_baseline( termscr->font_man)) ;
		}
		else
		{
			ml_window_draw_rect_frame( &termscr->window ,
				x + 2 , y + 2 , x + ml_char_width(ch) + 1 , y + ml_char_height(ch) + 1) ;
		}
	}

	if( orig)
	{
		ml_imgline_unshape( line , orig) ;
	}

	return  1 ;
}

static int
flush_scroll_cache(
	ml_term_screen_t *  termscr ,
	int  scroll_actual_screen
	)
{
	if( ! termscr->scroll_cache_rows)
	{
		return  0 ;
	}
	
	if( scroll_actual_screen && ml_window_is_scrollable( &termscr->window))
	{
		int  start_y ;
		int  end_y ;
		u_int  scroll_height ;

		scroll_height = ml_line_height( termscr->font_man) * abs( termscr->scroll_cache_rows) ;

		if( scroll_height < termscr->window.height)
		{
			start_y = convert_row_to_y( termscr ,
				termscr->scroll_cache_boundary_start) ;
			end_y = start_y +
				ml_line_height( termscr->font_man) *
				(termscr->scroll_cache_boundary_end -
				termscr->scroll_cache_boundary_start + 1) ;

			if( termscr->scroll_cache_rows > 0)
			{
				ml_window_scroll_upward_region( &termscr->window ,
					start_y , end_y , scroll_height) ;
			}
			else
			{
				ml_window_scroll_downward_region( &termscr->window ,
					start_y , end_y , scroll_height) ;
			}
		}
	#if  0
		else
		{
			ml_window_clear_all( &termscr->window) ;
		}
	#endif
	}
	else
	{
		/*
		 * setting modified mark to the lines within scroll region.
		 */

		int  row ;
		ml_image_line_t *  line ;

		if( termscr->scroll_cache_rows > 0)
		{
			/*
			 * scrolling upward.
			 */
			for( row = termscr->scroll_cache_boundary_start ;
				row <= termscr->scroll_cache_boundary_end - termscr->scroll_cache_rows ;
				row ++)
			{
				if( ( line = ml_term_model_get_line( termscr->model , row)))
				{
					ml_imgline_set_modified_all( line) ;
				}
			}
		}
		else
		{
			/*
			 * scrolling downward.
			 */
			for( row = termscr->scroll_cache_boundary_end ;
				row >= termscr->scroll_cache_boundary_start - termscr->scroll_cache_rows ;
				row --)
			{
				if( ( line = ml_term_model_get_line( termscr->model , row)))
				{
					ml_imgline_set_modified_all( line) ;
				}
			}
		}
	}

	termscr->scroll_cache_rows = 0 ;

	return  1 ;
}

static void
set_scroll_boundary(
	ml_term_screen_t *  termscr ,
	int  boundary_start ,
	int  boundary_end
	)
{
	if( termscr->scroll_cache_rows &&
		(termscr->scroll_cache_boundary_start != boundary_start ||
		termscr->scroll_cache_boundary_end != boundary_end))
	{
		flush_scroll_cache( termscr , 0) ;
	}

	termscr->scroll_cache_boundary_start = boundary_start ;
	termscr->scroll_cache_boundary_end = boundary_end ;
}

static int
redraw_image(
	ml_term_screen_t *  termscr
	)
{
	int  counter ;
	ml_image_line_t *  line ;
	int  y ;
	int  end_y ;
	int  beg_y ;

	flush_scroll_cache( termscr , 1) ;

	counter = 0 ;
	while(1)
	{
		if( ( line = ml_term_model_get_line_in_screen( termscr->model , counter)) == NULL)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " nothing is redrawn.\n") ;
		#endif
		
			return  1 ;
		}
		
		if( ml_imgline_is_modified( line))
		{
			break ;
		}

		counter ++ ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " redrawing -> line %d\n" , counter) ;
#endif

	beg_y = end_y = y = convert_row_to_y( termscr , counter) ;

	draw_line( termscr , line , y) ;
	ml_imgline_updated( line) ;

	counter ++ ;
	y += ml_line_height( termscr->font_man) ;
	end_y = y ;

	while( ( line = ml_term_model_get_line_in_screen( termscr->model , counter)) != NULL)
	{
		if( ml_imgline_is_modified( line))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " redrawing -> line %d\n" , counter) ;
		#endif

			draw_line( termscr , line , y) ;
			ml_imgline_updated( line) ;

			y += ml_line_height( termscr->font_man) ;
			end_y = y ;
		}
		else
		{
			y += ml_line_height( termscr->font_man) ;
		}
	
		counter ++ ;
	}

	return  1 ;
}

static int
highlight_cursor(
	ml_term_screen_t *  termscr
	)
{
	if( ! termscr->is_cursor_visible)
	{
		return  1 ;
	}

	flush_scroll_cache( termscr , 1) ;

	ml_term_model_highlight_cursor( termscr->model) ;

	draw_cursor( termscr , 0) ;

	/* restoring color as soon as highlighted cursor is drawn. */
	ml_term_model_unhighlight_cursor( termscr->model) ;

	ml_xic_set_spot( &termscr->window) ;

	return  1 ;
}

static int
unhighlight_cursor(
	ml_term_screen_t *  termscr
	)
{
	if( ! termscr->is_cursor_visible)
	{
		return  1 ;
	}
	
	flush_scroll_cache( termscr , 1) ;

	draw_cursor( termscr , 1) ;
	
	return  1 ;
}


/*
 * {enter|exit}_backscroll_mode() and bs_XXX() functions provides backscroll operations.
 *
 * Similar processing is done in ml_term_screen_scroll_{upward|downward|to}().
 */
 
static void
enter_backscroll_mode(
	ml_term_screen_t *  termscr
	)
{
	if( ml_is_backscroll_mode( termscr->model))
	{
		return ;
	}
	
	ml_set_backscroll_mode( termscr->model) ;

	if( HAS_SCROLL_LISTENER(termscr,bs_mode_entered))
	{
		(*termscr->screen_scroll_listener->bs_mode_entered)(
			termscr->screen_scroll_listener->self) ;
	}
}

static void
exit_backscroll_mode(
	ml_term_screen_t *  termscr
	)
{
	if( ! ml_is_backscroll_mode( termscr->model))
	{
		return ;
	}
	
	ml_unset_backscroll_mode( termscr->model) ;
	
	ml_term_model_set_modified_all( termscr->model) ;
	
	if( HAS_SCROLL_LISTENER(termscr,bs_mode_exited))
	{
		(*termscr->screen_scroll_listener->bs_mode_exited)(
			termscr->screen_scroll_listener->self) ;
	}
}

static void
bs_scroll_upward(
	ml_term_screen_t *  termscr
	)
{
	unhighlight_cursor( termscr) ;
	
	if( ml_term_model_backscroll_upward( termscr->model , 1))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_upward))
		{
			(*termscr->screen_scroll_listener->scrolled_upward)(
				termscr->screen_scroll_listener->self , 1) ;
		}
	}
	
	highlight_cursor( termscr) ;
}

static void
bs_scroll_downward(
	ml_term_screen_t *  termscr
	)
{
	unhighlight_cursor( termscr) ;
	
	if( ml_term_model_backscroll_downward( termscr->model , 1))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_downward))
		{
			(*termscr->screen_scroll_listener->scrolled_downward)(
				termscr->screen_scroll_listener->self , 1) ;
		}
	}
	
	highlight_cursor( termscr) ;
}

static void
bs_half_page_upward(
	ml_term_screen_t *  termscr
	)
{
	unhighlight_cursor( termscr) ;
	
	if( ml_term_model_backscroll_upward( termscr->model , ml_term_model_get_rows( termscr->model) / 2))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_upward))
		{
			(*termscr->screen_scroll_listener->scrolled_upward)(
				termscr->screen_scroll_listener->self ,
				ml_term_model_get_rows( termscr->model) / 2) ;
		}
	}
	
	highlight_cursor( termscr) ;
}

static void
bs_half_page_downward(
	ml_term_screen_t *  termscr
	)
{
	unhighlight_cursor( termscr) ;
	
	if( ml_term_model_backscroll_downward( termscr->model , ml_term_model_get_rows( termscr->model) / 2))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_downward))
		{
			(*termscr->screen_scroll_listener->scrolled_downward)(
				termscr->screen_scroll_listener->self ,
				ml_term_model_get_rows( termscr->model) / 2) ;
		}
	}
	
	highlight_cursor( termscr) ;
}

static void
bs_page_upward(
	ml_term_screen_t *  termscr
	)
{
	unhighlight_cursor( termscr) ;
	
	if( ml_term_model_backscroll_upward( termscr->model , ml_term_model_get_rows( termscr->model)))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_upward))
		{
			(*termscr->screen_scroll_listener->scrolled_upward)(
				termscr->screen_scroll_listener->self ,
				ml_term_model_get_rows( termscr->model)) ;
		}
	}
	
	highlight_cursor( termscr) ;
}

static void
bs_page_downward(
	ml_term_screen_t *  termscr
	)
{
	unhighlight_cursor( termscr) ;
	
	if( ml_term_model_backscroll_downward( termscr->model , ml_term_model_get_rows( termscr->model)))
	{
		redraw_image( termscr) ;
		
		if( HAS_SCROLL_LISTENER(termscr,scrolled_downward))
		{
			(*termscr->screen_scroll_listener->scrolled_downward)(
				termscr->screen_scroll_listener->self ,
				ml_term_model_get_rows( termscr->model)) ;
		}
	}
	
	highlight_cursor( termscr) ;
}

 
static ml_picture_modifier_t *
get_picture_modifier(
	ml_term_screen_t *  termscr
	)
{
	if( termscr->pic_mod.brightness == 100)
	{
		return  NULL ;
	}
	else
	{
		return  &termscr->pic_mod ;
	}
}


/*
 * !! Notice !!
 * don't call ml_restore_selected_region_color() directly.
 */
static void
restore_selected_region_color(
	ml_term_screen_t *  termscr
	)
{
	if( ml_restore_selected_region_color( &termscr->sel))
	{
		highlight_cursor( termscr) ;
	}
}


static void
write_to_pty(
	ml_term_screen_t *  termscr ,
	u_char *  str ,			/* str may be NULL */
	size_t  len ,
	mkf_parser_t *  parser		/* parser may be NULL */
	)
{
	if( termscr->pty == NULL)
	{
		return ;
	}

	/*
	 * this is necessary since ml_image_t or ml_logs_t is changed.
	 */
	restore_selected_region_color( termscr) ;
	exit_backscroll_mode( termscr) ;
	
	if( parser && str)
	{
		(*parser->init)( parser) ;
		(*parser->set_str)( parser , str , len) ;
	}

	(*termscr->encoding_listener->init)( termscr->encoding_listener->self , 0) ;

	if( parser)
	{
		u_char  conv_buf[512] ;
		size_t  filled_len ;

		while( ! parser->is_eos)
		{
		#ifdef  __DEBUG
			{
				int  i ;

				kik_debug_printf( KIK_DEBUG_TAG " written str:\n") ;
				for( i = 0 ; i < len ; i ++)
				{
					kik_msg_printf( "[%.2x]" , str[i]) ;
				}
				kik_msg_printf( "=>\n") ;
			}
		#endif

			if( ( filled_len = (*termscr->encoding_listener->convert)(
				termscr->encoding_listener->self , conv_buf , sizeof( conv_buf) , parser))
				== 0)
			{
				break ;
			}

		#ifdef  __DEBUG
			{
				int  i ;

				for( i = 0 ; i < filled_len ; i ++)
				{
					kik_msg_printf( "[%.2x]" , conv_buf[i]) ;
				}
				kik_msg_printf( "\n") ;
			}
		#endif

			ml_write_to_pty( termscr->pty , conv_buf , filled_len) ;
		}
	}
	else if( str)
	{
	#ifdef  __DEBUG
		{
			int  i ;

			kik_debug_printf( KIK_DEBUG_TAG " written str: ") ;
			for( i = 0 ; i < len ; i ++)
			{
				kik_msg_printf( "%.2x" , str[i]) ;
			}
			kik_msg_printf( "\n") ;
		}
	#endif

		ml_write_to_pty( termscr->pty , str , len) ;
	}
	else
	{
		return ;
	}
}


static int
set_wall_picture(
	ml_term_screen_t *  termscr
	)
{
	ml_picture_t  pic ;
	
	if( ! termscr->pic_file_path)
	{
		return  0 ;
	}
	
	if( ! ml_picture_init( &pic , &termscr->window , get_picture_modifier( termscr)))
	{
		goto  error ;
	}

	if( ! ml_picture_load_file( &pic , termscr->pic_file_path))
	{
		kik_msg_printf( " wall picture file %s is not found.\n" ,
			termscr->pic_file_path) ;

		ml_picture_final( &pic) ;
		
		goto  error ;
	}
	
	if( ! ml_window_set_wall_picture( &termscr->window , pic.pixmap))
	{
		ml_picture_final( &pic) ;
		
		goto  error ;
	}
	else
	{
		ml_picture_final( &pic) ;

		return  1 ;
	}

error:
	free( termscr->pic_file_path) ;
	termscr->pic_file_path = NULL ;

	ml_window_unset_wall_picture( &termscr->window) ;

	return  0 ;
}

static int
get_mod_meta_mask(
	Display *  display
	)
{
	int  mask_counter ;
	int  kc_counter ;
	XModifierKeymap *  mod_map ;
	KeyCode *  key_codes ;
	KeySym  sym ;
	int  mod_masks[] = { Mod1Mask , Mod2Mask , Mod3Mask , Mod4Mask , Mod5Mask } ;

	mod_map = XGetModifierMapping( display) ;
	key_codes = mod_map->modifiermap ;
	
	for( mask_counter = 0 ; mask_counter < 6 ; mask_counter++)
	{
		int  counter ;

		/*
		 * KeyCodes order is like this.
		 * Shift[max_keypermod] Lock[max_keypermod] Control[max_keypermod]
		 * Mod1[max_keypermod] Mod2[max_keypermod] Mod3[max_keypermod]
		 * Mod4[max_keypermod] Mod5[max_keypermod]
		 */

		/* skip shift/lock/control */
		kc_counter = (mask_counter + 3) * mod_map->max_keypermod ;
		
		for( counter = 0 ; counter < mod_map->max_keypermod ; counter++)
		{
			if( key_codes[kc_counter] == 0)
			{
				break ;
			}

			sym = XKeycodeToKeysym( display , key_codes[kc_counter] , 0) ;

			if( sym == XK_Meta_L || sym == XK_Meta_R ||
				sym == XK_Alt_L || sym == XK_Alt_R ||
				sym == XK_Super_L || sym == XK_Super_R ||
				sym == XK_Hyper_L || sym == XK_Hyper_R)
			{
				XFreeModifiermap( mod_map) ;
				
				return  mod_masks[mask_counter] ;
			}

			kc_counter ++ ;
		}
	}
	
	XFreeModifiermap( mod_map) ;

	return  0 ;
}

static int
select_iscii_lang(
	ml_term_screen_t *  termscr
	)
{
	u_int  font_size ;
	char *  font_name ;

	if( termscr->iscii_state == NULL)
	{
		return  0 ;
	}

	if( ! ml_iscii_select_lang( termscr->iscii_state , termscr->iscii_lang))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_select_lang() failed.\n") ;
	#endif
	
		return  0 ;
	}

	for( font_size = termscr->font_man->font_custom->min_font_size ;
		font_size <= termscr->font_man->font_custom->max_font_size ;
		font_size ++)
	{
		if( ( font_name = ml_iscii_get_font_name( termscr->iscii_state , font_size)) == NULL)
		{
			continue ;
		}
		
		ml_font_manager_set_local_font_name( termscr->font_man ,
			DEFAULT_FONT_ATTR(ISCII) , font_name , font_size) ;
	}

	/*
	 * XXX
	 * anti alias ISCII font is not supported.
	 */
	 	
	if( ! (termscr->font_present & FONT_AA))
	{
		ml_font_manager_reload( termscr->font_man) ;
	}
	
	return  1 ;
}

/*
 * updating  ml_logical_visual_t/ml_shape_t/ml_iscii_state_t
 */
static int
update_encoding_proper_aux(
	ml_term_screen_t *  termscr ,
	int  is_visual
	)
{
	ml_logical_visual_t *  logvis ;

	ml_term_model_delete_logical_visual( termscr->model) ;
	
	if( termscr->shape)
	{
		(*termscr->shape->delete)( termscr->shape) ;
		termscr->shape = NULL ;
	}

	if( (*termscr->encoding_listener->encoding)(termscr->encoding_listener->self) == ML_ISCII)
	{
		/*
		 * It is impossible to process ISCII with other encoding proper auxes.
		 */
		 
		if( termscr->iscii_state == NULL)
		{
			if( ( termscr->iscii_state = ml_iscii_new()) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_new() failed.\n") ;
			#endif
				
				return  0 ;
			}

			if( ! select_iscii_lang( termscr))
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " select_iscii_lang() failed.\n") ;
			#endif

				goto  iscii_error ;
			}
		}
		
		if( ( logvis = ml_logvis_iscii_new( termscr->model->image , termscr->iscii_state)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_iscii_new() failed.\n") ;
		#endif

			goto  iscii_error ;
		}

		if( ! ml_term_model_add_logical_visual( termscr->model , logvis))
		{
			goto  iscii_error ;
		}

		if( ( termscr->shape = ml_iscii_shape_new( termscr->iscii_state)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_iscii_shape_new() failed.\n") ;
		#endif

			goto  iscii_error ;
		}

		goto  success ;

	iscii_error:
		ml_term_model_delete_logical_visual( termscr->model) ;
	
		if( termscr->iscii_state)
		{
			ml_iscii_delete( termscr->iscii_state) ;
			termscr->iscii_state = NULL ;
		}

		return  0 ;
	}
	else
	{
		ml_shape_t *  shape ;

		shape = NULL ;

		if( termscr->iscii_state)
		{
			ml_iscii_delete( termscr->iscii_state) ;
			termscr->iscii_state = NULL ;
		}

		if( termscr->use_dynamic_comb)
		{
			if( ( logvis = ml_logvis_comb_new( termscr->model->image)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_comb_new() failed.\n") ;
			#endif

				goto  error ;
			}

			if( ! ml_term_model_add_logical_visual( termscr->model , logvis))
			{
				goto  error ;
			}
		}
		
		if( termscr->use_bidi &&
			(*termscr->encoding_listener->encoding)(
				termscr->encoding_listener->self) == ML_UTF8)
		{
			if( ( logvis = ml_logvis_bidi_new( termscr->model->image)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_bidi_new() failed.\n") ;
			#endif

				goto  error ;
			}

			if( ! ml_term_model_add_logical_visual( termscr->model , logvis))
			{
				goto  error ;
			}
			
			if( ( shape = ml_arabic_shape_new()) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_arabic_shape_new() failed.\n") ;
			#endif

				goto  error ;
			}
		}

		if( termscr->vertical_mode)
		{
			if( ( logvis = ml_logvis_vert_new( termscr->model->image , termscr->vertical_mode))
					== NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_logvis_vert_new() failed.\n") ;
			#endif

				goto  error ;
			}

			if( ! ml_term_model_add_logical_visual( termscr->model , logvis))
			{
				goto  error ;
			}
			
			if( shape)
			{
				/*
				 * shaping processing is impossible in vertical mode.
				 */
				 
				(*shape->delete)( shape) ;
				shape = NULL ;
			}
		}
		
		termscr->shape = shape ;

		goto  success ;
		
	error:
		ml_term_model_delete_logical_visual( termscr->model) ;
		
		if( shape)
		{
			(*shape->delete)( termscr->shape) ;
		}

		return  0 ;
	}
	
success:
	if( is_visual)
	{
		ml_term_model_render( termscr->model) ;
		ml_term_model_visual( termscr->model) ;
	}

	return  1 ;
}


/*
 * callbacks of ml_window events
 */

static void
window_realized(
	ml_window_t *  win
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	termscr->mod_meta_mask = get_mod_meta_mask( termscr->window.display) ;

	if( termscr->xim_open_in_startup)
	{
		ml_xic_activate( &termscr->window , "" , "") ;
	}

	set_wall_picture( termscr) ;
}

static void
window_exposed(
	ml_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	int  counter ;
	int  beg_row ;
	int  end_row ;
	ml_term_screen_t *  termscr ;
	ml_image_line_t *  line ;
	
	termscr = (ml_term_screen_t *) win ;

	beg_row = convert_y_to_row( termscr , NULL , y) ;
	end_row = convert_y_to_row( termscr , NULL , y + height) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " exposed [row] from %d to %d [y] from %d to %d\n" ,
		beg_row , end_row , y , y + height) ;
#endif
	
	for( counter = beg_row ; counter <= end_row ; counter ++)
	{
		if( ( line = ml_term_model_get_line_in_screen( termscr->model , counter)) == NULL)
		{
			break ;
		}

		ml_imgline_set_modified_all( line) ;
	}
	
	redraw_image( termscr) ;

	if( ml_convert_row_to_abs_row( termscr->model , beg_row) <=
			ml_term_model_cursor_row( termscr->model) &&
		ml_term_model_cursor_row( termscr->model) <=
			ml_convert_row_to_abs_row( termscr->model , end_row))
	{
		highlight_cursor( termscr) ;
	}
}

static void
window_resized(
	ml_window_t *  win
	)
{
	ml_term_screen_t *  termscr ;
	u_int  rows ;
	u_int  cols ;
	u_int  width ;
	u_int  height ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " term screen resized => width %d height %d.\n" ,
		win->width , win->height) ;
#endif
	
	termscr = (ml_term_screen_t *) win ;

	/*
	 * this is necessary since ml_image_t size is changed.
	 */
	restore_selected_region_color( termscr) ;
	exit_backscroll_mode( termscr) ;

	unhighlight_cursor( termscr) ;

	/*
	 * !! Notice !!
	 * ml_image_resize() modifies screen image that visual to logical should be done
	 * before ml_image_resize() is called.
	 */
	ml_term_model_logical( termscr->model) ;

	/*
	 * visual width/height => logical cols/rows
	 */

	width = (termscr->window.width * 100) / termscr->screen_width_ratio ;
	height = (termscr->window.height * 100) / termscr->screen_height_ratio ;
	
	if( termscr->vertical_mode)
	{
		rows = width / ml_col_width( termscr->font_man) ;
		cols = height / ml_line_height( termscr->font_man) ;
	}
	else
	{
		cols = width / ml_col_width( termscr->font_man) ;
		rows = height / ml_line_height( termscr->font_man) ;
	}

	ml_term_model_resize( termscr->model , cols , rows) ;

	if( termscr->pty)
	{
		ml_set_pty_winsize( termscr->pty , cols , rows) ;
	}

	ml_term_model_render( termscr->model) ;
	ml_term_model_visual( termscr->model) ;
	
	set_wall_picture( termscr) ;

	redraw_image( termscr) ;

	highlight_cursor( termscr) ;
}

static void
window_focused(
	ml_window_t *  win
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t *) win ;

	if( termscr->is_focused)
	{
		return ;
	}
	
	unhighlight_cursor( termscr) ;

	termscr->is_focused = 1 ;
	
	ml_window_unfade( win) ;
	
	highlight_cursor( termscr) ;
}

static void
window_unfocused(
	ml_window_t *  win
	)
{
	ml_term_screen_t *  termscr ;
	
	termscr = (ml_term_screen_t *) win ;

	if( ! termscr->is_focused)
	{
		return ;
	}
	
	unhighlight_cursor( termscr) ;
	
	termscr->is_focused = 0 ;
	
	ml_window_fade( win , termscr->fade_ratio) ;

	highlight_cursor( termscr) ;
}

/*
 * the finalizer of ml_term_screen_t.
 * 
 * ml_window_final(term_screen) -> window_finalized(term_screen)
 */
static void
window_finalized(
	ml_window_t *  win
	)
{
	ml_term_screen_delete( (ml_term_screen_t*)win) ;
}

static void
window_deleted(
	ml_window_t *  win
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	if( HAS_SYSTEM_LISTENER(termscr,close_pty))
	{
		(*termscr->system_listener->close_pty)( termscr->system_listener->self ,
			ml_get_root_window( &termscr->window)) ;
	}
}

static void
config_menu(
	ml_term_screen_t *  termscr ,
	int  x ,
	int  y
	)
{
	int  global_x ;
	int  global_y ;
	Window  child ;
	ml_sb_mode_t  sb_mode ;
	char *  sb_view_name ;
	ml_color_t  sb_fg_color ;
	ml_color_t  sb_bg_color ;
	char *  wall_pic ;

	XTranslateCoordinates( termscr->window.display , termscr->window.my_window ,
		DefaultRootWindow( termscr->window.display) , x , y ,
		&global_x , &global_y , &child) ;

	if( HAS_SCROLL_LISTENER(termscr,fg_color))
	{
		sb_fg_color = (*termscr->screen_scroll_listener->fg_color)(
				termscr->screen_scroll_listener->self) ;
	}
	else
	{
		sb_fg_color = ML_UNKNOWN_COLOR ;
	}
	
	if( HAS_SCROLL_LISTENER(termscr,bg_color))
	{
		sb_bg_color = (*termscr->screen_scroll_listener->bg_color)(
				termscr->screen_scroll_listener->self) ;
	}
	else
	{
		sb_bg_color = ML_UNKNOWN_COLOR ;
	}
	
	if( HAS_SCROLL_LISTENER(termscr,sb_mode))
	{
		sb_mode = (*termscr->screen_scroll_listener->sb_mode)(
				termscr->screen_scroll_listener->self) ;
	}
	else
	{
		sb_mode = SB_NONE ;
	}
	
	if( HAS_SCROLL_LISTENER(termscr,view_name))
	{
		sb_view_name = (*termscr->screen_scroll_listener->view_name)(
				termscr->screen_scroll_listener->self) ;
	}
	else
	{
		sb_view_name = "none" ;
	}

	if( termscr->pic_file_path)
	{
		wall_pic = termscr->pic_file_path ;
	}
	else
	{
		wall_pic = "none" ;
	}

	ml_config_menu_start( &termscr->config_menu , global_x , global_y ,
		(*termscr->encoding_listener->encoding)( termscr->encoding_listener->self) ,
		termscr->iscii_lang ,
		ml_get_color_name( termscr->color_man , ml_window_get_fg_color( &termscr->window)) ,
		ml_get_color_name( termscr->color_man , ml_window_get_bg_color( &termscr->window)) ,
		ml_get_color_name( termscr->color_man , sb_fg_color) ,
		ml_get_color_name( termscr->color_man , sb_bg_color) ,
		ml_term_model_get_tab_size( termscr->model) , ml_term_model_get_log_size( termscr->model) ,
		termscr->font_man->font_size ,
		termscr->font_man->font_custom->min_font_size ,
		termscr->font_man->font_custom->max_font_size ,
		termscr->font_man->line_space ,
		termscr->screen_width_ratio , termscr->screen_height_ratio ,
		termscr->mod_meta_mode , termscr->bel_mode , termscr->vertical_mode , sb_mode ,
		ml_is_using_char_combining() , termscr->use_dynamic_comb , termscr->copy_paste_via_ucs ,
		termscr->window.is_transparent , termscr->pic_mod.brightness , termscr->fade_ratio ,
		termscr->font_present , termscr->font_man->use_multi_col_char , termscr->use_bidi ,
		sb_view_name , ml_xic_get_xim_name( &termscr->window) , kik_get_locale() , wall_pic) ;
}

static int
use_utf8_selection(
	ml_term_screen_t *  termscr
	)
{
	ml_char_encoding_t  encoding ;

	encoding = (*termscr->encoding_listener->encoding)( termscr->encoding_listener->self) ;

	if( encoding == UTF8)
	{
		return  1 ;
	}
	else if( IS_UCS_SUBSET_ENCODING(encoding) && termscr->copy_paste_via_ucs)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static int
yank_event_received(
	ml_term_screen_t *  termscr ,
	Time  time
	)
{
	if( termscr->sel.is_owner)
	{
		if( termscr->sel.sel_str == NULL || termscr->sel.sel_len == 0)
		{
			return  0 ;
		}
		
		(*termscr->ml_str_parser->init)( termscr->ml_str_parser) ;
		ml_str_parser_set_str( termscr->ml_str_parser ,
			termscr->sel.sel_str , termscr->sel.sel_len) ;
		
		write_to_pty( termscr , NULL , 0 , termscr->ml_str_parser) ;

		return  1 ;
	}
	else
	{
		if( use_utf8_selection(termscr))
		{
			return  ml_window_utf8_selection_request( &termscr->window , time) ;
		}
		else
		{
			return  ml_window_xct_selection_request( &termscr->window , time) ;
		}
	}
}

static void
key_pressed(
	ml_window_t *  win ,
	XKeyEvent *  event
	)
{
	ml_term_screen_t *  termscr ;
	size_t  size ;
	u_char  seq[KEY_BUF_SIZE] ;
	KeySym  ksym ;
	mkf_parser_t *  parser ;

	termscr = (ml_term_screen_t *) win ;

	size = ml_window_get_str( win , seq , sizeof(seq) , &parser , &ksym , event) ;

#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " received sequence =>") ;
		for( i = 0 ; i < size ; i ++)
		{
			kik_msg_printf( "%.2x" , seq[i]) ;
		}
		kik_msg_printf( "\n") ;
	}
#endif

	if( (*termscr->encoding_listener->encoding)( termscr->encoding_listener->self) == ML_ISCII)
	{
		ml_iscii_keyb_t  keyb ;

		keyb = ml_iscii_current_keyb( termscr->iscii_state) ;
		
		if( ksym == XK_Alt_R)
		{
			if( keyb == ISCIIKEYB_NONE)
			{
				ml_set_window_name( &termscr->window , "Inscript Keyb") ;
				ml_iscii_select_keyb( termscr->iscii_state , ISCIIKEYB_INSCRIPT) ;
			}
			else
			{
				ml_set_window_name( &termscr->window , "mlterm") ;
				ml_iscii_select_keyb( termscr->iscii_state , ISCIIKEYB_NONE) ;
			}

			return ;
		}
		else if( ksym == XK_F1 && keyb != ISCIIKEYB_NONE)
		{
			if( keyb == ISCIIKEYB_IITKEYB)
			{
				ml_set_window_name( &termscr->window , "Inscript Keyb") ;
				ml_iscii_select_keyb( termscr->iscii_state , ISCIIKEYB_INSCRIPT) ;
			}
			else if( keyb == ISCIIKEYB_INSCRIPT)
			{
				ml_set_window_name( &termscr->window , "Phonetic Keyb") ;
				ml_iscii_select_keyb( termscr->iscii_state , ISCIIKEYB_IITKEYB) ;
			}

			return ;
		}
	}
	
	if( ml_keymap_match( termscr->keymap , XIM_OPEN , ksym , event->state))
	{
		ml_xic_activate( &termscr->window , "" , "") ;

		return ;
	}
	else if( ml_keymap_match( termscr->keymap , XIM_CLOSE , ksym , event->state))
	{
		ml_xic_deactivate( &termscr->window) ;

		return ;
	}
	else if( ml_keymap_match( termscr->keymap , NEW_PTY , ksym , event->state))
	{
		if( HAS_SYSTEM_LISTENER(termscr,open_pty))
		{
			termscr->system_listener->open_pty( termscr->system_listener->self) ;
		}

		return ;
	}
#ifdef  DEBUG
	else if( ml_keymap_match( termscr->keymap , EXIT_PROGRAM , ksym , event->state))
	{
		if( HAS_SYSTEM_LISTENER(termscr,exit))
		{
			termscr->system_listener->exit( termscr->system_listener->self , 1) ;
		}

		return ;
	}
#endif

	if( ml_is_backscroll_mode( termscr->model))
	{
		if( termscr->use_extended_scroll_shortcut)
		{
			if( ml_keymap_match( termscr->keymap , SCROLL_UP , ksym , event->state))
			{
				bs_scroll_downward( termscr) ;

				return ;
			}
			else if( ml_keymap_match( termscr->keymap , SCROLL_DOWN , ksym , event->state))
			{
				bs_scroll_upward( termscr) ;

				return ;
			}
		#if  1
			else if( ksym == XK_u || ksym == XK_Prior)
			{
				bs_half_page_downward( termscr) ;

				return ;
			}
			else if( ksym == XK_d || ksym == XK_Next)
			{
				bs_half_page_upward( termscr) ;

				return ;
			}
			else if( ksym == XK_k || ksym == XK_Up)
			{
				bs_scroll_downward( termscr) ;

				return ;
			}
			else if( ksym == XK_j || ksym == XK_Down)
			{
				bs_scroll_upward( termscr) ;

				return ;
			}
		#endif
		}
		
		if( ml_keymap_match( termscr->keymap , PAGE_UP , ksym , event->state))
		{
			bs_half_page_downward( termscr) ;

			return ;
		}
		else if( ml_keymap_match( termscr->keymap , PAGE_DOWN , ksym , event->state))
		{
			bs_half_page_upward( termscr) ;

			return ;
		}
		else if( ksym == XK_Shift_L || ksym == XK_Shift_R || ksym == XK_Control_L ||
			ksym == XK_Control_R || ksym == XK_Caps_Lock || ksym == XK_Shift_Lock ||
			ksym == XK_Meta_L || ksym == XK_Meta_R || ksym == XK_Alt_L ||
			ksym == XK_Alt_R || ksym == XK_Super_L || ksym == XK_Super_R ||
			ksym == XK_Hyper_L || ksym == XK_Hyper_R || ksym == XK_Escape)
		{
			/* any modifier keys(X11/keysymdefs.h) */

			return ;
		}
		else
		{
			exit_backscroll_mode( termscr) ;
		}
	}

	if( termscr->use_extended_scroll_shortcut &&
		ml_keymap_match( termscr->keymap , SCROLL_UP , ksym , event->state))
	{
		enter_backscroll_mode( termscr) ;
		bs_scroll_downward( termscr) ;
	}
	else if( ml_keymap_match( termscr->keymap , PAGE_UP , ksym , event->state))
	{
		enter_backscroll_mode( termscr) ;
		bs_half_page_downward( termscr) ;
	}
	else if( ml_keymap_match( termscr->keymap , INSERT_SELECTION , ksym , event->state))
	{
		yank_event_received( termscr , CurrentTime) ;
	}
#ifdef  __DEBUG
	else if( ksym == XK_F12)
	{
		/* this is for tests of ml_image_xxx functions */

		/* ml_image_xxx( termscr->image) ; */

		redraw_image( termscr) ;
	}
#endif
	else
	{
		char *  buf ;

		if( termscr->use_vertical_cursor)
		{
			if( termscr->vertical_mode & VERT_RTL)
			{
				if( ksym == XK_Up)
				{
					ksym = XK_Left ;
				}
				else if( ksym == XK_Down)
				{
					ksym = XK_Right ;
				}
				else if( ksym == XK_Left)
				{
					ksym = XK_Down ;
				}
				else if( ksym == XK_Right)
				{
					ksym = XK_Up ;
				}
			}
			else if( termscr->vertical_mode & VERT_LTR)
			{
				if( ksym == XK_Up)
				{
					ksym = XK_Left ;
				}
				else if( ksym == XK_Down)
				{
					ksym = XK_Right ;
				}
				else if( ksym == XK_Left)
				{
					ksym = XK_Up ;
				}
				else if( ksym == XK_Right)
				{
					ksym = XK_Down ;
				}
			}
		}

		if( ksym == XK_Delete && size == 1)
		{
			buf = ml_termcap_get_sequence( termscr->termcap , MLT_DELETE) ;
		}
		else if( ksym == XK_BackSpace && size == 1)
		{
			buf = ml_termcap_get_sequence( termscr->termcap , MLT_BACKSPACE) ;
		}
		else if( size > 0)
		{
			buf = NULL ;
			
			if( termscr->iscii_state)
			{
				size = ml_convert_ascii_to_iscii(
					termscr->iscii_state , seq , size , seq , size) ;
			}
		}
		/*
		 * following ksym is processed only if no sequence string is received(size == 0)
		 */
		else if( ksym == XK_Up)
		{
			if( termscr->is_app_cursor_keys)
			{
				buf = "\x1bOA" ;
			}
			else
			{
				buf = "\x1b[A" ;
			}
		}
		else if( ksym == XK_Down)
		{
			if( termscr->is_app_cursor_keys)
			{
				buf = "\x1bOB" ;
			}
			else
			{
				buf = "\x1b[B" ;
			}
		}
		else if( ksym == XK_Right)
		{
			if( termscr->is_app_cursor_keys)
			{
				buf = "\x1bOC" ;
			}
			else
			{
				buf = "\x1b[C" ;
			}
		}
		else if( ksym == XK_Left)
		{
			if( termscr->is_app_cursor_keys)
			{
				buf = "\x1bOD" ;
			}
			else
			{
				buf = "\x1b[D" ;
			}
		}
		else if( ksym == XK_Prior)
		{
			buf = "\x1b[5~" ;
		}
		else if( ksym == XK_Next)
		{
			buf = "\x1b[6~" ;
		}
		else if( ksym == XK_Insert)
		{
			buf = "\x1b[2~" ;
		}
		else if( ksym == XK_F1)
		{
			buf = "\x1b[11~" ;
		}
		else if( ksym == XK_F2)
		{
			buf = "\x1b[12~" ;
		}
		else if( ksym == XK_F3)
		{
			buf = "\x1b[13~" ;
		}
		else if( ksym == XK_F4)
		{
			buf = "\x1b[14~" ;
		}
		else if( ksym == XK_F5)
		{
			buf = "\x1b[15~" ;
		}
		else if( ksym == XK_F6)
		{
			buf = "\x1b[17~" ;
		}
		else if( ksym == XK_F7)
		{
			buf = "\x1b[18~" ;
		}
		else if( ksym == XK_F8)
		{
			buf = "\x1b[19~" ;
		}
		else if( ksym == XK_F9)
		{
			buf = "\x1b[20~" ;
		}
		else if( ksym == XK_F10)
		{
			buf = "\x1b[21~" ;
		}
		else if( ksym == XK_F11)
		{
			buf = "\x1b[23~" ;
		}
		else if( ksym == XK_F12)
		{
			buf = "\x1b[24~" ;
		}
		else if( ksym == XK_F13)
		{
			buf = "\x1b[25~" ;
		}
		else if( ksym == XK_F14)
		{
			buf = "\x1b[26~" ;
		}
		else if( ksym == XK_F15)
		{
			buf = "\x1b[28~" ;
		}
		else if( ksym == XK_F16)
		{
			buf = "\x1b[29~" ;
		}
		else if( ksym == XK_Help)
		{
			buf = "\x1b[28~" ;
		}
		else if( ksym == XK_Menu)
		{
			buf = "\x1b[29~" ;
		}
	#if  0
		else if( termscr->is_app_keypad && ksym == XK_KP_Home)
		{
			buf = "\x1bOw" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Up)
		{
			buf = "\x1bOx" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Down)
		{
			buf = "\x1bOw" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Right)
		{
			buf = "\x1bOv" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Left)
		{
			buf = "\x1bOt" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Prior)
		{
			buf = "\x1bOy" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Next)
		{
			buf = "\x1bOs" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_End)
		{
			buf = "\x1bOq" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Enter)
		{
			buf = "\x1bOM" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Begin)
		{
			buf = "\x1bOu" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Insert)
		{
			buf = "\x1bOp" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Begin)
		{
			buf = "\x1bOu" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Delete)
		{
			buf = "\x1bOn" ;
		}
	#endif
		else if( termscr->is_app_keypad && ksym == XK_KP_F1)
		{
			buf = "\x1bOP" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_F2)
		{
			buf = "\x1bOQ" ;
		}		
		else if( termscr->is_app_keypad && ksym == XK_KP_F3)
		{
			buf = "\x1bOR" ;
		}		
		else if( termscr->is_app_keypad && ksym == XK_KP_F4)
		{
			buf = "\x1bOS" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Multiply)
		{
			buf = "\x1bOj" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Add)
		{
			buf = "\x1bOk" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Separator)
		{
			buf = "\x1bOl" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Subtract)
		{
			buf = "\x1bOm" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Decimal)
		{
			buf = "\x1bOn" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_Divide)
		{
			buf = "\x1bOo" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_0)
		{
			buf = "\x1bOp" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_1)
		{
			buf = "\x1bOq" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_2)
		{
			buf = "\x1bOr" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_3)
		{
			buf = "\x1bOs" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_4)
		{
			buf = "\x1bOt" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_5)
		{
			buf = "\x1bOu" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_6)
		{
			buf = "\x1bOv" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_7)
		{
			buf = "\x1bOw" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_8)
		{
			buf = "\x1bOx" ;
		}
		else if( termscr->is_app_keypad && ksym == XK_KP_9)
		{
			buf = "\x1bOy" ;
		}
		else
		{
			return ;
		}

		if( termscr->mod_meta_mask & event->state)
		{
			if( termscr->mod_meta_mode == MOD_META_OUTPUT_ESC)
			{
				write_to_pty( termscr , "\x1b" , 1 , NULL) ;
			}
			else if( termscr->mod_meta_mode == MOD_META_SET_MSB)
			{
				int  counter ;

				for( counter = 0 ; counter < size ; counter ++)
				{
					if( 0x20 <= seq[counter] && seq[counter] <= 0x7e)
					{
						seq[counter] |= 0x80 ;
					}
				}
			}
		}

		if( buf)
		{
			write_to_pty( termscr , buf , strlen(buf) , NULL) ;
		}
		else
		{
			write_to_pty( termscr , seq , size , parser) ;
		}
	}
}

static void
utf8_selection_request_failed(
	ml_window_t *  win ,
	XSelectionEvent *  event
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;
	
	/* UTF8_STRING selection request failed. retrying with XCOMPOUND_TEXT */
	ml_window_xct_selection_request( &termscr->window , event->time) ;
}

static void
selection_cleared(
	ml_window_t *  win ,
	XSelectionClearEvent *  event
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;
	
	if( termscr->sel.is_owner)
	{
		ml_sel_clear( &termscr->sel) ;
	}

	restore_selected_region_color( termscr) ;
}

static int
copy_paste_via_ucs(
	ml_term_screen_t *  termscr
	)
{
	ml_char_encoding_t  encoding ;

	encoding = (*termscr->encoding_listener->encoding)( termscr->encoding_listener->self) ;

	if( IS_UCS_SUBSET_ENCODING(encoding) && termscr->copy_paste_via_ucs)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static size_t
convert_selection_to_xct(
	ml_term_screen_t *  termscr ,
	u_char *  str ,
	size_t  len
	)
{
	size_t  filled_len ;
	
#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending internal str: ") ;
		for( i = 0 ; i < termscr->sel.sel_len ; i ++)
		{
			ml_char_dump( &termscr->sel.sel_str[i]) ;
		}
		kik_msg_printf( "\n -> converting to ->\n") ;
	}
#endif

	(*termscr->ml_str_parser->init)( termscr->ml_str_parser) ;
	ml_str_parser_set_str( termscr->ml_str_parser , termscr->sel.sel_str , termscr->sel.sel_len) ;
	
	(*termscr->xct_conv->init)( termscr->xct_conv) ;
	filled_len = (*termscr->xct_conv->convert)( termscr->xct_conv ,
		str , len , termscr->ml_str_parser) ;

#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending xct str: ") ;
		for( i = 0 ; i < filled_len ; i ++)
		{
			kik_msg_printf( "%.2x" , str[i]) ;
		}
		kik_msg_printf( "\n") ;
	}
#endif

	return  filled_len ;
}

static size_t
convert_selection_to_utf8(
	ml_term_screen_t *  termscr ,
	u_char *  str ,
	size_t  len
	)
{
	size_t  filled_len ;

#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending internal str: ") ;
		for( i = 0 ; i < termscr->sel.sel_len ; i ++)
		{
			ml_char_dump( &termscr->sel.sel_str[i]) ;
		}
		kik_msg_printf( "\n -> converting to ->\n") ;
	}
#endif

	(*termscr->ml_str_parser->init)( termscr->ml_str_parser) ;
	ml_str_parser_set_str( termscr->ml_str_parser , termscr->sel.sel_str , termscr->sel.sel_len) ;
	
	(*termscr->utf8_conv->init)( termscr->utf8_conv) ;
	filled_len = (*termscr->utf8_conv->convert)( termscr->utf8_conv ,
		str , len , termscr->ml_str_parser) ;
		
#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " sending utf8 str: ") ;
		for( i = 0 ; i < filled_len ; i ++)
		{
			kik_msg_printf( "%.2x" , str[i]) ;
		}
		kik_msg_printf( "\n") ;
	}
#endif

	return  filled_len ;
}

static void
xct_selection_requested(
	ml_window_t * win ,
	XSelectionRequestEvent *  event ,
	Atom  type
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	if( termscr->sel.sel_str == NULL || termscr->sel.sel_len == 0)
	{
		ml_window_send_selection( win , event , NULL , 0 , 0) ;
	}
	else
	{
		u_char *  xct_str ;
		size_t  xct_len ;
		size_t  filled_len ;

		xct_len = termscr->sel.sel_len * XCT_MAX_CHAR_SIZE ;

		if( ( xct_str = alloca( xct_len)) == NULL)
		{
			return ;
		}

		filled_len = convert_selection_to_xct( termscr , xct_str , xct_len) ;

		ml_window_send_selection( win , event , xct_str , filled_len , type) ;
	}
}

static void
utf8_selection_requested(
	ml_window_t * win ,
	XSelectionRequestEvent *  event ,
	Atom  type
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	if( termscr->sel.sel_str == NULL || termscr->sel.sel_len == 0)
	{		
		ml_window_send_selection( win , event , NULL , 0 , 0) ;
	}
	else
	{
		u_char *  utf8_str ;
		size_t  utf8_len ;
		size_t  filled_len ;
		
		utf8_len = termscr->sel.sel_len * UTF8_MAX_CHAR_SIZE ;

		if( ( utf8_str = alloca( utf8_len)) == NULL)
		{
			return ;
		}

		filled_len = convert_selection_to_utf8( termscr , utf8_str , utf8_len) ;

		ml_window_send_selection( win , event , utf8_str , filled_len , type) ;
	}
}

static void
xct_selection_notified(
	ml_window_t *  win ,
	u_char *  str ,
	size_t  len
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	if( copy_paste_via_ucs(termscr))
	{
		/* XCOMPOUND TEXT -> UCS -> PTY ENCODING */
		
		u_char  conv_buf[512] ;
		size_t  filled_len ;

		(*termscr->xct_parser->init)( termscr->xct_parser) ;
		(*termscr->xct_parser->set_str)( termscr->xct_parser , str , len) ;
		
		(*termscr->utf8_conv->init)( termscr->utf8_conv) ;

		while( ! termscr->xct_parser->is_eos)
		{
			if( ( filled_len = (*termscr->utf8_conv->convert)(
				termscr->utf8_conv , conv_buf , sizeof( conv_buf) ,
				termscr->xct_parser)) == 0)
			{
				break ;
			}

			write_to_pty( termscr , conv_buf , filled_len , termscr->utf8_parser) ;
		}
	}
	else
	{
		/* XCOMPOUND TEXT -> UCS -> PTY ENCODING */
		
		write_to_pty( termscr , str , len , termscr->xct_parser) ;
	}

	return ;
}

static void
utf8_selection_notified(
	ml_window_t *  win ,
	u_char *  str ,
	size_t  len
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	write_to_pty( termscr , str , len , termscr->utf8_parser) ;
}


static void
start_selection(
	ml_term_screen_t *  termscr ,
	int  col_r ,
	int  row_r
	)
{
	int  col_l ;
	int  row_l ;

	if( col_r == 0)
	{
		ml_image_line_t *  line ;
		
		if( ( line = ml_term_model_get_line_in_all( termscr->model , row_r - 1))
			== NULL || ml_imgline_is_empty( line))
		{
			col_l = col_r ;
			row_l = row_r ;
		}
		else
		{
			col_l = line->num_of_filled_chars - 1 ;
			row_l = row_r - 1 ;
		}
	}
	else
	{
		col_l = col_r - 1 ;
		row_l = row_r ;
	}

	ml_start_selection( &termscr->sel , col_l , row_l , col_r , row_r) ;
}

static void
selecting_with_motion(
	ml_term_screen_t *  termscr ,
	int  x ,
	int  y ,
	Time  time
	)
{
	int  char_index ;
	int  abs_row ;
	int  x_is_minus ;
	u_int  x_rest ;
	ml_image_line_t *  line ;

	x_is_minus = 0 ;
	if( x < 0)
	{
		x = 0 ;
		x_is_minus = 1 ;
	}
	else if( x > termscr->window.width)
	{
		x = termscr->window.width ;
	}
	
	if( y < 0)
	{
		if( ml_term_model_get_num_of_logged_lines( termscr->model) > 0)
		{
			if( ! ml_is_backscroll_mode( termscr->model))
			{
				enter_backscroll_mode( termscr) ;
			}
			
			bs_scroll_downward( termscr) ;
		}
		
		y = 0 ;
	}
	else if( y > termscr->window.height - ml_line_height( termscr->font_man))
	{
		if( ml_is_backscroll_mode( termscr->model))
		{
			bs_scroll_upward( termscr) ;
		}
		
		y = termscr->window.height - ml_line_height( termscr->font_man) ;
	}

	abs_row = ml_convert_row_to_abs_row( termscr->model , convert_y_to_row( termscr , NULL , y)) ;

	if( ( line = ml_term_model_get_line_in_all( termscr->model , abs_row)) == NULL ||
		ml_imgline_is_empty( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " image line(%d) not found.\n" , abs_row) ;
	#endif

		return ;
	}
	
	char_index = convert_x_to_char_index( termscr , line , &x_rest , x) ;

	if( line->num_of_filled_chars - 1 == char_index && x_rest)
	{
		/*
		 * XXX hack (anyway it works)
		 * this points to an invalid char , but by its secondary effect ,
		 * if mouse is dragged in a char-less area , nothing is selected.
		 */
		char_index ++ ;
	}

	if( ! termscr->sel.is_selecting)
	{
		restore_selected_region_color( termscr) ;
		
		if( ! termscr->sel.is_owner)
		{
			if( ml_window_set_selection_owner( &termscr->window , time) == 0)
			{
				return ;
			}
		}

		start_selection( termscr , char_index , abs_row) ;
	}
	else
	{
		if( ml_is_after_sel_right_base_pos( &termscr->sel , char_index , abs_row))
		{
			if( char_index > 0)
			{
				char_index -- ;
			}
		}
		else if( ml_is_before_sel_left_base_pos( &termscr->sel , char_index , abs_row))
		{
			if( ! x_is_minus && char_index < line->num_of_filled_chars - 1)
			{
				char_index ++ ;
			}
		}
		
		if( ml_selected_region_is_changed( &termscr->sel , char_index , abs_row , 1))
		{
			ml_selecting( &termscr->sel , char_index , abs_row) ;
		}
	}
}

static void
button_motion(
	ml_window_t *  win ,
	XMotionEvent *  event
	)
{
	ml_term_screen_t *  termscr ;
	
	termscr = (ml_term_screen_t*) win ;

	if( termscr->is_mouse_pos_sending && ! (event->state & ShiftMask))
	{
		return ;
	}

	selecting_with_motion( termscr , event->x , event->y , event->time) ;
}

static void
button_press_continued(
	ml_window_t *  win ,
	XButtonEvent *  event
	)
{
	ml_term_screen_t *  termscr ;
	
	termscr = (ml_term_screen_t*) win ;
	
	if( termscr->sel.is_selecting &&
		(event->y < 0 || win->height - ml_line_height( termscr->font_man) < event->y))
	{
		selecting_with_motion( termscr , event->x , event->y , event->time) ;
	}
}

static void
selecting_word(
	ml_term_screen_t *  termscr ,
	int  x ,
	int  y ,
	Time  time
	)
{
	int  char_index ;
	int  row ;
	u_int  x_rest ;
	int  beg_row ;
	int  beg_char_index ;
	int  end_row ;
	int  end_char_index ;
	ml_image_line_t *  line ;

	row = ml_convert_row_to_abs_row( termscr->model , convert_y_to_row( termscr , NULL , y)) ;

	if( ( line = ml_term_model_get_line_in_all( termscr->model , row)) == NULL ||
		ml_imgline_is_empty( line))
	{
		return ;
	}

	char_index = convert_x_to_char_index( termscr , line , &x_rest , x) ;

	if( line->num_of_filled_chars - 1 == char_index && x_rest)
	{
		/* over end of line */

		return ;
	}

	if( ml_term_model_get_word_region( termscr->model , &beg_char_index , &beg_row , &end_char_index ,
		&end_row , char_index , row) == 0)
	{
		return ;
	}

	if( ! termscr->sel.is_selecting)
	{
		restore_selected_region_color( termscr) ;
		
		if( ! termscr->sel.is_owner)
		{
			if( ml_window_set_selection_owner( &termscr->window , time) == 0)
			{
				return ;
			}
		}
		
		start_selection( termscr , beg_char_index , beg_row) ;
	}

	ml_selecting( &termscr->sel , end_char_index , end_row) ;
}

static void
selecting_line(
	ml_term_screen_t *  termscr ,
	int  y ,
	Time  time
	)
{
	int  row ;
	int  beg_row ;
	int  end_char_index ;
	int  end_row ;

	row = ml_convert_row_to_abs_row( termscr->model , convert_y_to_row( termscr , NULL , y)) ;

	if( ml_term_model_get_line_region( termscr->model , &beg_row , &end_char_index , &end_row , row)
		== 0)
	{
		return ;
	}
	
	if( ! termscr->sel.is_selecting)
	{
		restore_selected_region_color( termscr) ;
		
		if( ! termscr->sel.is_owner)
		{
			if( ml_window_set_selection_owner( &termscr->window , time) == 0)
			{
				return ;
			}
		}
		
		start_selection( termscr , 0 , beg_row) ;
	}

	ml_selecting( &termscr->sel , end_char_index , end_row) ;
}

static int
report_mouse_tracking(
	ml_term_screen_t *  termscr ,
	XButtonEvent *  event ,
	int  is_released
	)
{
	ml_image_line_t *  line ;
	int  button ;
	int  key_state ;
	int  col ;
	int  row ;
	u_char  buf[7] ;

	if( is_released)
	{
		button = 3 ;
	}
	else
	{
		button = event->button - Button1 ;
	}

	/*
	 * Shift = 4
	 * Meta = 8
	 * Control = 16
	 */
	key_state = ((event->state & ShiftMask) ? 4 : 0) +
		((event->state & ControlMask) ? 16 : 0) ;

	if( termscr->vertical_mode)
	{
		u_int  x_rest ;
		
		col = convert_y_to_row( termscr , NULL , event->y) ;

	#if  0
		if( termscr->font_man->use_multi_col_char)
		{
			/*
			 * XXX
			 * col can be inaccurate if full width characters are used.
			 */
		}
	#endif
	
		if( ( line = ml_term_model_get_line_in_screen( termscr->model , col)) == NULL)
		{
			return  0 ;
		}
		
		row = ml_convert_char_index_to_col( line ,
			convert_x_to_char_index( termscr , line , &x_rest , event->x) , 0) ;
			
		if( termscr->vertical_mode & VERT_RTL)
		{
			row = ml_term_model_get_cols( termscr->model) - row - 1 ;
		}
		
	#if  0
		if( termscr->font_man->use_multi_col_char)
		{
			/*
			 * XXX
			 * row can be inaccurate if full width characters are used.
			 */
		}
	#endif
	}
	else
	{
		row = convert_y_to_row( termscr , NULL , event->y) ;
		
		if( ( line = ml_term_model_get_line_in_screen( termscr->model , row)) == NULL)
		{
			return  0 ;
		}
		
		col = ml_convert_char_index_to_col( line ,
			convert_x_to_char_index( termscr , line , NULL , event->x) , 0) ;
	}

	strcpy( buf , "\x1b[M") ;

	buf[3] = 0x20 + button + key_state ;
	buf[4] = 0x20 + col + 1 ;
	buf[5] = 0x20 + row + 1 ;

	write_to_pty( termscr , buf , 6 , NULL) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [reported cursor pos] %d %d\n" , col , row) ;
#endif
	
	return  1 ;
}

static void
button_pressed(
	ml_window_t *  win ,
	XButtonEvent *  event ,
	int  click_num
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*)win ;
	
	restore_selected_region_color( termscr) ;

	if( termscr->is_mouse_pos_sending && ! (event->state & ShiftMask))
	{
		report_mouse_tracking( termscr , event , 0) ;

		return ;
	}

	if( event->button == 2)
	{
		yank_event_received( termscr , event->time) ;
	}
	else if( click_num == 2 && event->button == 1)
	{
		/* double clicked */
		
		selecting_word( termscr , event->x , event->y , event->time) ;
	}
	else if( click_num == 3 && event->button == 1)
	{
		/* triple click */

		selecting_line( termscr , event->y , event->time) ;
	}
	else if( event->button == 3 && (event->state & ControlMask))
	{
		config_menu( termscr , event->x , event->y) ;
	}
	else if ( event->button == 4) 
	{
		/* wheel mouse */
		
		enter_backscroll_mode(termscr) ;
		if( event->state & ShiftMask)
		{
			bs_scroll_downward(termscr) ;
		}
		else if( event->state & ControlMask)
		{
			bs_page_downward(termscr) ;
		} 
		else 
		{
			bs_half_page_downward(termscr) ;
		}
	}
	else if ( event->button == 5) 
	{
		/* wheel mouse */
		
		enter_backscroll_mode(termscr) ;
		if( event->state & ShiftMask)
		{
			bs_scroll_upward(termscr) ;
		}
		else if( event->state & ControlMask)
		{
			bs_page_upward(termscr) ;
		} 
		else 
		{
			bs_half_page_upward(termscr) ;
		}
	}
}

static void
button_released(
	ml_window_t *  win ,
	XButtonEvent *  event
	)
{
	ml_term_screen_t *  termscr ;

	termscr = (ml_term_screen_t*) win ;

	if( termscr->is_mouse_pos_sending && ! (event->state & ShiftMask))
	{
		report_mouse_tracking( termscr , event , 1) ;

		return ;
	}
	
	if( termscr->sel.is_selecting)
	{
		ml_stop_selecting( &termscr->sel) ;
	}
}


/*
 * !! Notice !!
 * this is closely related to ml_{image|log}_copy_region()
 */
static void
reverse_or_restore_color(
	ml_term_screen_t *  termscr ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row ,
	int (*func)( ml_image_line_t * , int)
	)
{
	int  char_index ;
	int  row ;
	ml_image_line_t *  line ;
	u_int  size_except_end_space ;

	if( ( line = ml_term_model_get_line_in_all( termscr->model , beg_row)) == NULL ||
		ml_imgline_is_empty( line))
	{
		return ;
	}

	size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

	row = beg_row ;
	if( beg_row == end_row)
	{
		for( char_index = beg_char_index ;
			char_index < K_MIN(end_char_index + 1,size_except_end_space) ; char_index ++)
		{
			(*func)( line , char_index) ;
		}
	}
	else if( beg_row < end_row)
	{
		for( char_index = beg_char_index ; char_index < size_except_end_space ; char_index ++)
		{
			(*func)( line , char_index) ;
		}

		for( row ++ ; row < end_row ; row ++)
		{
			if( ( line = ml_term_model_get_line_in_all( termscr->model , row)) == NULL ||
				ml_imgline_is_empty( line))
			{
				goto  end ;
			}

			size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;
			
			for( char_index = 0 ; char_index < size_except_end_space ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
		
		if( ( line = ml_term_model_get_line_in_all( termscr->model , row)) == NULL ||
			ml_imgline_is_empty( line))
		{
			goto  end ;
		}

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;
		
		for( char_index = 0 ;
			char_index < K_MIN(end_char_index + 1,size_except_end_space) ; char_index ++)
		{
			(*func)( line , char_index) ;
		}
	}

end:
	redraw_image( termscr) ;

	return  ;
}


/*
 * callbacks of ml_config_menu_event events.
 */

static void
font_size_changed(
	ml_term_screen_t *  termscr
	)
{
	if( HAS_SCROLL_LISTENER(termscr,line_height_changed))
	{
		(*termscr->screen_scroll_listener->line_height_changed)(
			termscr->screen_scroll_listener->self , ml_line_height( termscr->font_man)) ;
	}

	/* screen will redrawn in window_resized() */
	ml_window_resize( &termscr->window , screen_width( termscr) , screen_height( termscr) ,
		NOTIFY_TO_PARENT) ;

	ml_window_set_normal_hints( &termscr->window ,
		ml_col_width( termscr->font_man) , ml_line_height( termscr->font_man) ,
		ml_col_width( termscr->font_man) , ml_line_height( termscr->font_man)) ;
		
	ml_window_reset_font( &termscr->window) ;

	/*
	 * !! Notice !!
	 * ml_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( termscr->window.window_resized)
	{
		(*termscr->window.window_resized)( &termscr->window) ;
	}
}

static void
change_font_size(
	void *  p ,
	u_int  font_size
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( font_size == termscr->font_man->font_size)
	{
		/* not changed */
		
		return ;
	}
	
	if( ! ml_change_font_size( termscr->font_man , font_size))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_change_font_size(%d) failed.\n" , font_size) ;
	#endif
	
		return ;
	}
	
	/* redrawing all lines with new fonts. */
	ml_term_model_set_modified_all( termscr->model) ;

	font_size_changed( termscr) ;

	/* this is because font_man->font_set may have changed in ml_change_font_size() */
	ml_xic_font_set_changed( &termscr->window) ;
}

static void
change_line_space(
	void *  p ,
	u_int  line_space
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	termscr->font_man->line_space = line_space ;

	font_size_changed( termscr) ;
}

static void
change_screen_width_ratio(
	void *  p ,
	u_int  ratio
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->screen_width_ratio == ratio)
	{
		return ;
	}
	
	termscr->screen_width_ratio = ratio ;

	ml_window_resize( &termscr->window , screen_width( termscr) , screen_height( termscr) ,
		NOTIFY_TO_PARENT) ;

	/*
	 * !! Notice !!
	 * ml_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( termscr->window.window_resized)
	{
		(*termscr->window.window_resized)( &termscr->window) ;
	}
}

static void
change_screen_height_ratio(
	void *  p ,
	u_int  ratio
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->screen_height_ratio == ratio)
	{
		return ;
	}
	
	termscr->screen_height_ratio = ratio ;

	ml_window_resize( &termscr->window , screen_width( termscr) , screen_height( termscr) ,
		NOTIFY_TO_PARENT) ;

	/*
	 * !! Notice !!
	 * ml_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( termscr->window.window_resized)
	{
		(*termscr->window.window_resized)( &termscr->window) ;
	}
}

static void
change_char_encoding(
	void *  p ,
	ml_char_encoding_t  encoding
	)
{
	ml_term_screen_t *  termscr ;
	int  result ;
	
	termscr = p ;

	if( (*termscr->encoding_listener->encoding)( termscr->encoding_listener->self) == encoding)
	{
		/* not changed */
		
		return ;
	}

	if( encoding == ML_ISCII)
	{
		/*
		 * ISCII needs variable column width and character combining.
		 * (similar processing is done in ml_set_encoding_listener)
		 */

		if( ! (termscr->font_present & FONT_VAR_WIDTH))
		{
			(*termscr->config_menu_listener.change_font_present)( termscr ,
				termscr->font_present | FONT_VAR_WIDTH) ;
		}

		ml_use_char_combining() ;
	}
		
	if( ( result = ml_font_manager_usascii_font_cs_changed( termscr->font_man ,
		ml_get_usascii_font_cs( encoding))) == -1)
	{
		/* failed */

	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " encoding couldn't be changed to %x.\n" , encoding) ;
	#endif

		return ;
	}

	if( result)
	{
		font_size_changed( termscr) ;

		/*
		 * this is because font_man->font_set may have changed in
		 * ml_font_manager_usascii_font_cs_changed()
		 */
		ml_xic_font_set_changed( &termscr->window) ;
	}

	if( ! (*termscr->encoding_listener->encoding_changed)(
		termscr->encoding_listener->self , encoding))
	{
		kik_error_printf( "VT100 encoding and Terminal screen encoding are discrepant.\n") ;
	}
	
	update_encoding_proper_aux( termscr , 1) ;
	
	ml_term_model_set_modified_all( termscr->model) ;
}

static void
change_iscii_lang(
	void *  p ,
	ml_iscii_lang_t  lang
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->iscii_lang == lang)
	{
		/* not changed */
		
		return ;
	}

	termscr->iscii_lang = lang ;
	
	if( termscr->iscii_state)
	{
		ml_term_model_logical( termscr->model) ;
		select_iscii_lang( termscr) ;
		ml_term_model_visual( termscr->model) ;
		ml_term_model_set_modified_all( termscr->model) ;
	}
}

static void
change_tab_size(
	void *  p ,
	u_int  tab_size
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	ml_term_model_set_tab_size( termscr->model , tab_size) ;
}

static void
change_log_size(
	void *  p ,
	u_int  logsize
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( ml_term_model_get_log_size( termscr->model) == logsize)
	{
		/* not changed */
		
		return ;
	}

	/*
	 * this is necesary since ml_logs_t size is changed.
	 */
	restore_selected_region_color( termscr) ;
	exit_backscroll_mode( termscr) ;
	
	ml_term_model_change_log_size( termscr->model , logsize) ;

	if( HAS_SCROLL_LISTENER(termscr,log_size_changed))
	{
		(*termscr->screen_scroll_listener->log_size_changed)(
			termscr->screen_scroll_listener->self , logsize) ;
	}
}

static void
change_sb_view(
	void *  p ,
	char *  name
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( HAS_SCROLL_LISTENER(termscr,change_view))
	{
		(*termscr->screen_scroll_listener->change_view)(
			termscr->screen_scroll_listener->self , name) ;
	}
}

static void
change_mod_meta_mode(
	void *  p ,
	ml_mod_meta_mode_t  mod_meta_mode
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	termscr->mod_meta_mode = mod_meta_mode ;
}

static void
change_bel_mode(
	void *  p ,
	ml_bel_mode_t  bel_mode
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	termscr->bel_mode = bel_mode ;
}

static void
change_vertical_mode(
	void *  p ,
	ml_vertical_mode_t  vertical_mode
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->vertical_mode == vertical_mode)
	{
		/* not changed */
		
		return ;
	}

	/*
	 * vertical font is automatically used under vertical mode.
	 * similler processing is done in ml_term_screen_new.
	 */
	if( vertical_mode)
	{
		if( ! (termscr->font_present & FONT_VERTICAL))
		{
			(*termscr->config_menu_listener.change_font_present)( termscr ,
				(termscr->font_present | FONT_VERTICAL) & ~FONT_VAR_WIDTH) ;
		}
	}
	else
	{
		if( termscr->font_present & FONT_VERTICAL)
		{
			(*termscr->config_menu_listener.change_font_present)( termscr ,
				termscr->font_present & ~FONT_VERTICAL) ;
		}
	}

	termscr->vertical_mode = vertical_mode ;
	
	update_encoding_proper_aux( termscr , 1) ;
	
	/* redrawing under new vertical mode. */
	ml_term_model_set_modified_all( termscr->model) ;
	
	ml_window_resize( &termscr->window , screen_width(termscr) , screen_height(termscr) ,
		NOTIFY_TO_PARENT) ;

	/*
	 * !! Notice !!
	 * ml_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( termscr->window.window_resized)
	{
		(*termscr->window.window_resized)( &termscr->window) ;
	}
}

static void
change_sb_mode(
	void *  p ,
	ml_sb_mode_t  sb_mode
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	if( HAS_SCROLL_LISTENER(termscr,change_sb_mode))
	{
		(*termscr->screen_scroll_listener->change_sb_mode)(
			termscr->screen_scroll_listener->self , sb_mode) ;
	}
}

static void
change_char_combining_flag(
	void *  p ,
	int  is_combining_char
	)
{
	if( ! is_combining_char)
	{
		ml_unuse_char_combining() ;
	}
	else
	{
		ml_use_char_combining() ;
	}
}

static void
change_dynamic_comb_flag(
	void *  p ,
	int  use_dynamic_comb
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->use_dynamic_comb == use_dynamic_comb)
	{
		/* not changed */
		
		return ;
	}

	termscr->use_dynamic_comb = use_dynamic_comb ;

	ml_term_model_set_modified_all( termscr->model) ;

	update_encoding_proper_aux( termscr , 1) ;
}

static void
change_copy_paste_via_ucs_flag(
	void *  p ,
	int  flag
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	termscr->copy_paste_via_ucs = flag ;
}

static void
change_fg_color(
	void *  p ,
	char *  name
	)
{
	ml_term_screen_t *  termscr ;
	ml_color_t  color ;

	termscr = p ;

	if( ( color = ml_get_color( termscr->color_man , name)) == ML_UNKNOWN_COLOR)
	{
		return ;
	}

	if( ml_window_get_fg_color( &termscr->window) == color)
	{
		/* not changed */
		
		return ;
	}

	ml_window_unfade( &termscr->window) ;
	
	ml_window_set_fg_color( &termscr->window , color) ;
	ml_xic_fg_color_changed( &termscr->window) ;
}

static void
change_bg_color(
	void *  p ,
	char *  name
	)
{
	ml_term_screen_t *  termscr ;
	ml_color_t  color ;

	termscr = p ;

	if( ( color = ml_get_color( termscr->color_man , name)) == ML_UNKNOWN_COLOR)
	{
		return ;
	}

	if( ml_window_get_bg_color( &termscr->window) == color)
	{
		/* not changed */
		
		return ;
	}
	
	ml_window_unfade( &termscr->window) ;

	ml_window_set_bg_color( &termscr->window , color) ;
	ml_xic_bg_color_changed( &termscr->window) ;
}

static void
change_sb_fg_color(
	void *  p ,
	char *  name
	)
{
	ml_term_screen_t *  termscr ;
	ml_color_t  color ;

	termscr = p ;

	if( ( color = ml_get_color( termscr->color_man , name)) == ML_UNKNOWN_COLOR)
	{
		return ;
	}

	if( HAS_SCROLL_LISTENER(termscr,change_fg_color))
	{
		(*termscr->screen_scroll_listener->change_fg_color)(
			termscr->screen_scroll_listener->self , color) ;
	}
}

static void
change_sb_bg_color(
	void *  p ,
	char *  name
	)
{
	ml_term_screen_t *  termscr ;
	ml_color_t  color ;

	termscr = p ;

	if( ( color = ml_get_color( termscr->color_man , name)) == ML_UNKNOWN_COLOR)
	{
		return ;
	}
	
	if( HAS_SCROLL_LISTENER(termscr,change_bg_color))
	{
		(*termscr->screen_scroll_listener->change_bg_color)(
			termscr->screen_scroll_listener->self , color) ;
	}
}

static void
larger_font_size(
	void *  p
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	ml_larger_font( termscr->font_man) ;

	font_size_changed( termscr) ;

	/* this is because font_man->font_set may have changed in ml_larger_font() */
	ml_xic_font_set_changed( &termscr->window) ;

	/* redrawing all lines with new fonts. */
	ml_term_model_set_modified_all( termscr->model) ;
		
	return ;
}

static void
smaller_font_size(
	void *  p
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	ml_smaller_font( termscr->font_man) ;

	font_size_changed( termscr) ;
	
	/* this is because font_man->font_set may have changed in ml_smaller_font() */
	ml_xic_font_set_changed( &termscr->window) ;

	/* redrawing all lines with new fonts. */
	ml_term_model_set_modified_all( termscr->model) ;
	
	return ;
}

static void
change_transparent_flag(
	void *  p ,
	int  is_transparent
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->window.is_transparent == is_transparent)
	{
		/* not changed */
		
		return ;
	}

	if( is_transparent)
	{
		ml_window_set_transparent( &termscr->window , get_picture_modifier( termscr)) ;
	}
	else
	{
		ml_window_unset_transparent( &termscr->window) ;
	}
	
	if( HAS_SCROLL_LISTENER(termscr,transparent_state_changed))
	{
		(*termscr->screen_scroll_listener->transparent_state_changed)(
			termscr->screen_scroll_listener->self , is_transparent ,
			get_picture_modifier( termscr)) ;
	}
}

static void
change_font_present(
	void *  p ,
	ml_font_present_t  font_present
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	if( termscr->vertical_mode)
	{
		font_present &= ~FONT_VAR_WIDTH ;
	}

	if( termscr->font_present == font_present)
	{
		/* not changed */
		
		return ;
	}

	if( ! ml_font_manager_change_font_present( termscr->font_man , font_present))
	{
		return ;
	}

	font_size_changed( termscr) ;
	
	/* redrawing all lines with new fonts. */
	ml_term_model_set_modified_all( termscr->model) ;
	
	termscr->font_present = font_present ;
}

static void
change_multi_col_char_flag(
	void *  p ,
	int  flag
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	if( flag)
	{
		ml_use_multi_col_char( termscr->font_man) ;
	}
	else
	{
		ml_unuse_multi_col_char( termscr->font_man) ;
	}
}

static void
change_bidi_flag(
	void *  p ,
	int  use_bidi
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->use_bidi == use_bidi)
	{
		/* not changed */
		
		return ;
	}

	termscr->use_bidi = use_bidi ;
	
	ml_term_model_set_modified_all( termscr->model) ;

	update_encoding_proper_aux( termscr , 1) ;
}

static void
change_wall_picture(
	void *  p ,
	char *  file_path
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->pic_file_path)
	{
		if( strcmp( termscr->pic_file_path , file_path) == 0)
		{
			/* not changed */
			
			return ;
		}
		
		free( termscr->pic_file_path) ;
	}

	termscr->pic_file_path = strdup( file_path) ;

	set_wall_picture( termscr) ;
}

static void
change_brightness(
	void *  p ,
	u_int  brightness
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->pic_mod.brightness == brightness)
	{
		/* not changed */
		
		return ;
	}

	termscr->pic_mod.brightness = brightness ;
	
	if( termscr->window.is_transparent)
	{
		ml_window_set_transparent( &termscr->window , get_picture_modifier( termscr)) ;
		
		if( HAS_SCROLL_LISTENER(termscr,transparent_state_changed))
		{
			(*termscr->screen_scroll_listener->transparent_state_changed)(
				termscr->screen_scroll_listener->self , 1 , get_picture_modifier( termscr)) ;
		}
	}
	else
	{
		set_wall_picture( termscr) ;
	}
}
	
static void
change_fade_ratio(
	void *  p ,
	u_int  fade_ratio
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->fade_ratio == fade_ratio)
	{
		/* not changed */
		
		return ;
	}

	termscr->fade_ratio = fade_ratio ;

	if( ! termscr->is_focused)
	{
		unhighlight_cursor( termscr) ;

		if( termscr->fade_ratio >= 100)
		{
			ml_window_unfade( &termscr->window) ;
		}
		else
		{
			/* suppressing redrawing */
			termscr->window.window_exposed = NULL ;
			ml_window_unfade( &termscr->window) ;

			termscr->window.window_exposed = window_exposed ;
			ml_window_fade( &termscr->window , termscr->fade_ratio) ;
		}
		
		highlight_cursor( termscr) ;
	}
}

static void
change_xim(
	void *  p ,
	char *  xim ,
	char *  locale
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	ml_xic_deactivate( &termscr->window) ;

	ml_xic_activate( &termscr->window , xim , locale) ;
}

static void
full_reset(
	void *  p
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	(*termscr->encoding_listener->init)( termscr->encoding_listener->self , 1) ;
}


/*
 * callbacks of ml_sel_event_listener_t events.
 */
 
static void
reverse_color(
	void *  p ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	reverse_or_restore_color( (ml_term_screen_t*)p , beg_char_index , beg_row ,
		end_char_index , end_row , ml_imgline_reverse_color) ;
}

static void
restore_color(
	void *  p ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;
	
	reverse_or_restore_color( (ml_term_screen_t*)p , beg_char_index , beg_row ,
		end_char_index , end_row , ml_imgline_restore_color) ;
}

static int
select_in_window(
	void *  p ,
	ml_char_t **  chars ,
	u_int *  len ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	ml_term_screen_t *  termscr ;
	u_int  size ;
	ml_char_t *  buf ;

	termscr = p ;

	if( ( size = ml_term_model_get_region_size( termscr->model , beg_char_index , beg_row ,
			end_char_index , end_row)) == 0)
	{
		return  0 ;
	}

	if( ( buf = ml_str_alloca( size)) == NULL)
	{
		return  0 ;
	}

	*len = ml_term_model_copy_region( termscr->model , buf , size , beg_char_index ,
		beg_row , end_char_index , end_row) ;

#ifdef  DEBUG
	if( size != *len)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" ml_term_model_get_region_size() == %d and ml_term_model_copy_region() == %d"
			" are not the same size !\n" ,
			size , *len) ;
	}
#endif

	if( (*chars = ml_str_new( size)) == NULL)
	{
		return  0 ;
	}

	ml_str_copy( *chars , buf , size) ;

	ml_str_final( buf , size) ;
	
	return  1 ;
}


/*
 * callbacks of ml_term_model_event_listener_t events.
 */
 
static int
window_scroll_upward_region(
	void *  p ,
	int  beg_row ,
	int  end_row ,
	u_int  size
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->vertical_mode || ! ml_window_is_scrollable( &termscr->window))
	{
		return  0 ;
	}

	set_scroll_boundary( termscr , beg_row , end_row) ;
	
	termscr->scroll_cache_rows += size ;
	
	return  1 ;
}

static int
window_scroll_downward_region(
	void *  p ,
	int  beg_row ,
	int  end_row ,
	u_int  size
	)
{
	ml_term_screen_t *  termscr ;

	termscr = p ;

	if( termscr->vertical_mode || ! ml_window_is_scrollable( &termscr->window))
	{
		return  0 ;
	}

	set_scroll_boundary( termscr , beg_row , end_row) ;
	
	termscr->scroll_cache_rows -= size ;

	return  1 ;
}

/*
 * callbacks of ml_xim events.
 */
 
/*
 * this doesn't consider backscroll mode.
 */
static int
get_spot(
	void *  p ,
	int *  x ,
	int *  y
	)
{
	ml_term_screen_t *  termscr ;
	ml_image_line_t *  line ;

	termscr = p ;
	
	if( ( line = ml_term_model_cursor_line( termscr->model)) == NULL ||
		ml_imgline_is_empty( line))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " cursor line doesn't exist ?.\n") ;
	#endif
	
		return  0 ;
	}
	
	*y = convert_row_to_y( termscr , ml_term_model_cursor_row( termscr->model)) +
		ml_line_height( termscr->font_man) ;
	
	*x = convert_char_index_to_x( termscr , line , ml_term_model_cursor_char_index( termscr->model)) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " xim spot => x %d y %d\n" , *x , *y) ;
#endif

	return  1 ;
}

static XFontSet
get_fontset(
	void *  p
	)
{
	ml_term_screen_t *  termscr ;
	
	termscr = p ;

	return  ml_get_fontset( termscr->font_man) ;
}


/* --- global functions --- */

ml_term_screen_t *
ml_term_screen_new(
	u_int  cols ,
	u_int  rows ,
	ml_font_manager_t *  font_man ,
	ml_color_manager_t *  color_man ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	u_int  brightness ,
	u_int  fade_ratio ,
	ml_keymap_t *  keymap ,
	ml_termcap_t *  termcap ,
	u_int  num_of_log_lines ,
	u_int  tab_size ,
	u_int  screen_width_ratio ,
	u_int  screen_height_ratio ,
	int  xim_open_in_startup ,
	ml_mod_meta_mode_t  mod_meta_mode ,
	ml_bel_mode_t  bel_mode ,
	int  copy_paste_via_ucs ,
	char *  pic_file_path ,
	int  use_transbg ,
	ml_font_present_t  font_present ,
	int  use_bidi ,
	ml_vertical_mode_t  vertical_mode ,
	int  use_vertical_cursor ,
	int  big5_buggy ,
	char *  conf_menu_path ,
	ml_iscii_lang_t  iscii_lang ,
	int  use_extended_scroll_shortcut ,
	int  use_dynamic_comb
	)
{
	ml_term_screen_t *  termscr ;
	ml_char_t  sp_ch ;
	ml_char_t  nl_ch ;

	if( ( termscr = malloc( sizeof( ml_term_screen_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
	
		return  NULL ;
	}

	/* allocated dynamically */
	termscr->model = NULL ;
	termscr->utf8_parser = NULL ;
	termscr->xct_parser = NULL ;
	termscr->ml_str_parser = NULL ;
	termscr->utf8_conv = NULL ;
	termscr->xct_conv = NULL ;
	
	termscr->pty = NULL ;
	
	termscr->iscii_lang = iscii_lang ;
	termscr->iscii_state = NULL ;
	termscr->shape = NULL ;

	termscr->font_man = font_man ;
	termscr->color_man = color_man ;

	if( ( termscr->vertical_mode = vertical_mode))
	{
		/*
		 * vertical font is automatically used under vertical mode.
		 * similler processing is done in change_vertical_mode.
		 */
		font_present |= FONT_VERTICAL ;
		font_present &= ~FONT_VAR_WIDTH ;
	}

	termscr->use_vertical_cursor = use_vertical_cursor ;
	
	if( font_present)
	{
		if( ! ml_font_manager_change_font_present( termscr->font_man , font_present))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " ml_font_manager_change_font_present failed.\n") ;
		#endif
		
			font_present = 0 ;
		}
	}
	
	termscr->font_present = font_present ;

	termscr->use_bidi = use_bidi ;
	
	ml_char_init( &sp_ch) ;
	ml_char_init( &nl_ch) ;
	
	ml_char_set( &sp_ch , " " , 1 , ml_get_usascii_font( termscr->font_man) ,
		0 , ML_FG_COLOR , ML_BG_COLOR , 0) ;
	ml_char_set( &nl_ch , "\n" , 1 , ml_get_usascii_font( termscr->font_man) ,
		0 , ML_FG_COLOR , ML_BG_COLOR , 0) ;

	termscr->sel_listener.self = termscr ;
	termscr->sel_listener.select_in_window = select_in_window ;
	termscr->sel_listener.reverse_color = reverse_color ;
	termscr->sel_listener.restore_color = restore_color ;

	if( ! ml_sel_init( &termscr->sel , &termscr->sel_listener))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_sel_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	termscr->termmdl_listener.self = termscr ;
	termscr->termmdl_listener.window_scroll_upward_region = window_scroll_upward_region ;
	termscr->termmdl_listener.window_scroll_downward_region = window_scroll_downward_region ;
	termscr->termmdl_listener.scrolled_out_line_received = NULL ;

	if( ! ( termscr->model = ml_term_model_new( &termscr->termmdl_listener ,
					cols , rows , &sp_ch , &nl_ch , tab_size , num_of_log_lines)))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_term_model_new failed.\n") ;
	#endif
	
		goto  error ;
	}

	ml_char_final( &sp_ch) ;
	ml_char_final( &nl_ch) ;

	termscr->pic_mod.brightness = brightness ;
	
	termscr->fade_ratio = fade_ratio ;
	termscr->is_focused = 0 ;

	termscr->screen_width_ratio = screen_width_ratio ;
	termscr->screen_height_ratio = screen_height_ratio ;
	
	if( ml_window_init( &termscr->window ,
		ml_color_table_new( termscr->color_man , fg_color , bg_color) ,
		screen_width(termscr) , screen_height(termscr) ,
		ml_col_width( termscr->font_man) , ml_line_height( termscr->font_man) ,
		ml_col_width( termscr->font_man) , ml_line_height( termscr->font_man) , 2) == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_window_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	termscr->xim_listener.self = termscr ;
	termscr->xim_listener.get_spot = get_spot ;
	termscr->xim_listener.get_fontset = get_fontset ;
	termscr->window.xim_listener = &termscr->xim_listener ;

	termscr->xim_open_in_startup = xim_open_in_startup ;

	ml_window_set_cursor( &termscr->window , XC_xterm) ;

	/*
	 * event call backs.
	 */

	ml_window_init_event_mask( &termscr->window ,
		ButtonPressMask | ButtonMotionMask | ButtonReleaseMask | KeyPressMask) ;

	termscr->window.window_realized = window_realized ;
	termscr->window.window_finalized = window_finalized ;
	termscr->window.window_exposed = window_exposed ;
	termscr->window.window_focused = window_focused ;
	termscr->window.window_unfocused = window_unfocused ;
	termscr->window.key_pressed = key_pressed ;
	termscr->window.window_resized = window_resized ;
	termscr->window.button_motion = button_motion ;
	termscr->window.button_released = button_released ;
	termscr->window.button_pressed = button_pressed ;
	termscr->window.button_press_continued = button_press_continued ;
	termscr->window.selection_cleared = selection_cleared ;
	termscr->window.xct_selection_requested = xct_selection_requested ;
	termscr->window.utf8_selection_requested = utf8_selection_requested ;
	termscr->window.xct_selection_notified = xct_selection_notified ;
	termscr->window.utf8_selection_notified = utf8_selection_notified ;
	termscr->window.xct_selection_request_failed = NULL ;
	termscr->window.utf8_selection_request_failed = utf8_selection_request_failed ;
	termscr->window.window_deleted = window_deleted ;

	if( use_transbg)
	{
		ml_window_set_transparent( &termscr->window , get_picture_modifier( termscr)) ;
	}
	
	if( pic_file_path)
	{
		termscr->pic_file_path = strdup( pic_file_path) ;
	}
	else
	{
		termscr->pic_file_path = NULL ;
	}

	termscr->config_menu_listener.self = termscr ;
	termscr->config_menu_listener.change_char_encoding = change_char_encoding ;
	termscr->config_menu_listener.change_iscii_lang = change_iscii_lang ;
	termscr->config_menu_listener.change_fg_color = change_fg_color ;
	termscr->config_menu_listener.change_bg_color = change_bg_color ;
	termscr->config_menu_listener.change_sb_fg_color = change_sb_fg_color ;
	termscr->config_menu_listener.change_sb_bg_color = change_sb_bg_color ;
	termscr->config_menu_listener.change_tab_size = change_tab_size ;
	termscr->config_menu_listener.change_log_size = change_log_size ;
	termscr->config_menu_listener.change_font_size = change_font_size ;
	termscr->config_menu_listener.change_line_space = change_line_space ;
	termscr->config_menu_listener.change_screen_width_ratio = change_screen_width_ratio ;
	termscr->config_menu_listener.change_screen_height_ratio = change_screen_height_ratio ;
	termscr->config_menu_listener.change_mod_meta_mode = change_mod_meta_mode ;
	termscr->config_menu_listener.change_bel_mode = change_bel_mode ;
	termscr->config_menu_listener.change_vertical_mode = change_vertical_mode ;
	termscr->config_menu_listener.change_sb_mode = change_sb_mode ;
	termscr->config_menu_listener.change_char_combining_flag = change_char_combining_flag ;
	termscr->config_menu_listener.change_dynamic_comb_flag = change_dynamic_comb_flag ;
	termscr->config_menu_listener.change_copy_paste_via_ucs_flag = change_copy_paste_via_ucs_flag ;
	termscr->config_menu_listener.change_transparent_flag = change_transparent_flag ;
	termscr->config_menu_listener.change_brightness = change_brightness ;
	termscr->config_menu_listener.change_fade_ratio = change_fade_ratio ;
	termscr->config_menu_listener.change_font_present = change_font_present ;
	termscr->config_menu_listener.change_multi_col_char_flag = change_multi_col_char_flag ;
	termscr->config_menu_listener.change_bidi_flag = change_bidi_flag ;
	termscr->config_menu_listener.change_sb_view = change_sb_view ;
	termscr->config_menu_listener.change_xim = change_xim ;
	termscr->config_menu_listener.larger_font_size = larger_font_size ;
	termscr->config_menu_listener.smaller_font_size = smaller_font_size ;
	termscr->config_menu_listener.change_wall_picture = change_wall_picture ;
	termscr->config_menu_listener.full_reset = full_reset ;

	if( ! ml_config_menu_init( &termscr->config_menu , conf_menu_path ,
		&termscr->config_menu_listener))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_config_menu_init failed.\n") ;
	#endif
	
		goto  error ;
	}

	termscr->keymap = keymap ;
	termscr->termcap = termcap ;
	
	termscr->mod_meta_mask = 0 ;
	termscr->mod_meta_mode = mod_meta_mode ;
	termscr->bel_mode = bel_mode ;
	
	termscr->use_extended_scroll_shortcut = use_extended_scroll_shortcut ;
	termscr->use_dynamic_comb = use_dynamic_comb ;

	/*
	 * for receiving selection.
	 */

	if( ( termscr->utf8_parser = mkf_utf8_parser_new()) == NULL)
	{
		goto  error ;
	}
	
	if( ( termscr->xct_parser = mkf_xct_parser_new()) == NULL)
	{
		goto  error ;
	}

	/*
	 * for sending selection
	 */
	 
	if( ( termscr->ml_str_parser = ml_str_parser_new()) == NULL)
	{
		goto  error ;
	}

	if( ( termscr->utf8_conv = mkf_utf8_conv_new()) == NULL)
	{
		goto  error ;
	}

	if( big5_buggy)
	{
		if( ( termscr->xct_conv = mkf_xct_big5_buggy_conv_new()) == NULL)
		{
			goto  error ;
		}
	}
	else if( ( termscr->xct_conv = mkf_xct_conv_new()) == NULL)
	{
		goto  error ;
	}

	termscr->copy_paste_via_ucs = copy_paste_via_ucs ;

	termscr->encoding_listener = NULL ;
	termscr->system_listener = NULL ;
	termscr->screen_scroll_listener = NULL ;

	termscr->scroll_cache_rows = 0 ;
	termscr->scroll_cache_boundary_start = 0 ;
	termscr->scroll_cache_boundary_end = 0 ;

	termscr->is_reverse = 0 ;
	termscr->is_cursor_visible = 1 ;
	termscr->is_app_keypad = 0 ;
	termscr->is_app_cursor_keys = 0 ;
	termscr->is_mouse_pos_sending = 0 ;

	return  termscr ;

error:
	if( termscr->model)
	{
		ml_term_model_delete( termscr->model) ;
	}
	
	if( termscr->utf8_parser)
	{
		(*termscr->utf8_parser->delete)( termscr->utf8_parser) ;
	}
	
	if( termscr->xct_parser)
	{
		(*termscr->xct_parser->delete)( termscr->xct_parser) ;
	}
	
	if( termscr->ml_str_parser)
	{
		(*termscr->ml_str_parser->delete)( termscr->ml_str_parser) ;
	}
	
	if( termscr->utf8_conv)
	{
		(*termscr->utf8_conv->delete)( termscr->utf8_conv) ;
	}
	
	if( termscr->xct_conv)
	{
		(*termscr->xct_conv->delete)( termscr->xct_conv) ;
	}
	
	if( termscr)
	{
		free( termscr) ;
	}

	return  NULL ;
}

int
ml_term_screen_delete(
	ml_term_screen_t *  termscr
	)
{
	ml_term_model_delete( termscr->model) ;
	ml_sel_final( &termscr->sel) ;
	ml_config_menu_final( &termscr->config_menu) ;

	if( termscr->shape)
	{
		(*termscr->shape->delete)( termscr->shape) ;
	}

	if( termscr->iscii_state)
	{
		ml_iscii_delete( termscr->iscii_state) ;
	}

	if( termscr->pic_file_path)
	{
		free( termscr->pic_file_path) ;
	}
	
	if( termscr->utf8_parser)
	{
		(*termscr->utf8_parser->delete)( termscr->utf8_parser) ;
	}
	
	if( termscr->xct_parser)
	{
		(*termscr->xct_parser->delete)( termscr->xct_parser) ;
	}
	
	if( termscr->ml_str_parser)
	{
		(*termscr->ml_str_parser->delete)( termscr->ml_str_parser) ;
	}
	
	if( termscr->utf8_conv)
	{
		(*termscr->utf8_conv->delete)( termscr->utf8_conv) ;
	}
	
	if( termscr->xct_conv)
	{
		(*termscr->xct_conv->delete)( termscr->xct_conv) ;
	}
	
	free( termscr) ;

	return  1 ;
}

int
ml_term_screen_set_pty(
	ml_term_screen_t *  termscr ,
	ml_pty_t *  pty
	)
{
	if( termscr->pty)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " pty is already set.\n") ;
	#endif
	
		return  0 ;
	}

	termscr->pty = pty ;

	return  1 ;
}

int
ml_set_system_listener(
	ml_term_screen_t *  termscr ,
	ml_system_event_listener_t *  system_listener
	)
{
	if( termscr->system_listener)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " system listener is already set.\n") ;
	#endif
	
		return  0 ;
	}

	termscr->system_listener = system_listener ;

	return  1 ;
}

int
ml_set_encoding_listener(
	ml_term_screen_t *  termscr ,
	ml_pty_encoding_event_listener_t *  encoding_listener
	)
{
	if( termscr->encoding_listener)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " encoding listener is already set.\n") ;
	#endif

		return  0 ;
	}

	/*
	 * all members must be set.
	 */
	if( ! encoding_listener->encoding_changed ||
		! encoding_listener->encoding ||
		! encoding_listener->convert ||
		! encoding_listener->init)
	{
		return  0 ;
	}

	termscr->encoding_listener = encoding_listener ;
	
	update_encoding_proper_aux( termscr , 0) ;

	if( (*termscr->encoding_listener->encoding)( termscr->encoding_listener->self) == ML_ISCII)
	{
		/*
		 * ISCII needs variable column width and character combining.
		 * (similar processing is done in change_char_encoding)
		 */

		if( ! (termscr->font_present & FONT_VAR_WIDTH))
		{
			if( ml_font_manager_change_font_present( termscr->font_man ,
				termscr->font_present | FONT_VAR_WIDTH))
			{
				termscr->font_present |= FONT_VAR_WIDTH ;
			}
		}

		ml_use_char_combining() ;
	}
	
	return  1 ;
}

int
ml_set_screen_scroll_listener(
	ml_term_screen_t *  termscr ,
	ml_screen_scroll_event_listener_t *  screen_scroll_listener
	)
{
	if( termscr->screen_scroll_listener)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " screen scroll listener is already set.\n") ;
	#endif
	
		return  0 ;
	}

	termscr->screen_scroll_listener = screen_scroll_listener ;

	return  1 ;
}


/*
 * for scrollbar scroll.
 *
 * Similar processing is done in bs_xxx().
 */
 
int
ml_term_screen_scroll_upward(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	unhighlight_cursor( termscr) ;
	
	if( ! ml_is_backscroll_mode( termscr->model))
	{
		enter_backscroll_mode( termscr) ;
	}

	ml_term_model_backscroll_upward( termscr->model , size) ;
	
	redraw_image( termscr) ;
	highlight_cursor( termscr) ;
	
	return  1 ;
}

int
ml_term_screen_scroll_downward(
	ml_term_screen_t *  termscr ,
	u_int  size
	)
{
	unhighlight_cursor( termscr) ;
	
	if( ! ml_is_backscroll_mode( termscr->model))
	{
		enter_backscroll_mode( termscr) ;
	}

	ml_term_model_backscroll_downward( termscr->model , size) ;

	redraw_image( termscr) ;
	highlight_cursor( termscr) ;

	return  1 ;
}

int
ml_term_screen_scroll_to(
	ml_term_screen_t *  termscr ,
	int  row
	)
{
	unhighlight_cursor( termscr) ;

	if( ! ml_is_backscroll_mode( termscr->model))
	{
		enter_backscroll_mode( termscr) ;
	}
	
	ml_term_model_backscroll_to( termscr->model , row) ;

	redraw_image( termscr) ;
	highlight_cursor( termscr) ;

	return  1 ;
}


ml_picture_modifier_t *
ml_term_screen_get_picture_modifier(
	ml_term_screen_t *  termscr
	)
{
	return  get_picture_modifier( termscr) ;
}


/*
 * these start/stop functions ensure that VT100 commands should treat screen buffer
 * as logical order , not visual order.
 */
 
int
ml_term_screen_start_vt100_cmd(
	ml_term_screen_t *  termscr
	)
{
	restore_selected_region_color( termscr) ;
	exit_backscroll_mode( termscr) ;

	unhighlight_cursor( termscr) ;

	ml_term_model_logical( termscr->model) ;

	return  1 ;
}

int
ml_term_screen_stop_vt100_cmd(
	ml_term_screen_t *  termscr
	)
{
	ml_term_model_render( termscr->model) ;
	ml_term_model_visual( termscr->model) ;
	
	redraw_image( termscr) ;
	highlight_cursor( termscr) ;
	
	return  1 ;
}


/*
 * VT100 commands
 *
 * !! Notice !!
 * These functions are called under logical order mode.
 */
 
ml_font_t *
ml_term_screen_get_font(
	ml_term_screen_t *  termscr ,
	ml_font_attr_t  attr
	)
{
	ml_font_t *  font ;
	
	if( ( font = ml_get_font( termscr->font_man , attr)) == NULL)
	{
		return  NULL ;
	}

#ifdef  DEBUG
	if( ml_line_height( termscr->font_man) < font->height)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" font(cs %x) height %d is larger than the basic line height %d.\n" ,
			ml_font_cs(font) , font->height , ml_line_height( termscr->font_man)) ;
	}
	else if( ml_line_height( termscr->font_man) > font->height)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" font(cs %x) height %d is smaller than the basic line height %d.\n" ,
			ml_font_cs(font) , font->height , ml_line_height( termscr->font_man)) ;
	}
#endif

	return  font ;
}

ml_color_t
ml_term_screen_get_color(
	ml_term_screen_t *  termscr ,
	char *  name
	)
{
	return  ml_get_color( termscr->color_man , name) ;
}

int
ml_term_screen_bel(
	ml_term_screen_t *  termscr
	)
{	
	if( termscr->bel_mode == BEL_SOUND)
	{
		XBell( termscr->window.display , 0) ;
	}
	else if( termscr->bel_mode == BEL_VISUAL)
	{
		ml_window_fill_all( &termscr->window) ;

		XFlush( termscr->window.display) ;

		ml_window_clear_all( &termscr->window) ;
		ml_term_model_set_modified_all( termscr->model) ;
	}

	return  1 ;
}

int
ml_term_screen_resize_columns(
	ml_term_screen_t *  termscr ,
	u_int  cols
	)
{
	if( cols == ml_term_model_get_logical_cols( termscr->model))
	{
		return  0 ;
	}

	/*
	 * ml_term_screen_{start|stop}_term_screen are necessary since
	 * window is redrawn in window_resized().
	 */
	 
	ml_term_screen_stop_vt100_cmd( termscr) ;
	
	ml_window_resize( &termscr->window , ml_col_width((termscr)->font_man) * cols ,
		ml_line_height((termscr)->font_man) * ml_term_model_get_rows( termscr->model) ,
		NOTIFY_TO_PARENT) ;

	/*
	 * !! Notice !!
	 * ml_window_resize() will invoke ConfigureNotify event but window_resized() won't be
	 * called , since xconfigure.width , xconfigure.height are the same as the already
	 * resized window.
	 */
	if( termscr->window.window_resized)
	{
		(*termscr->window.window_resized)( &termscr->window) ;
	}

	ml_term_screen_start_vt100_cmd( termscr) ;
	
	return  1 ;
}

int
ml_term_screen_reverse_video(
	ml_term_screen_t *  termscr
	)
{
	if( termscr->is_reverse == 1)
	{
		return  0 ;
	}

	ml_window_reverse_video( &termscr->window) ;
	
	ml_term_model_set_modified_all( termscr->model) ;

	termscr->is_reverse = 1 ;

	return  1 ;
}

int
ml_term_screen_restore_video(
	ml_term_screen_t *  termscr
	)
{
	if( termscr->is_reverse == 0)
	{
		return  0 ;
	}
	
	ml_window_reverse_video( &termscr->window) ;
	
	ml_term_model_set_modified_all( termscr->model) ;
	
	termscr->is_reverse = 0 ;

	return  0 ;
}

int
ml_term_screen_set_app_keypad(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_app_keypad = 1 ;

	return  1 ;
}

int
ml_term_screen_set_normal_keypad(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_app_keypad = 0 ;

	return  1 ;
}

int
ml_term_screen_set_app_cursor_keys(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_app_cursor_keys = 1 ;

	return  1 ;
}

int
ml_term_screen_set_normal_cursor_keys(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_app_cursor_keys = 0 ;

	return  1 ;
}

int
ml_term_screen_cursor_visible(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_cursor_visible = 1 ;

	return  1 ;
}

int
ml_term_screen_cursor_invisible(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_cursor_visible = 0 ;

	return  1 ;
}

int
ml_term_screen_set_mouse_pos_sending(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_mouse_pos_sending = 1 ;

	return  1 ;
}

int
ml_term_screen_unset_mouse_pos_sending(
	ml_term_screen_t *  termscr
	)
{
	termscr->is_mouse_pos_sending = 0 ;

	return  1 ;
}

int
ml_term_screen_send_device_attr(
	ml_term_screen_t *  termscr
	)
{
	/* vt100 answerback */
	char  seq[] = "\x1b[?1;2c" ;

	ml_write_to_pty( termscr->pty , seq , strlen( seq)) ;

	return  1 ;
}

int
ml_term_screen_report_device_status(
	ml_term_screen_t *  termscr
	)
{
	ml_write_to_pty( termscr->pty , "\x1b[0n" , 4) ;

	return  1 ;
}

int
ml_term_screen_report_cursor_position(
	ml_term_screen_t *  termscr
	)
{
	char  seq[4 + DIGIT_STR_LEN(u_int) + 1] ;

	sprintf( seq , "\x1b[%d;%dR" ,
		ml_term_model_cursor_row( termscr->model) + 1 ,
		ml_term_model_cursor_col( termscr->model) + 1) ;

	ml_write_to_pty( termscr->pty , seq , strlen( seq)) ;

	return  1 ;
}

int
ml_term_screen_set_window_name(
	ml_term_screen_t *  termscr ,
	u_char *  name
	)
{
	return  ml_set_window_name( &termscr->window , name) ;
}

int
ml_term_screen_set_icon_name(
	ml_term_screen_t *  termscr ,
	u_char *  name
	)
{
	return  ml_set_icon_name( &termscr->window , name) ;
}

int
ml_term_screen_fill_all_with_e(
	ml_term_screen_t *  termscr
	)
{
	ml_char_t  e_ch ;

	ml_char_init( &e_ch) ;
	ml_char_set( &e_ch , "E" , 1 , ml_get_usascii_font( termscr->font_man) ,
		0 , ML_FG_COLOR , ML_BG_COLOR , 0) ;

	ml_term_model_fill_all( termscr->model , &e_ch) ;

	ml_char_final( &e_ch) ;

	return  1 ;
}

int
ml_term_screen_set_config(
	ml_term_screen_t *  termscr ,
	char *  key ,
	char *  value
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s=%s\n" , key , value) ;
#endif

	/*
	 * ml_term_screen_{start|stop}_term_screen are necessary since
	 * window is redrawn in chagne_wall_picture().
	 */

	ml_term_screen_stop_vt100_cmd( termscr) ;

	if( strcmp( key , "encoding") == 0)
	{
		ml_char_encoding_t  encoding ;

		if( ( encoding = ml_get_char_encoding( value)) == ML_UNKNOWN_ENCODING)
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_char_encoding)
		{
			(*termscr->config_menu_listener.change_char_encoding)( termscr , encoding) ;
		}
	}
	else if( strcmp( key , "iscii_lang") == 0)
	{
		ml_iscii_lang_t  lang ;

		if( ( lang = ml_iscii_get_lang( value)) == ISCIILANG_UNKNOWN)
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_iscii_lang)
		{
			(*termscr->config_menu_listener.change_iscii_lang)( termscr , lang) ;
		}
	}
	else if( strcmp( key , "fg_color") == 0)
	{
		if( termscr->config_menu_listener.change_fg_color)
		{
			(*termscr->config_menu_listener.change_fg_color)( termscr , value) ;
		}
	}
	else if( strcmp( key , "bg_color") == 0)
	{
		if( termscr->config_menu_listener.change_bg_color)
		{
			(*termscr->config_menu_listener.change_bg_color)( termscr , value) ;
		}
	}
	else if( strcmp( key , "sb_fg_color") == 0)
	{
		if( termscr->config_menu_listener.change_sb_fg_color)
		{
			(*termscr->config_menu_listener.change_sb_fg_color)( termscr , value) ;
		}
	}
	else if( strcmp( key , "sb_bg_color") == 0)
	{
		if( termscr->config_menu_listener.change_sb_bg_color)
		{
			(*termscr->config_menu_listener.change_sb_bg_color)( termscr , value) ;
		}
	}
	else if( strcmp( key , "tabsize") == 0)
	{
		u_int  tab_size ;

		if( ! kik_str_to_uint( &tab_size , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_tab_size)
		{
			(*termscr->config_menu_listener.change_tab_size)( termscr , tab_size) ;
		}
	}
	else if( strcmp( key , "logsize") == 0)
	{
		u_int  log_size ;

		if( ! kik_str_to_uint( &log_size , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_log_size)
		{
			(*termscr->config_menu_listener.change_log_size)( termscr , log_size) ;
		}
	}
	else if( strcmp( key , "fontsize") == 0)
	{
		u_int  font_size ;

		if( strcmp( value , "larger") == 0)
		{
			if( termscr->config_menu_listener.larger_font_size)
			{
				(*termscr->config_menu_listener.larger_font_size)( termscr) ;
			}
		}
		else if( strcmp( value , "smaller") == 0)
		{
			if( termscr->config_menu_listener.smaller_font_size)
			{
				(*termscr->config_menu_listener.smaller_font_size)( termscr) ;
			}
		}
		else
		{
			if( ! kik_str_to_uint( &font_size , value))
			{
				return  0 ;
			}

			if( termscr->config_menu_listener.change_font_size)
			{
				(*termscr->config_menu_listener.change_font_size)( termscr , font_size) ;
			}
		}
	}
	else if( strcmp( key , "line_space") == 0)
	{
		u_int  line_space ;

		if( ! kik_str_to_uint( &line_space , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_line_space)
		{
			(*termscr->config_menu_listener.change_line_space)( termscr , line_space) ;
		}
	}
	else if( strcmp( key , "screen_width_ratio") == 0)
	{
		u_int  ratio ;

		if( ! kik_str_to_uint( &ratio , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_screen_width_ratio)
		{
			(*termscr->config_menu_listener.change_screen_width_ratio)( termscr , ratio) ;
		}
	}
	else if( strcmp( key , "screen_height_ratio") == 0)
	{
		u_int  ratio ;

		if( ! kik_str_to_uint( &ratio , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_screen_height_ratio)
		{
			(*termscr->config_menu_listener.change_screen_height_ratio)( termscr , ratio) ;
		}
	}
	else if( strcmp( key , "scrollbar_view_name") == 0)
	{
		if( termscr->config_menu_listener.change_sb_view)
		{
			(*termscr->config_menu_listener.change_sb_view)(
				termscr , value) ;
		}
	}
	else if( strcmp( key , "mod_meta_mode") == 0)
	{
		if( termscr->config_menu_listener.change_mod_meta_mode)
		{
			(*termscr->config_menu_listener.change_mod_meta_mode)(
				termscr , ml_get_mod_meta_mode( value)) ;
		}
	}
	else if( strcmp( key , "bel_mode") == 0)
	{
		if( termscr->config_menu_listener.change_bel_mode)
		{
			(*termscr->config_menu_listener.change_bel_mode)(
				termscr , ml_get_bel_mode( value)) ;
		}
	}
	else if( strcmp( key , "vertical_mode") == 0)
	{
		if( termscr->config_menu_listener.change_vertical_mode)
		{
			(*termscr->config_menu_listener.change_vertical_mode)(
				termscr , ml_get_vertical_mode( value)) ;
		}
	}
	else if( strcmp( key , "scrollbar_mode") == 0)
	{
		if( termscr->config_menu_listener.change_sb_mode)
		{
			(*termscr->config_menu_listener.change_sb_mode)(
				termscr , ml_get_sb_mode( value)) ;
		}
	}
	else if( strcmp( key , "use_combining") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_char_combining_flag)
		{
			(*termscr->config_menu_listener.change_char_combining_flag)( termscr , flag) ;
		}
	}
	else if( strcmp( key , "use_dynamic_comb") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_dynamic_comb_flag)
		{
			(*termscr->config_menu_listener.change_dynamic_comb_flag)( termscr , flag) ;
		}
	}
	else if( strcmp( key , "copy_paste_via_ucs") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_copy_paste_via_ucs_flag)
		{
			(*termscr->config_menu_listener.change_copy_paste_via_ucs_flag)( termscr , flag) ;
		}
	}
	else if( strcmp( key , "use_transbg") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_transparent_flag)
		{
			(*termscr->config_menu_listener.change_transparent_flag)( termscr , flag) ;
		}
	}
	else if( strcmp( key , "brightness") == 0)
	{
		u_int  brightness ;

		if( ! kik_str_to_uint( &brightness , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_brightness)
		{
			(*termscr->config_menu_listener.change_brightness)( termscr , brightness) ;
		}
	}
	else if( strcmp( key , "fade_ratio") == 0)
	{
		u_int  fade_ratio ;

		if( ! kik_str_to_uint( &fade_ratio , value))
		{
			return  0 ;
		}

		if( termscr->config_menu_listener.change_fade_ratio)
		{
			(*termscr->config_menu_listener.change_fade_ratio)( termscr , fade_ratio) ;
		}
	}
	else if( strcmp( key , "use_anti_alias") == 0)
	{
		ml_font_present_t  font_present ;

		font_present = termscr->font_present ;
		
		if( strcmp( value , "true") == 0)
		{
			font_present |= FONT_AA ;
		}
		else if( strcmp( value , "false") == 0)
		{
			font_present &= ~FONT_AA ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_font_present)
		{
			(*termscr->config_menu_listener.change_font_present)( termscr , font_present) ;
		}
	}
	else if( strcmp( key , "use_variable_column_width") == 0)
	{
		ml_font_present_t  font_present ;

		font_present = termscr->font_present ;
		
		if( strcmp( value , "true") == 0)
		{
			font_present |= FONT_VAR_WIDTH ;
		}
		else if( strcmp( value , "false") == 0)
		{
			font_present &= ~FONT_VAR_WIDTH ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_font_present)
		{
			(*termscr->config_menu_listener.change_font_present)( termscr , font_present) ;
		}
	}
	else if( strcmp( key , "use_multi_column_char") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_multi_col_char_flag)
		{
			(*termscr->config_menu_listener.change_multi_col_char_flag)( termscr , flag) ;
		}
	}
	else if( strcmp( key , "use_bidi") == 0)
	{
		int  flag ;
		
		if( strcmp( value , "true") == 0)
		{
			flag = 1 ;
		}
		else if( strcmp( value , "false") == 0)
		{
			flag = 0 ;
		}
		else
		{
			return  0 ;
		}
		
		if( termscr->config_menu_listener.change_bidi_flag)
		{
			(*termscr->config_menu_listener.change_bidi_flag)( termscr , flag) ;
		}
	}
	else if( strcmp( key , "xim") == 0)
	{
		char *  xim ;
		char *  locale ;
		char *  p ;

		xim = value ;

		if( ( p = strchr( value , ':')) == NULL)
		{
			locale = "" ;
		}
		else
		{
			*p = '\0' ;
			locale = p + 1 ;
		}

		if( termscr->config_menu_listener.change_xim)
		{
			(*termscr->config_menu_listener.change_xim)( termscr , xim , locale) ;
		}
	}
	else if( strcmp( key , "wall_picture") == 0)
	{
		if( termscr->config_menu_listener.change_wall_picture)
		{
			(*termscr->config_menu_listener.change_wall_picture)( termscr , value) ;
		}
	}
	else if( strcmp( key , "full_reset") == 0)
	{
		if( termscr->config_menu_listener.full_reset)
		{
			(*termscr->config_menu_listener.full_reset)( termscr) ;
		}
	}
	
	ml_term_screen_start_vt100_cmd( termscr) ;

	return  1 ;
}

int
ml_term_screen_get_config(
	ml_term_screen_t *  termscr ,
	char *  key
	)
{
	char *  value ;
	char  digit[DIGIT_STR_LEN(u_int) + 1] ;
	char *  true = "true" ;
	char *  false = "false" ;
	char  cwd[PATH_MAX] ;
		
	if( strcmp( key , "encoding") == 0)
	{
		value = ml_get_char_encoding_name( (*termscr->encoding_listener->encoding)(
				termscr->encoding_listener->self)) ;
	}
	else if( strcmp( key , "iscii_lang") == 0)
	{
		value = ml_iscii_get_lang_name( termscr->iscii_lang) ;
	}
	else if( strcmp( key , "fg_color") == 0)
	{
		value = ml_get_color_name( termscr->color_man , ml_window_get_fg_color( &termscr->window)) ;
	}
	else if( strcmp( key , "bg_color") == 0)
	{
		value = ml_get_color_name( termscr->color_man , ml_window_get_bg_color( &termscr->window)) ;
	}
	else if( strcmp( key , "sb_fg_color") == 0)
	{
		if( termscr->screen_scroll_listener && termscr->screen_scroll_listener->fg_color)
		{
			value = ml_get_color_name( termscr->color_man ,
					(*termscr->screen_scroll_listener->fg_color)(
					termscr->screen_scroll_listener->self)) ;
		}
		else
		{
			value = NULL ;
		}
	}
	else if( strcmp( key , "sb_bg_color") == 0)
	{
		if( termscr->screen_scroll_listener && termscr->screen_scroll_listener->bg_color)
		{
			value = ml_get_color_name( termscr->color_man ,
					(*termscr->screen_scroll_listener->bg_color)(
					termscr->screen_scroll_listener->self)) ;
		}
		else
		{
			value = NULL ;
		}
	}
	else if( strcmp( key , "tabsize") == 0)
	{
		sprintf( digit , "%d" , ml_term_model_get_tab_size( termscr->model)) ;
		value = digit ;
	}
	else if( strcmp( key , "logsize") == 0)
	{
		sprintf( digit , "%d" , ml_term_model_get_log_size( termscr->model)) ;
		value = digit ;
	}
	else if( strcmp( key , "fontsize") == 0)
	{
		sprintf( digit , "%d" , termscr->font_man->font_size) ;
		value = digit ;
	}
	else if( strcmp( key , "line_space") == 0)
	{
		sprintf( digit , "%d" , termscr->font_man->line_space) ;
		value = digit ;
	}
	else if( strcmp( key , "screen_width_ratio") == 0)
	{
		sprintf( digit , "%d" , termscr->screen_width_ratio) ;
		value = digit ;
	}
	else if( strcmp( key , "screen_height_ratio") == 0)
	{
		sprintf( digit , "%d" , termscr->screen_height_ratio) ;
		value = digit ;
	}
	else if( strcmp( key , "scrollbar_view_name") == 0)
	{
		if( termscr->screen_scroll_listener && termscr->screen_scroll_listener->view_name)
		{
			value = (*termscr->screen_scroll_listener->view_name)(
					termscr->screen_scroll_listener->self) ;
		}
		else
		{
			value = NULL ;
		}
	}
	else if( strcmp( key , "mod_meta_mode") == 0)
	{
		value = ml_get_mod_meta_mode_name( termscr->mod_meta_mode) ;
	}
	else if( strcmp( key , "bel_mode") == 0)
	{
		value = ml_get_bel_mode_name( termscr->bel_mode) ;
	}
	else if( strcmp( key , "vertical_mode") == 0)
	{
		value = ml_get_vertical_mode_name( termscr->vertical_mode) ;
	}
	else if( strcmp( key , "scrollbar_mode") == 0)
	{
		if( termscr->screen_scroll_listener &&
			termscr->screen_scroll_listener->sb_mode)
		{
			value = ml_get_sb_mode_name( (*termscr->screen_scroll_listener->sb_mode)(
				termscr->screen_scroll_listener->self)) ;
		}
		else
		{
			value = ml_get_sb_mode_name( SB_NONE) ;
		}
	}
	else if( strcmp( key , "use_combining") == 0)
	{
		if( ml_is_using_char_combining())
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_dynamic_comb") == 0)
	{
		if( termscr->use_dynamic_comb)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "copy_paste_via_ucs") == 0)
	{
		if( termscr->copy_paste_via_ucs)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_transbg") == 0)
	{
		if( termscr->window.is_transparent)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "brightness") == 0)
	{
		sprintf( digit , "%d" , termscr->pic_mod.brightness) ;
		value = digit ;
	}
	else if( strcmp( key , "fade_ratio") == 0)
	{
		sprintf( digit , "%d" , termscr->fade_ratio) ;
		value = digit ;
	}
	else if( strcmp( key , "use_anti_alias") == 0)
	{
		if( termscr->font_present & FONT_AA)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_variable_column_width") == 0)
	{
		if( termscr->font_present & FONT_VAR_WIDTH)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_multi_column_char") == 0)
	{
		if( termscr->font_man->use_multi_col_char)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "use_bidi") == 0)
	{
		if( termscr->use_bidi)
		{
			value = true ;
		}
		else
		{
			value = false ;
		}
	}
	else if( strcmp( key , "xim") == 0)
	{
		value = ml_xic_get_xim_name( &termscr->window) ;
	}
	else if( strcmp( key , "locale") == 0)
	{
		value = kik_get_locale() ;
	}
	else if( strcmp( key , "wall_picture") == 0)
	{
		if( termscr->pic_file_path)
		{
			value = termscr->pic_file_path ;
		}
		else
		{
			value = "" ;
		}
	}
	else if( strcmp( key , "pwd") == 0)
	{
		value = getcwd( cwd , PATH_MAX) ;
	}
	else
	{
		goto  error ;
	}

	if( value == NULL)
	{
		goto  error ;
	}

	ml_write_to_pty( termscr->pty , "#" , 1) ;
	ml_write_to_pty( termscr->pty , key , strlen( key)) ;
	ml_write_to_pty( termscr->pty , "=" , 1) ;
	ml_write_to_pty( termscr->pty , value , strlen( value)) ;
	ml_write_to_pty( termscr->pty , "\n" , 1) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " #%s=%s\n" , key , value) ;
#endif

	return  1 ;

error:
	ml_write_to_pty( termscr->pty , "#error\n" , 7) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " #error\n") ;
#endif

	return  0 ;
}
