/*
 *	$Id$
 */

#include  "x_window.h"

#include  <stdlib.h>		/* abs */
#include  <string.h>		/* memset/memcpy */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* realloc/free */
#include  <kiklib/kik_util.h>	/* K_MIN/K_MAX */

#include  "../x_xic.h"
#include  "../x_picture.h"
#include  "../x_imagelib.h"
#ifndef  DISABLE_XDND
#include  "../x_dnd.h"
#endif

#include  "x_display.h"		/* x_display_get_cursor */
#include  "cocoa.h"


/* win->width is not multiples of (win)->width_inc if window is maximized. */
#define  RIGHT_MARGIN(win) \
	((win)->width_inc ? ((win)->width - (win)->min_width) % (win)->width_inc : 0)
#define  BOTTOM_MARGIN(win) \
	((win)->height_inc ? ((win)->height - (win)->min_height) % (win)->height_inc : 0)

#define  ParentRelative  (1L)

#define  IS_XSCREEN(win)  ((win)->selection_cleared)
#define  IS_XLAYOUT(win)  ((win)->child_window_resized)

#if  0
#define  DEBUG_SCROLLABLE
#endif

#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static int  click_interval = 250 ;	/* millisecond, same as xterm. */
static int  use_urgent_bell ;


/* --- static functions --- */

static void
urgent_bell(
	x_window_t *  win ,
	int  on
	)
{
	if( use_urgent_bell && ( ! win->is_focused || ! on))
	{
		win = x_get_root_window( win) ;

		app_urgent_bell( on) ;
	}
}

static void
notify_focus_in_to_children(
	x_window_t *  win
	)
{
	u_int  count ;

	if( ! win->is_focused)
	{
		if( win->inputtable > 0 ||
		    win->parent == NULL)	/* check_scrollable checks root->is_focused */
		{
			win->is_focused = 1 ;
		}

		if( win->inputtable > 0)
		{
			x_xic_set_focus( win) ;

			if( win->window_focused)
			{
				(*win->window_focused)( win) ;
			}
		}
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		notify_focus_in_to_children( win->children[count]) ;
	}
}

static void
notify_focus_out_to_children(
	x_window_t *  win
	)
{
	u_int  count ;

	if( win->is_focused)
	{
		win->is_focused = 0 ;

		x_xic_unset_focus( win) ;

		if( win->window_unfocused)
		{
			(*win->window_unfocused)( win) ;
		}
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		notify_focus_out_to_children( win->children[count]) ;
	}
}

static void
notify_move_to_children(
	x_window_t *  win
	)
{
	int  count ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		notify_move_to_children( win->children[count]) ;
	}
}

static u_int
total_min_width(
	x_window_t *  win
	)
{
	int  count ;
	u_int  min_width ;

	min_width = win->min_width + win->hmargin * 2 ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			/* XXX */
			min_width += total_min_width( win->children[count]) ;
		}
	}

	return  min_width ;
}

static u_int
total_min_height(
	x_window_t *  win
	)
{
	int  count ;
	u_int  min_height ;

	min_height = win->min_height + win->vmargin * 2 ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			/* XXX */
			min_height += total_min_height( win->children[count]) ;
		}
	}

	return  min_height ;
}

static u_int
total_width_inc(
	x_window_t *  win
	)
{
	int  count ;
	u_int  width_inc ;

	width_inc = win->width_inc ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			u_int  sub_inc ;

			/*
			 * XXX
			 * we should calculate least common multiple of width_inc and sub_inc.
			 */
			if( ( sub_inc = total_width_inc( win->children[count])) > width_inc)
			{
				width_inc = sub_inc ;
			}
		}
	}

	return  width_inc ;
}

static u_int
total_height_inc(
	x_window_t *  win
	)
{
	int  count ;
	u_int  height_inc ;

	height_inc = win->height_inc ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		if( win->children[count]->is_mapped)
		{
			u_int  sub_inc ;

			/*
			 * XXX
			 * we should calculate least common multiple of width_inc and sub_inc.
			 */
			if( ( sub_inc = total_height_inc( win->children[count])) > height_inc)
			{
				height_inc = sub_inc ;
			}
		}
	}

	return  height_inc ;
}

static void
reset_input_focus(
	x_window_t *  win
	)
{
	u_int  count ;

	if( win->inputtable)
	{
		win->inputtable = -1 ;
	}
	else
	{
		win->inputtable = 0 ;
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		reset_input_focus( win->children[count]) ;
	}
}

