/*
 *	$Id$
 */

#include  "x_scrollbar.h"

#include  <kiklib/kik_debug.h>

#include  "cocoa.h"


/* --- static functions --- */

static void
update_scroller(
	x_scrollbar_t *  sb
	)
{
	scroller_update( sb->window.my_window ,
		sb->num_of_filled_log_lines == 0 ? 1.0 :
		((float)(sb->current_row + sb->num_of_filled_log_lines)) /
			((float)sb->num_of_filled_log_lines) ,
		((float)sb->num_of_scr_lines) /
			((float)(sb->num_of_filled_log_lines + sb->num_of_scr_lines))) ;
}

/*
 * callbacks of x_window_t events.
 */

static void
window_resized(
	x_window_t *  win
	)
{
	x_scrollbar_t *  sb ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " scrollbar resized.\n") ;
#endif

	sb = (x_scrollbar_t*) win ;

	sb->num_of_scr_lines = sb->window.height / sb->line_height ;

	update_scroller( sb) ;
}


/* --- global functions --- */

int
x_scrollbar_init(
	x_scrollbar_t *  sb ,
	x_scrollbar_event_listener_t *  sb_listener ,
	char *  view_name ,
	char *  fg_color ,
	char *  bg_color ,
	u_int  height ,
	u_int  line_height ,
	u_int  num_of_log_lines ,
	u_int  num_of_filled_log_lines ,
	int  use_transbg ,
	x_picture_modifier_t *  pic_mod
	)
{
	sb->view_name = "simple" ;
	sb->view = NULL ;
	sb->fg_color = NULL ;
	sb->bg_color = NULL ;

	sb->sb_listener = sb_listener ;

	if( ! x_window_init( &sb->window , 10 , height , 10 , 0 , 0 , 0 , 0 , 0 , 0 , 0))
	{
		return  0 ;
	}

	sb->line_height = line_height ;
	sb->num_of_scr_lines = height / line_height ;
	sb->num_of_log_lines = num_of_log_lines ;
	sb->num_of_filled_log_lines = num_of_filled_log_lines ;

	sb->window.window_resized = window_resized ;

	return  1 ;
}

int
x_scrollbar_final(
	x_scrollbar_t *  sb
	)
{
	return  1 ;
}

int
x_scrollbar_set_num_of_log_lines(
	x_scrollbar_t *  sb ,
	u_int  num_of_log_lines
	)
{
	if( sb->num_of_log_lines == num_of_log_lines)
	{
		return  1 ;
	}

	sb->num_of_log_lines = num_of_log_lines ;

	if( sb->num_of_filled_log_lines > sb->num_of_log_lines)
	{
		sb->num_of_filled_log_lines = sb->num_of_log_lines ;
	}

	update_scroller( sb) ;

	return  1 ;
}

int
x_scrollbar_set_num_of_filled_log_lines(
	x_scrollbar_t *  sb ,
	u_int  lines
	)
{
	if( lines > sb->num_of_log_lines)
	{
		lines = sb->num_of_log_lines ;
	}

	if( sb->num_of_filled_log_lines == lines)
	{
		return  1 ;
	}

	sb->num_of_filled_log_lines = lines ;

	update_scroller( sb) ;

	return  1 ;
}

int
x_scrollbar_line_is_added(
	x_scrollbar_t *  sb
	)
{
	if( (*sb->sb_listener->screen_is_static)(sb->sb_listener->self))
	{
		if( sb->num_of_filled_log_lines < sb->num_of_log_lines)
		{
			sb->num_of_filled_log_lines ++ ;
		}

		sb->current_row -- ;
	}
	else if( sb->num_of_filled_log_lines == sb->num_of_log_lines)
	{
		return  0 ;
	}
	else
	{
		sb->num_of_filled_log_lines ++ ;
	}

	update_scroller( sb) ;

	return  1 ;
}

int
x_scrollbar_reset(
	x_scrollbar_t *  sb
	)
{
	sb->current_row = 0 ;

	update_scroller( sb) ;

	return  1 ;
}

int
x_scrollbar_move_upward(
	x_scrollbar_t *  sb ,
	u_int  size
	)
{
	/*
	 * XXX Adhoc solution
	 * Fix x_screen.c:bs_{half_}page_{up|down}ward() instead.
	 */
	if( sb->current_row + sb->num_of_filled_log_lines == 0)
	{
		return  0 ;
	}

	return  x_scrollbar_move( sb , sb->current_row - size) ;
}

int
x_scrollbar_move_downward(
	x_scrollbar_t *  sb ,
	u_int  size
	)
{
	if( sb->current_row >= 0)
	{
		return  0 ;
	}

	return  x_scrollbar_move( sb , sb->current_row + size) ;
}

int
x_scrollbar_move(
	x_scrollbar_t *  sb ,
	int  row
	)
{
	if( 0 < row)
	{
		row = 0 ;
	}
	else if( row + (int)sb->num_of_filled_log_lines < 0)
	{
		row = -(sb->num_of_filled_log_lines) ;
	}

	if( sb->current_row == row)
	{
		return  0 ;
	}

	sb->current_row = row ;

	update_scroller( sb) ;

	return  1 ;
}

int
x_scrollbar_set_line_height(
	x_scrollbar_t *  sb ,
	u_int  line_height
	)
{
	if( sb->line_height == line_height)
	{
		return  0 ;
	}
	
	sb->line_height = line_height ;

	return  1 ;
}

int
x_scrollbar_set_fg_color(
	x_scrollbar_t *  sb ,
	char *  fg_color
	)
{
	return  1 ;
}

int
x_scrollbar_set_bg_color(
	x_scrollbar_t *  sb ,
	char *  bg_color
	)
{
	return  1 ;
}

int
x_scrollbar_change_view(
	x_scrollbar_t *  sb ,
	char *  name
	)
{
	return  1 ;
}

int
x_scrollbar_set_transparent(
	x_scrollbar_t *  sb ,
	x_picture_modifier_t *  pic_mod ,
	int  force
	)
{
	return  1 ;
}

int
x_scrollbar_unset_transparent(
	x_scrollbar_t *  sb
	)
{
	return  1 ;
}


void
x_scrollbar_is_moved(
	x_scrollbar_t *  sb ,
	float  pos
	)
{
	sb->current_row = sb->num_of_filled_log_lines * pos - sb->num_of_filled_log_lines ;

	if( sb->sb_listener->screen_scroll_to)
	{
		(*sb->sb_listener->screen_scroll_to)( sb->sb_listener->self , sb->current_row) ;
	}
}