static int
clear_margin_area(
	x_window_t *  win
	)
{
	u_int  right_margin ;
	u_int  bottom_margin ;
	u_int  win_width ;
	u_int  win_height ;

	right_margin = RIGHT_MARGIN(win) ;
	bottom_margin = BOTTOM_MARGIN(win) ;
	win_width = win->width - right_margin ;
	win_height = win->height - bottom_margin ;

	if( win->wall_picture)
	{
		Pixmap  pic ;
		int  src_x ;
		int  src_y ;

		if( win->wall_picture == ParentRelative)
		{
			pic = win->parent->wall_picture ;
			src_x = win->x ;
			src_y = win->y ;
		}
		else
		{
			pic = win->wall_picture ;
			src_x = src_y = 0 ;
		}

		if( win->hmargin > 0 || right_margin > 0)
		{
			view_copy_area( win->my_window , pic ,
				src_x , src_y , win->hmargin , ACTUAL_HEIGHT(win) , 0 , 0) ;
			view_copy_area( win->my_window , pic ,
				src_x + win_width + win->hmargin , src_y ,
				win->hmargin + right_margin ,
				ACTUAL_HEIGHT(win) ,
				win_width + win->hmargin , 0) ;
		}

		if( win->vmargin > 0 || bottom_margin > 0)
		{
			view_copy_area( win->my_window , pic ,
				src_x + win->hmargin , src_y ,
				win_width , win->vmargin ,
				win->hmargin , 0) ;
			view_copy_area( win->my_window , pic ,
				src_x + win->hmargin , src_y + win_height + win->vmargin ,
				win_width , win->vmargin + bottom_margin ,
				win->hmargin , win_height + win->vmargin) ;
		}
	}
	else
	{
		if( win->hmargin > 0 || right_margin > 0)
		{
			view_fill_with( win->my_window , &win->bg_color , 0 , 0 ,
				win->hmargin , ACTUAL_HEIGHT(win)) ;
			view_fill_with( win->my_window , &win->bg_color ,
				win_width + win->hmargin , 0 ,
				ACTUAL_WIDTH(win) - win_width - win->hmargin ,
				ACTUAL_HEIGHT(win)) ;
		}

		if( win->vmargin > 0 || bottom_margin > 0)
		{
			view_fill_with( win->my_window , &win->bg_color ,
				win->hmargin , 0 ,
				win_width , win->vmargin) ;
			view_fill_with( win->my_window , &win->bg_color ,
				win->hmargin , win_height + win->vmargin ,
				win_width , ACTUAL_HEIGHT(win) - win_height - win->vmargin) ;
		}
	}

	return  1 ;
}

static void
expose(
	x_window_t *  win ,
	XExposeEvent *  event
	)
{
	int  clear_margin ;

	if( win->update_window_flag)
	{
		if( ! event->force_expose)
		{
			if( win->update_window)
			{
				(*win->update_window)( win , win->update_window_flag) ;
			}

			return ;
		}

		event->x = win->hmargin ;
		event->y = win->vmargin ;
		event->width = win->width ;
		event->height = win->height ;
		clear_margin = 1 ;
	}
	else
	{
		clear_margin = 0 ;

		if( event->x < win->hmargin)
		{
			event->width -= (win->hmargin - event->x) ;
			event->x = win->hmargin ;
			clear_margin = 1 ;
		}

		if( event->y < win->vmargin)
		{
			event->height -= (win->vmargin - event->y) ;
			event->y = win->vmargin ;
			clear_margin = 1 ;
		}

		if( ! clear_margin &&
		    ( event->x + event->width > win->hmargin + win->width ||
		      event->y + event->height > win->vmargin + win->height))
		{
			clear_margin = 1 ;
		}
	}

	if( clear_margin)
	{
		clear_margin_area( win) ;
	}

	if( win->window_exposed)
	{
		(*win->window_exposed)( win ,
			event->x - win->hmargin , event->y - win->vmargin ,
			event->width , event->height) ;
	}
}

static int
need_pointer_motion_mask(
	x_window_t *  win
	)
{
	u_int  count ;

	if( win->event_mask & PointerMotionMask)
	{
		return  1 ;
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		if( need_pointer_motion_mask( win->children[count]))
		{
			return  1 ;
		}
	}

	return  0 ;
}


/* --- global functions --- */

int
x_window_init(
	x_window_t *  win ,
	u_int  width ,
	u_int  height ,
	u_int  min_width ,
	u_int  min_height ,
	u_int  width_inc ,
	u_int  height_inc ,
	u_int  hmargin ,
	u_int  vmargin ,
	int  create_gc ,	/* ignored */
	int  inputtable
	)
{
	memset( win , 0 , sizeof( x_window_t)) ;

	win->fg_color.pixel = 0xff000000 ;
	win->bg_color.pixel = 0xffffffff ;

	win->is_scrollable = 0 ;

	win->is_focused = 0 ;

	win->inputtable = inputtable ;

	/* This flag will map window automatically in x_window_show(). */
	win->is_mapped = 1 ;

	win->create_gc = create_gc ;

	win->width = width ;
	win->height = height ;
	win->min_width = min_width ;
	win->min_height = min_height ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;
	win->hmargin = hmargin ;
	win->vmargin = vmargin ;

	win->prev_clicked_button = -1 ;

	win->app_name = "mlterm" ;

	return	1 ;
}

int
x_window_final(
	x_window_t *  win
	)
{
	u_int  count ;
	Window  my_window ;

#ifdef  __DEBUG
	kik_debug_printf( "[deleting child windows]\n") ;
	x_window_dump_children( win) ;
#endif

	my_window = win->my_window ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_final( win->children[count]) ;
	}

	free( win->children) ;
	win->num_of_children = 0 ;

	x_display_clear_selection( win->disp , win) ;

	x_xic_deactivate( win) ;

	if( my_window)
	{
		/*
		 * win->parent may be NULL because this function is called
		 * from x_layout after x_window_remove_child(),
		 * not only win->parent but also IS_XLAYOUT(win) is necessary.
		 */
		if( ! win->parent && IS_XLAYOUT(win))
		{
			window_dealloc( my_window) ;
		}
		else
		{
			view_dealloc( my_window) ;
		}
	}

	if( win->window_finalized)
	{
		(*win->window_finalized)( win) ;
	}

	return  1 ;
}

int
x_window_set_type_engine(
	x_window_t *  win ,
	x_type_engine_t  type_engine
	)
{
	return  0 ;
}

int
x_window_add_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
	if( event_mask & PointerMotionMask)
	{
		win->event_mask = PointerMotionMask ;

		if( x_get_root_window( win)->my_window)
		{
			window_accepts_mouse_moved_events(
				x_get_root_window( win)->my_window , 1) ;
		}
	}

	return  1 ;
}

int
x_window_remove_event_mask(
	x_window_t *  win ,
	long  event_mask
	)
{
	if( event_mask & PointerMotionMask)
	{
		x_window_t *  root ;

		win->event_mask = 0 ;

		root = x_get_root_window( win) ;
		if( root->my_window && ! need_pointer_motion_mask( root))
		{
			window_accepts_mouse_moved_events( root->my_window , 0) ;
		}
	}

	return  1 ;
}

int
x_window_ungrab_pointer(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_window_set_wall_picture(
	x_window_t *  win ,
	Pixmap  pic ,
	int  do_expose
	)
{
	u_int  count ;

	win->wall_picture = pic ;

	if( win->my_window != None && do_expose)
	{
	#if  0
		view_update( win->my_window , 1) ;
	#endif
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		x_window_set_wall_picture( win->children[count] , ParentRelative , do_expose) ;
	}

	return  1 ;
}

int
x_window_unset_wall_picture(
	x_window_t *  win ,
	int  do_expose
	)
{
	u_int  count ;

	win->wall_picture = None ;

	if( win->my_window != None)
	{
	#if  0
		InvalidateRect( win->my_window, NULL, FALSE) ;
	#endif
	}

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		x_window_set_wall_picture( win->children[count] , ParentRelative , do_expose) ;
	}

	return  1 ;
}

int
x_window_set_transparent(
	x_window_t *  win ,
	x_picture_modifier_t *  pic_mod
	)
{
	return  0 ;
}

int
x_window_unset_transparent(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_window_set_cursor(
	x_window_t *  win ,
	u_int  cursor_shape
	)
{
	win->cursor_shape = cursor_shape ;

	return  1 ;
}

int
x_window_set_fg_color(
	x_window_t *  win ,
	x_color_t *  fg_color
	)
{
	if( win->fg_color.pixel == fg_color->pixel)
	{
		return  0 ;
	}

	win->fg_color = *fg_color ;

	return  1 ;
}

int
x_window_set_bg_color(
	x_window_t *  win ,
	x_color_t *  bg_color
	)
{
	if( win->bg_color.pixel == bg_color->pixel)
	{
		return  0 ;
	}

	win->bg_color = *bg_color ;

	if( win->my_window && IS_XSCREEN(win))
	{
		view_bg_color_changed( win->my_window) ;
		x_window_update_all( win) ;
	}

	return  1 ;
}

int
x_window_add_child(
	x_window_t *  win ,
	x_window_t *  child ,
	int  x ,
	int  y ,
	int  map
	)
{
	void *  p ;

	if( ( p = realloc( win->children , sizeof( *win->children) * (win->num_of_children + 1)))
		== NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif

		return  0 ;
	}

	win->children = p ;

	child->parent = win ;
	child->x = x + win->hmargin ;
	child->y = y + win->vmargin ;

	if( ! ( child->is_mapped = map) && child->inputtable > 0)
	{
		child->inputtable = -1 ;
	}

	win->children[ win->num_of_children ++] = child ;

	return  1 ;
}

int
x_window_remove_child(
	x_window_t *  win ,
	x_window_t *  child
	)
{
	u_int  count ;

	for( count = 0 ; count < win->num_of_children ; count++)
	{
		if( win->children[count] == child)
		{
			child->parent = NULL ;
			win->children[count] = win->children[--win->num_of_children] ;

			return  1 ;
		}
	}

	return  0 ;
}

x_window_t *
x_get_root_window(
	x_window_t *  win
	)
{
	while( win->parent != NULL)
	{
		win = win->parent ;
	}

	return  win ;
}

int
x_window_show(
	x_window_t *  win ,
	int  hint	/* If win->parent(_window) is None,
			   specify XValue|YValue to localte window at win->x/win->y. */
	)
{
	u_int  count ;

	if( win->my_window)
	{
		/* already shown */

		return  0 ;
	}

	if( win->parent)
	{
		win->disp = win->parent->disp ;
		win->parent_window = win->parent->my_window ;
		win->gc = win->parent->gc ;
	}

#ifndef  USE_QUARTZ
	if( hint & XNegative)
	{
		win->x += (win->disp->width - ACTUAL_WIDTH(win)) ;
	}

	if( hint & YNegative)
	{
		win->y += (win->disp->height - ACTUAL_HEIGHT(win)) ;
	}
#endif

#ifndef  DISABLE_XDND
	/* DragAcceptFiles( win->my_window , TRUE) ; */
#endif

	if( win->parent && ! win->parent->is_transparent &&
	    win->parent->wall_picture)
	{
		x_window_set_wall_picture( win , ParentRelative , 0) ;
	}

	/*
	 * This should be called after Window Manager settings, because
	 * x_set_{window|icon}_name() can be called in win->window_realized().
	 */
	if( win->window_realized)
	{
		(*win->window_realized)( win) ;
	}

	/*
	 * showing child windows.
	 */

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_show( win->children[count] , 0) ;
	}

	/*
	 * really visualized.
	 */

	if( win->is_mapped)
	{
		if( win->inputtable > 0)
		{
			reset_input_focus( x_get_root_window( win)) ;
			win->inputtable = 1 ;
		}
	}

	if( win->is_transparent)
	{
		x_window_set_transparent( win , win->pic_mod) ;
	}

	if( IS_XSCREEN(win) && win->parent->my_window)
	{
		view_alloc( win) ;
	}

	return  1 ;
}

int
x_window_map(
	x_window_t *  win
	)
{
	if( win->is_mapped)
	{
		return  1 ;
	}

	if( win->parent)
	{
		view_set_hidden( win->my_window , 0) ;
	}

	win->is_mapped = 1 ;

	return  1 ;
}

int
x_window_unmap(
	x_window_t *  win
	)
{
	if( ! win->is_mapped)
	{
		return  1 ;
	}

	if( win->parent)
	{
		view_set_hidden( win->my_window , 1) ;

		/* Scrollbar position is corrupt without this. */
		x_window_move( win , -1 , -1) ;
	}

	win->is_mapped = 0 ;

	return  1 ;
}

int
x_window_resize(
	x_window_t *  win ,
	u_int  width ,			/* excluding margin */
	u_int  height ,			/* excluding margin */
	x_resize_flag_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	if( win->width == width && win->height == height)
	{
		return  0 ;
	}

	/* Max width of each window is screen width. */
	if( (flag & LIMIT_RESIZE) && win->disp->width < width)
	{
		win->width = win->disp->width - win->hmargin * 2 ;
	}
	else
	{
		win->width = width ;
	}

	/* Max height of each window is screen height. */
	if( (flag & LIMIT_RESIZE) && win->disp->height < height)
	{
		win->height = win->disp->height - win->vmargin * 2 ;
	}
	else
	{
		win->height = height ;
	}

	if( win->parent)
	{
		view_set_rect( win->my_window , win->x ,
			ACTUAL_HEIGHT(win->parent) - ACTUAL_HEIGHT(win) - win->y ,
			ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;

		if( (flag & NOTIFY_TO_PARENT) && win->parent->child_window_resized)
		{
			(*win->parent->child_window_resized)( win->parent , win) ;
		}
	}
	else
	{
		/*
		 * win->parent may be NULL because this function is called
		 * from x_layout after x_window_remove_child(),
		 * not only win->parent but also IS_XLAYOUT(win) is necessary.
		 */
		if( IS_XLAYOUT(win))
		{
			window_resize( win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
		}
	}

	if( (flag & NOTIFY_TO_MYSELF) && win->window_resized)
	{
		(*win->window_resized)( win) ;
		win->update_window_flag = 0 ;	/* force redraw */
	}

	return  1 ;
}

/*
 * !! Notice !!
 * This function is not recommended.
 * Use x_window_resize if at all possible.
 */
int
x_window_resize_with_margin(
	x_window_t *  win ,
	u_int  width ,
	u_int  height ,
	x_resize_flag_t  flag	/* NOTIFY_TO_PARENT , NOTIFY_TO_MYSELF */
	)
{
	return  x_window_resize( win , width - win->hmargin * 2 ,
			height - win->vmargin * 2 , flag) ;
}

int
x_window_set_normal_hints(
	x_window_t *  win ,
	u_int  min_width ,
	u_int  min_height ,
	u_int  width_inc ,
	u_int  height_inc
	)
{
	x_window_t *  root ;

	win->min_width = min_width ;
	win->min_height = min_height  ;
	win->width_inc = width_inc ;
	win->height_inc = height_inc ;

	/* root is always x_layout_t (use_mdi is always true) on macosx/cocoa. */
	root = x_get_root_window( win) ;

	window_set_normal_hints( root->my_window ,
		total_width_inc( root) , total_height_inc( root)) ;

	return  1 ;
}

int
x_window_set_override_redirect(
	x_window_t *  win ,
	int  flag
	)
{
	x_window_t *  root ;

	if( ( root = x_get_root_window( win))->my_window)
	{
	#if  0
		if( flag)
		{
			SetWindowLong( root->my_window , GWL_STYLE , 0) ;
			win->cmd_show = SW_SHOWNA ;

			SetWindowPos( root->my_window , HWND_TOPMOST , 0 , 0 , 0 , 0 ,
				SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER) ;
		}
		else
		{
			SetWindowLong( root->my_window , GWL_STYLE ,
				! win->parent ? WS_OVERLAPPEDWINDOW : WS_CHILD | WS_VISIBLE) ;
			win->cmd_show = SW_SHOWNORMAL ;
		}
	#endif

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
x_window_set_borderless_flag(
	x_window_t *  win ,
	int  flag
	)
{
	return  0 ;
}

int
x_window_move(
	x_window_t *  win ,
	int  x ,
	int  y
	)
{
	if( win->parent)
	{
		x += win->parent->hmargin ;
		y += win->parent->vmargin ;
	}

	if( win->x == x && win->y == y)
	{
		return  0 ;
	}

	win->x = x ;
	win->y = y ;

	if( win->parent)
	{
		view_set_rect( win->my_window , x ,
			ACTUAL_HEIGHT(win->parent) - ACTUAL_HEIGHT(win) - y ,
			ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;
	}

	return  1 ;
}

/*
 * This function can be used in context except window_exposed and update_window events.
 */
int
x_window_clear(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
#ifdef  AUTO_CLEAR_MARGIN
	if( x + width >= win->width)
	{
		/* Clearing margin area */
		width += win->hmargin ;
	}

	if( x > 0)
#endif
	{
		x += win->hmargin ;
	}
#ifdef  AUTO_CLEAR_MARGIN
	else
	{
		/* Clearing margin area */
		width += win->hmargin ;
	}

	if( y + height >= win->height)
	{
		/* Clearing margin area */
		height += win->vmargin ;
	}

	if( y > 0)
#endif
	{
		y += win->vmargin ;
	}
#ifdef  AUTO_CLEAR_MARGIN
	else
	{
		/* Clearing margin area */
		height += win->vmargin ;
	}
#endif

	if( win->wall_picture)
	{
		Pixmap  pic ;
		int  src_x ;
		int  src_y ;

		if( win->wall_picture == ParentRelative)
		{
			pic = win->parent->wall_picture ;
			src_x = win->x ;
			src_y = win->y ;
		}
		else
		{
			pic = win->wall_picture ;
			src_x = src_y = 0 ;
		}

		view_copy_area( win->my_window , pic ,
			src_x + x , src_y + y , width , height , x , y) ;
	}
	else
	{
		view_fill_with( win->my_window , &win->bg_color , x , y , width , height) ;
	}

	return  1 ;
}

int
x_window_clear_all(
	x_window_t *  win
	)
{
	return  x_window_clear( win , 0 , 0 , win->width , win->height) ;
}

int
x_window_fill(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	return  x_window_fill_with( win , &win->fg_color , x , y , width , height) ;
}

int
x_window_fill_with(
	x_window_t *  win ,
	x_color_t *  color ,
	int  x ,
	int  y ,
	u_int	width ,
	u_int	height
	)
{
	view_fill_with( win->my_window , color , x + win->hmargin , y + win->vmargin ,
		width , height) ;

	return  1 ;
}

/*
 * This function can be used in context except window_exposed and update_window events.
 */
int
x_window_blank(
	x_window_t *  win
	)
{
	return  1 ;
}

#if  0
/*
 * XXX
 * at the present time , not used and not maintained.
 */
int
x_window_fill_all_with(
	x_window_t *  win ,
	x_color_t *  color
	)
{
	return  0 ;
}
#endif

int
x_window_update(
	x_window_t *  win ,
	int  flag
	)
{
	if( win->update_window_flag)
	{
		win->update_window_flag |= flag ;
	}
	else
	{
		win->update_window_flag = flag ;
		view_update( win->my_window , 0) ;
	}

	return  1 ;
}

int
x_window_update_all(
	x_window_t *  win
	)
{
	u_int  count ;

	if( IS_XSCREEN(win))
	{
		view_update( win->my_window , 1) ;
	}

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_update_all( win->children[count]) ;
	}

	return  1 ;
}

void
x_window_idling(
	x_window_t *  win
	)
{
	u_int  count ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_idling( win->children[count]) ;
	}

#ifdef  __DEBUG
	if( win->button_is_pressing)
	{
		kik_debug_printf( KIK_DEBUG_TAG " button is pressing...\n") ;
	}
#endif

	if( win->button_is_pressing && win->button_press_continued)
	{
		(*win->button_press_continued)( win , &win->prev_button_press_event) ;
	}
	else if( win->idling)
	{
		(*win->idling)( win) ;
	}
}

/*
 * Return value: 0 => different window.
 *               1 => finished processing.
 *              -1 => continuing default processing.
 */
int
x_window_receive_event(
	x_window_t *   win ,
	XEvent *  event
	)
{
  	switch( event->type)
        {
	case  X_KEY_FOCUS_IN:
		reset_input_focus( x_get_root_window( win)) ;
		win->inputtable = 1 ;
		win = x_get_root_window( win) ;
		notify_focus_out_to_children( win) ;
		/* Fall through */

	case  X_FOCUS_IN:
		urgent_bell( win , 0) ;
		notify_focus_in_to_children( win) ;
		break ;

	case  X_FOCUS_OUT:
		notify_focus_out_to_children( win) ;
		break ;

	case  X_BUTTON_PRESS:
		if( win->button_pressed)
		{
			XButtonEvent *  bev ;

			bev = (XButtonEvent*)event ;
			bev->x -= win->hmargin ;
			bev->y -= win->vmargin ;

			(*win->button_pressed)( win , bev , bev->click_count) ;

			win->button_is_pressing = 1 ;
			win->prev_button_press_event = *bev ;
		}
		break ;

	case  X_BUTTON_RELEASE:
		if( win->button_released)
		{
			XButtonEvent *  bev ;

			bev = (XButtonEvent*)event ;
			bev->x -= win->hmargin ;
			bev->y -= win->vmargin ;

			(*win->button_released)( win , bev) ;

			win->button_is_pressing = 0 ;
		}
		break ;

	case  X_BUTTON_MOTION:
		if( win->button_motion)
		{
			XMotionEvent *  mev ;

			mev = (XMotionEvent*)event ;
			mev->x -= win->hmargin ;
			mev->y -= win->vmargin ;

			(*win->button_motion)( win , mev) ;

			/* following button motion ... */
			win->prev_button_press_event.x = mev->x ;
			win->prev_button_press_event.y = mev->y ;
			win->prev_button_press_event.time = mev->time ;
		}
		break ;

	case  X_POINTER_MOTION:
		if( win->pointer_motion)
		{
			XMotionEvent *  mev ;

			mev = (XMotionEvent*)event ;
			mev->x -= win->hmargin ;
			mev->y -= win->vmargin ;

			(*win->pointer_motion)( win , mev) ;
		}
		break ;

	case  X_KEY_PRESS:
		if( win->key_pressed)
		{
			(*win->key_pressed)( win , (XKeyEvent*)event) ;
		}
		break ;

	case  X_EXPOSE:
		expose( win , (XExposeEvent*)event) ;
		break ;

	case  X_SELECTION_REQUESTED:
		if( win->utf_selection_requested)
		{
			(*win->utf_selection_requested)( win ,
				(XSelectionRequestEvent*)event , 0) ;
		}
		break ;

	case  X_SELECTION_NOTIFIED:
		if( win->utf_selection_notified)
		{
			(*win->utf_selection_notified)( win ,
				((XSelectionNotifyEvent*)event)->data ,
				((XSelectionNotifyEvent*)event)->len) ;
		}
		break ;

	case  X_CLOSE_WINDOW:
		/* root window */
		win->my_window = None ;
		if( win->window_deleted)
		{
			(*win->window_deleted)( win) ;
		}
		break ;
	}

	return  1 ;
}

size_t
x_window_get_str(
	x_window_t *  win ,
	u_char *  seq ,
	size_t  seq_len ,
	mkf_parser_t **  parser ,
	KeySym *  keysym ,
	XKeyEvent *  event
	)
{
	return  x_xic_get_str( win, seq, seq_len, parser, keysym, event) ;
}

/*
 * Scroll functions.
 * The caller side should clear the scrolled area.
 */

int
x_window_scroll_upward(
	x_window_t *  win ,
	u_int  height
	)
{
	return  x_window_scroll_upward_region( win , 0 , win->height , height) ;
}

int
x_window_scroll_upward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  height
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->height || boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" boundary start %d end %d height %d in window((h) %d (w) %d)\n" ,
			boundary_start , boundary_end , height , win->height , win->width) ;
	#endif

		return  0 ;
	}

	view_scroll( win->my_window,
		win->hmargin, win->vmargin + boundary_start + height,	/* src */
		win->width, boundary_end - boundary_start - height,		/* size */
		win->hmargin, win->vmargin + boundary_start) ;		/* dst */

	return  1 ;
}

int
x_window_scroll_downward(
	x_window_t *  win ,
	u_int  height
	)
{
	return  x_window_scroll_downward_region( win , 0 , win->height , height) ;
}

int
x_window_scroll_downward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  height
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->height || boundary_end <= boundary_start + height)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " boundary start %d end %d height %d\n" ,
			boundary_start , boundary_end , height) ;
	#endif

		return  0 ;
	}

	view_scroll( win->my_window ,
		win->hmargin , win->vmargin + boundary_start,	/* src */
		win->width, boundary_end - boundary_start - height,	/* size */
		win->hmargin, win->vmargin + boundary_start + height) ;	/* dst */

	return  1 ;
}

int
x_window_scroll_leftward(
	x_window_t *  win ,
	u_int  width
	)
{
	return  x_window_scroll_leftward_region( win , 0 , win->width , width) ;
}

int
x_window_scroll_leftward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  width
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->width || boundary_end <= boundary_start + width)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" boundary start %d end %d width %d in window((h) %d (w) %d)\n" ,
			boundary_start , boundary_end , width , win->height , win->width) ;
	#endif

		return  0 ;
	}

	view_scroll( win->my_window ,
		win->hmargin + boundary_start + width , win->vmargin ,	/* src */
		boundary_end - boundary_start - width , win->height ,	/* size */
		win->hmargin + boundary_start , win->vmargin) ;		/* dst */

	return  1 ;
}

int
x_window_scroll_rightward(
	x_window_t *  win ,
	u_int  width
	)
{
	return  x_window_scroll_rightward_region( win , 0 , win->width , width) ;
}

int
x_window_scroll_rightward_region(
	x_window_t *  win ,
	int  boundary_start ,
	int  boundary_end ,
	u_int  width
	)
{
	if( ! win->is_scrollable)
	{
		return  0 ;
	}

	if( boundary_start < 0 || boundary_end > win->width || boundary_end <= boundary_start + width)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " boundary start %d end %d width %d\n" ,
			boundary_start , boundary_end , width) ;
	#endif

		return  0 ;
	}

	view_scroll( win->my_window ,
		win->hmargin + boundary_start , win->vmargin ,	/* src */
		boundary_end - boundary_start - width , win->height ,	/* size */
		win->hmargin + boundary_start + width , win->vmargin) ;	/* dst */

	return  1 ;
}

int
x_window_copy_area(
	x_window_t *  win ,
	Pixmap  src ,
	PixmapMask  mask ,
	int  src_x ,	/* >= 0 */
	int  src_y ,	/* >= 0 */
	u_int  width ,
	u_int  height ,
	int  dst_x ,	/* >= 0 */
	int  dst_y	/* >= 0 */
	)
{
	if( dst_x >= win->width || dst_y >= win->height)
	{
		return  0 ;
	}

	if( dst_x + width > win->width)
	{
		width = win->width - dst_x ;
	}

	if( dst_y + height > win->height)
	{
		height = win->height - dst_y ;
	}

	view_copy_area( win->my_window , src , src_x , src_y , width , height ,
		dst_x + win->hmargin , dst_y + win->vmargin) ;

	return  1 ;
}

void
x_window_set_clip(
	x_window_t *  win ,
	int  x ,
	int  y ,
	u_int  width ,
	u_int  height
	)
{
	view_set_clip( win->my_window , x + win->hmargin , y + win->vmargin , width , height) ;
}

void
x_window_unset_clip(
	x_window_t *  win
	)
{
	view_unset_clip( win->my_window) ;
}

int
x_window_draw_decsp_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	return  x_window_draw_string( win , font , fg_color , x , y , str , len) ;
}

int
x_window_draw_decsp_image_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	return  x_window_draw_image_string( win , font , fg_color , bg_color ,
			x , y , str , len) ;
}

int
x_window_draw_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	view_draw_string( win->my_window , font , fg_color ,
		x + win->hmargin , y + win->vmargin , str , len) ;

	return  1 ;
}

int
x_window_draw_string16(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	int  x ,
	int  y ,
	XChar2b *  str ,
	u_int  len
	)
{
	view_draw_string16( win->my_window , font , fg_color ,
		x + win->hmargin , y + win->vmargin , str , len) ;

	return  1 ;
}

int
x_window_draw_image_string(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	u_char *  str ,
	u_int  len
	)
{
	view_draw_string( win->my_window , font , fg_color ,
		x + win->hmargin , y + win->vmargin , str , len) ;

	return  1 ;
}

int
x_window_draw_image_string16(
	x_window_t *  win ,
	x_font_t *  font ,
	x_color_t *  fg_color ,
	x_color_t *  bg_color ,
	int  x ,
	int  y ,
	XChar2b *  str ,
	u_int  len
	)
{
	view_draw_string16( win->my_window , font , fg_color ,
		x + win->hmargin , y + win->vmargin , str , len) ;

	return  1 ;
}

int
x_window_draw_rect_frame(
	x_window_t *  win ,
	int  x1 ,
	int  y1 ,
	int  x2 ,
	int  y2
	)
{
	view_draw_rect_frame( win->my_window , &win->fg_color ,
		x1 + win->hmargin , y1 + win->vmargin ,
		x2 + win->hmargin , y2 + win->vmargin) ;

	return  1 ;
}

int
x_set_use_clipboard_selection(
	int  use_it
	)
{
	return  0 ;
}

int
x_is_using_clipboard_selection(void)
{
	return  0 ;
}

int
x_window_set_selection_owner(
	x_window_t *  win ,
	Time  time
	)
{
#if  0
	kik_debug_printf( KIK_DEBUG_TAG " x_window_set_selection_owner.\n") ;
#endif

#if  0
	if( ( ! win->is_sel_owner &&
	      ! x_display_own_selection( win->disp , win)) ||
	    ! cocoa_clipboard_own( win->my_window))
	{
		return  0 ;
	}

	win->is_sel_owner = 1 ;
#else
	cocoa_clipboard_own( win->my_window) ;
#endif

	return  1 ;
}

int
x_window_xct_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
	return  0 ;
}

int
x_window_utf_selection_request(
	x_window_t *  win ,
	Time  time
	)
{
	if( win->utf_selection_notified)
	{
		const char *  str ;

		if( ( str = cocoa_clipboard_get()))
		{
			(*win->utf_selection_notified)( win , str  , strlen(str)) ;

			return  1 ;
		}
	}

	return  0 ;
}

int
x_window_send_picture_selection(
	x_window_t *  win ,
	Pixmap  pixmap ,
	u_int  width ,
	u_int  height
	)
{
#if  0
	HBITMAP  hbmp ;
	HGDIOBJ old ;
	HDC  hdc ;

	if( MessageBox( win->my_window , "Set this picture to the clipboard." ,
		"" , MB_OKCANCEL) != IDOK)
	{
		return  0 ;
	}

	hbmp = CreateCompatibleBitmap( pixmap , width , height) ;
	hdc = CreateCompatibleDC( pixmap) ;
	old = SelectObject( hdc , hbmp) ;
	BitBlt( hdc , 0 , 0 , width , height , pixmap , 0 , 0 , SRCCOPY) ;
	SelectObject( hdc , old) ;
	DeleteDC( hdc) ;

	if( ( ! win->is_sel_owner &&
	      ! x_display_own_selection( win->disp , win)) ||
	    OpenClipboard( win->my_window) == FALSE)
	{
		return  0 ;
	}

	/*
	 * If win->is_sel_owner is already 1, win->is_sel_owner++ prevents
	 * WM_DESTROYCLIPBOARD by EmtpyClipboard() from calling
	 * x_display_clear_selection().
	 */
	win->is_sel_owner ++ ;

	EmptyClipboard() ;
	SetClipboardData( CF_BITMAP , hbmp) ;
	CloseClipboard() ;

	DeleteObject( hbmp) ;
#endif

	return  1 ;
}

int
x_window_send_text_selection(
	x_window_t *  win ,
	XSelectionRequestEvent *  req_ev ,
	u_char *  sel_data ,
	size_t  sel_len ,
	Atom  sel_type
	)
{
	cocoa_clipboard_set( sel_data , sel_len) ;

	return  1 ;
}

int
x_set_window_name(
	x_window_t *  win ,
	u_char *  name
	)
{
	x_window_t *  root ;

	root = x_get_root_window( win) ;

#if  0
	if( name == NULL)
	{
		name = root->app_name ;

	#ifndef  UTF16_IME_CHAR
		SetWindowTextA( root->my_window , name) ;

		return  1 ;
	#endif
	}

	SetWindowTextW( root->my_window , name) ;
#endif

	return  1 ;
}

int
x_set_icon_name(
	x_window_t *  win ,
	u_char *  name
	)
{
	return  0 ;
}

int
x_window_set_icon(
	x_window_t *  win ,
	x_icon_picture_t *  icon
	)
{
	return  0 ;
}

int
x_window_remove_icon(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_window_reset_group(
	x_window_t *  win
	)
{
	return  0 ;
}

int
x_window_get_visible_geometry(
	x_window_t *  win ,
	int *  x ,		/* x relative to parent window */
	int *  y ,		/* y relative to parent window */
	int *  my_x ,		/* x relative to my window */
	int *  my_y ,		/* y relative to my window */
	u_int *  width ,
	u_int *  height
	)
{
	return  1 ;
}

int
x_set_click_interval(
	int  interval
	)
{
	click_interval = interval ;

	return  1 ;
}

u_int
x_window_get_mod_ignore_mask(
	x_window_t *  win ,
	KeySym *  keysyms
	)
{
	return  ~0 ;
}

u_int
x_window_get_mod_meta_mask(
	x_window_t *  win ,
	char *  mod_key
	)
{
	return  ModMask ;
}

int
x_set_use_urgent_bell(
	int  use
	)
{
	use_urgent_bell = use ;

	return  1 ;
}

int
x_window_bell(
	x_window_t *  win ,
	x_bel_mode_t  mode
	)
{
	urgent_bell( win , 1) ;

	if( mode & BEL_VISUAL)
	{
		view_visual_bell( win->my_window) ;
	}

	if( mode & BEL_SOUND)
	{
		cocoa_beep() ;
	}

	return  1 ;
}

int
x_window_translate_coordinates(
	x_window_t *  win,
	int x,
	int y,
	int *  global_x,
	int *  global_y,
	Window *  child
	)
{
#if  0
	POINT  p ;

	p.x = x ;
	p.y = y ;

	ClientToScreen( win->my_window , &p) ;

	*global_x = p.x ;
	*global_y = p.y ;
	*child = None ;
#endif

	return  0 ;
}

void
x_window_set_input_focus(
	x_window_t *  win
	)
{
	reset_input_focus( x_get_root_window( win)) ;
	win->inputtable = 1 ;

	view_set_input_focus( win->my_window) ;
}

#ifdef  DEBUG
void
x_window_dump_children(
	x_window_t *  win
	)
{
	u_int  count ;

	kik_msg_printf( "%p(%li) => " , win , win->my_window) ;
	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		kik_msg_printf( "%p(%li) " , win->children[count] ,
			win->children[count]->my_window) ;
	}
	kik_msg_printf( "\n") ;

	for( count = 0 ; count < win->num_of_children ; count ++)
	{
		x_window_dump_children( win->children[count]) ;
	}
}
#endif
