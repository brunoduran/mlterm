/*
 *	$Id$
 */

#ifndef  NO_IMAGE

#include  "x_picture.h"


#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_mem.h>	/* malloc */
#include  <kiklib/kik_util.h>	/* K_MIN */

/*
 * XXX
 * Don't link libpthread to mlterm for now.
 * Xlib doesn't work on threading without XInitThreads().
 * Threading is not supported for 8 or less bpp framebuffer imaging
 * because of x_display_enable_to_change_cmap() and x_display_set_cmap().
 */
#if ! defined(__CYGWIN__) || ! defined(USE_WIN32GUI)
#undef  HAVE_PTHREAD
#endif

#ifdef  X_PROTOCOL
#undef  HAVE_WINDOWS_H
#endif

#if  ! defined(USE_WIN32API) && defined(HAVE_PTHREAD)
#include  <pthread.h>
#ifndef  HAVE_WINDOWS_H
#include  <sys/select.h>
#include  <unistd.h>
#endif
#endif

#include  "x_imagelib.h"


#define  DUMMY_PIXMAP  ((Pixmap)1)
#define  PIXMAP_IS_ACTIVE(inline_pic) \
	((inline_pic).pixmap && (inline_pic).pixmap != DUMMY_PIXMAP)

#if  0
#define  __DEBUG
#endif


typedef struct  inline_pic_args
{
	int  idx ;
#ifdef  HAVE_WINDOWS_H
	HANDLE  ev ;
#else
	int  ev ;
#endif

} inline_pic_args_t ;


/* --- static varaibles --- */

static x_picture_t **  pics ;
static u_int  num_of_pics ;
static x_icon_picture_t **  icon_pics ;
static u_int  num_of_icon_pics ;
static x_inline_picture_t *  inline_pics ;
static u_int  num_of_inline_pics ;


/* --- static functions --- */

static x_picture_t *
create_picture_intern(
	Display *  display ,
	x_picture_modifier_t *  mod ,
	char *  file_path ,
	u_int  width ,
	u_int  height
	)
{
	x_picture_t *  pic ;

	if( ( pic = malloc( sizeof( x_picture_t))) == NULL)
	{
		return  NULL ;
	}

	if( mod)
	{
		if( ( pic->mod = malloc( sizeof( x_picture_modifier_t))) == NULL)
		{
			goto  error1 ;
		}
		
		*pic->mod = *mod ;
	}
	else
	{
		pic->mod = NULL ;
	}

	if( ( pic->file_path = strdup( file_path)) == NULL)
	{
		goto  error2 ;
	}

	pic->display = display ;
	pic->width = width ;
	pic->height = height ;

	return  pic ;

error2:
	free( pic->mod) ;
error1:
	free( pic) ;

	return  NULL ;
}

static int
delete_picture_intern(
	x_picture_t *  pic
	)
{
	free( pic->file_path) ;
	free( pic->mod) ;
	free( pic) ;

	return  1 ;
}

static x_picture_t *
create_bg_picture(
	x_window_t *  win ,
	x_picture_modifier_t *  mod ,
	char *  file_path
	)
{
	x_picture_t *  pic ;

	if( ! ( pic = create_picture_intern( win->disp->display , mod , file_path ,
			ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win))))
	{
		return  NULL ;
	}

	if( strcmp( file_path , "root") == 0)
	{
		pic->pixmap = x_imagelib_get_transparent_background( win , mod) ;
	}
	else
	{
		pic->pixmap = x_imagelib_load_file_for_background( win , file_path , mod) ;
	}

	if( pic->pixmap == None)
	{
		delete_picture_intern( pic) ;

		return  NULL ;
	}

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " New pixmap %ul is created.\n" , pic->pixmap) ;
#endif

	pic->ref_count = 1 ;

	return  pic ;
}

static int
delete_picture(
	x_picture_t *  pic
	)
{
	/* XXX Pixmap of "pixmap:<ID>" is managed by others, so don't free here. */
	if( strncmp( pic->file_path , "pixmap:" , 7) != 0)
	{
		x_delete_image( pic->display , pic->pixmap) ;
	}

	delete_picture_intern( pic) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " pixmap is deleted.\n") ;
#endif

	return  1 ;
}

static x_icon_picture_t *
create_icon_picture(
	x_display_t *  disp ,
	char *  file_path	/* Don't specify NULL. */
	)
{
	u_int  icon_size = 48 ;
	x_icon_picture_t *  pic ;

	if( ( pic = malloc( sizeof( x_icon_picture_t))) == NULL)
	{
		return  NULL ;
	}

	if( ( pic->file_path = strdup( file_path)) == NULL)
	{
		free( pic->file_path) ;
		
		return  NULL ;
	}
	
	if( ! x_imagelib_load_file( disp , file_path ,
		&(pic->cardinal) , &(pic->pixmap) , &(pic->mask) , &icon_size , &icon_size))
	{
		free( pic->file_path) ;
		free( pic) ;

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Failed to load icon file(%s).\n" , file_path) ;
	#endif

		return  NULL ;
	}

	pic->disp = disp ;
	pic->ref_count = 1 ;

#if  0
	kik_debug_printf( KIK_DEBUG_TAG " Successfully loaded icon file %s.\n" , file_path) ;
#endif

	return  pic ;
}

static int
delete_icon_picture(
	x_icon_picture_t *  pic
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " %s icon will be deleted.\n" , pic->file_path) ;
#endif

	x_delete_image( pic->disp->display , pic->pixmap) ;
	x_delete_mask( pic->disp->display , pic->mask) ;

	free( pic->cardinal) ;
	free( pic->file_path) ;

	free( pic) ;

	return  1 ;
}

static void
delete_inline_picture(
	x_inline_picture_t *  pic	/* pic->pixmap mustn't be NULL. */
	)
{
	/* pic->disp can be NULL by x_picture_display_closed(). */
	if( pic->disp)
	{
		x_delete_image( pic->disp->display , pic->pixmap) ;
		x_delete_mask( pic->disp->display , pic->mask) ;

		free( pic->file_path) ;
		pic->disp = NULL ;
	}

	/* pixmap == None means that the inline picture is empty. */
	pic->pixmap = None ;
}

static void
pty_closed(
	ml_term_t *  term
	)
{
	u_int  count ;

	for( count = 0 ; count < num_of_inline_pics ; count++)
	{
		if( inline_pics[count].term == term &&
		    PIXMAP_IS_ACTIVE(inline_pics[count]))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " delete inline picture %d (%d)\n" ,
				count , num_of_inline_pics) ;
		#endif

			delete_inline_picture( inline_pics + count) ;
		}
	}

	/* XXX The memory of intern_pics is not freed. */
}

static int
cleanup_inline_pictures(
	ml_term_t *  term
	)
{
#define THRESHOLD  48
	static int  need_cleanup ;
	int  count ;
	int  empty_idx ;
	u_int8_t *  flags ;

	/*
	 * Don't cleanup unused inline pictures until the number of cached inline
	 * pictures is THRESHOLD or more(num_of_inline_pics >= THRESHOLD and
	 * need_cleanup is true).
	 */
	if( num_of_inline_pics < THRESHOLD || ! ( flags = alloca( num_of_inline_pics)))
	{
		if( num_of_inline_pics == 0)
		{
			/* XXX */
			ml_term_pty_closed_event = pty_closed ;
		}

		return  -1 ;
	}

	if( ! need_cleanup)
	{
		memset( flags , 1 , num_of_inline_pics) ;
	}
	else
	{
		int  beg ;
		int  end ;
		int  row ;
		ml_line_t *  line ;
		int  restore_alt_edit ;

		memset( flags , 0 , num_of_inline_pics) ;

		beg = - ml_term_get_num_of_logged_lines( term) ;
		end = ml_term_get_rows( term) ;
		restore_alt_edit = 0 ;

	check_pictures:
		for( row = beg ; row <= end ; row++)
		{
			if( ( line = ml_term_get_line( term , row)))
			{
				for( count = 0 ; count < line->num_of_filled_chars ; count++)
				{
					ml_char_t *  comb ;
					u_int  size ;

					if( ( comb = ml_get_combining_chars( line->chars + count ,
								&size)) &&
					    ml_char_cs( comb) == PICTURE_CHARSET)
					{
						flags[INLINEPIC_ID(ml_char_code( comb))] = 1 ;
					}
				}
			}
		}

		/* XXX FIXME */
		if( term->screen->edit == &term->screen->alt_edit)
		{
			restore_alt_edit = 1 ;
			term->screen->edit = &term->screen->normal_edit ;
			beg = 0 ;

			goto  check_pictures ;
		}
		else if( restore_alt_edit)
		{
			term->screen->edit = &term->screen->alt_edit ;
		}
	}

	empty_idx = -1 ;

	for( count = num_of_inline_pics - 1 ; count >= 0 ; count--)
	{
		if( inline_pics[count].pixmap == None)
		{
			/* do nothing */
		}
		else if( inline_pics[count].pixmap != DUMMY_PIXMAP &&
		         ! flags[count] && inline_pics[count].term == term)
		{
			/*
			 * Don't cleanup inline pictures refered twice or more times
			 * until num_of_inline_pics reaches 32 or more.
			 */
			if( inline_pics[count].ref_count >= 2 &&
			    num_of_inline_pics < THRESHOLD + 8)
			{
				inline_pics[count].ref_count /= 2 ;

				continue ;
			}
			else
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG
					" delete inline picture %s %d (%d) \n" ,
					inline_pics[count].file_path ,
					count , num_of_inline_pics) ;
			#endif

				delete_inline_picture( inline_pics + count) ;

				if( count == num_of_inline_pics - 1)
				{
					num_of_inline_pics -- ;

					/*
					 * Don't return count because it is out
					 * of num_of_inline_pics.
					 */
					continue ;
				}
			}
		}
		else
		{
			continue ;
		}

		if( empty_idx == -1)
		{
			if( ! need_cleanup)
			{
				return  count ;
			}
			else
			{
				empty_idx = count ;

				/* Continue cleaning up. */
			}
		}
	}

	if( empty_idx == -1 && num_of_inline_pics >= THRESHOLD)
	{
		/*
		 * There is no empty entry. (The number of cached inline pictures
		 * is THRESHOLD or more.)
		 */
		need_cleanup = 1 ;
	}
	else
	{
		need_cleanup = 0 ;
	}

	return  empty_idx ;
}

static int
load_file(
	void *  p
	)
{
	int  idx ;
	Pixmap  pixmap ;
	PixmapMask  mask ;
	u_int  width ;
	u_int  height ;

	idx = ((inline_pic_args_t*)p)->idx ;
	width = inline_pics[idx].width ;
	height = inline_pics[idx].height ;

	if( x_imagelib_load_file( inline_pics[idx].disp , inline_pics[idx].file_path ,
		NULL , &pixmap , &mask , &width , &height))
	{
		/* XXX pthread_mutex_lock( &mutex) is necessary. */
		inline_pics[idx].pixmap = pixmap ;
		inline_pics[idx].mask = mask ;
		inline_pics[idx].width = width ;
		inline_pics[idx].height = height ;

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " new inline picture (%s %d %p %p) is created.\n" ,
			inline_pics[idx].file_path , idx ,
			inline_pics[idx].pixmap , inline_pics[idx].mask) ;
	#endif

		return  1 ;
	}
	else
	{
		free( inline_pics[idx].file_path) ;
		inline_pics[idx].file_path = NULL ;
		inline_pics[idx].pixmap = None ;	/* Mark as empty */

		return  0 ;
	}
}

#if  defined(USE_WIN32API) || defined(HAVE_PTHREAD)

#ifdef  USE_WIN32API
static u_int __stdcall
#else
static void *
#endif
load_file_async(
	void *  p
  	)
{
#ifdef  HAVE_PTHREAD
	pthread_detach( pthread_self()) ;
#endif

	load_file( p) ;

#ifdef  HAVE_WINDOWS_H
	if( ((inline_pic_args_t*)p)->ev)
	{
		SetEvent( ((inline_pic_args_t*)p)->ev) ;
		CloseHandle( ((inline_pic_args_t*)p)->ev) ;
	}
#else
	if( ((inline_pic_args_t*)p)->ev != -1)
	{
		close( ((inline_pic_args_t*)p)->ev) ;
	}
#endif

	free( p) ;

	return  0 ;
}

#endif


/* --- global functions --- */

int
x_picture_display_opened(
	Display *  display
	)
{
	if( ! x_imagelib_display_opened( display))
	{
		return  0 ;
	}

	return  1 ;
}

int
x_picture_display_closed(
	Display *  display
	)
{
	int  count ;

	if( num_of_icon_pics > 0)
	{
		for( count = num_of_icon_pics - 1 ; count >= 0 ; count--)
		{
			if( icon_pics[count]->disp->display == display)
			{
				delete_icon_picture( icon_pics[count]) ;
				icon_pics[count] = icon_pics[--num_of_icon_pics] ;
			}
		}

		if( num_of_icon_pics == 0)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " All cached icons were free'ed\n") ;
		#endif
		
			free( icon_pics) ;
			icon_pics = NULL ;
		}
	}

	for( count = 0 ; count < num_of_inline_pics ; count++)
	{
		if( inline_pics[count].disp &&
		    inline_pics[count].disp->display == display)
		{
			if( PIXMAP_IS_ACTIVE(inline_pics[count]))
			{
				x_delete_image( display , inline_pics[count].pixmap) ;
				x_delete_mask( display , inline_pics[count].mask) ;
			}

			/*
			 * Don't set x_inline_picture_t::pixmap = None here because
			 * this inline picture can still exist in ml_term_t.
			 */

			free( inline_pics[count].file_path) ;
			inline_pics[count].disp = NULL ;
		}
	}

	return  x_imagelib_display_closed( display) ;
}

/*
 * Judge whether pic_mods are equal or not.
 * \param a,b picture modifier
 * \return  1 when they are same. 0 when not.
 */
int
x_picture_modifiers_equal(
	x_picture_modifier_t *  a ,	/* Can be NULL (which means normal pic_mod) */
	x_picture_modifier_t *  b	/* Can be NULL (which means normal pic_mod) */
	)
{
	if( a == b)
	{
		/* If a==NULL and b==NULL, return 1 */
		return  1 ;
	}

	if( a == NULL)
	{
		a = b ;
		b = NULL ;
	}

	if( b == NULL)
	{
		/* Check if 'a' is normal or not. */

		if( (a->brightness == 100) &&
			(a->contrast == 100) &&
			(a->gamma == 100) &&
			(a->alpha == 0))
		{
			return  1 ;
		}
	}
	else
	{
		if( (a->brightness == b->brightness) &&
			(a->contrast == b->contrast) &&
			(a->gamma == b->gamma) &&
			(a->alpha == b->alpha) &&
			(a->blend_red == b->blend_red) &&
			(a->blend_green == b->blend_green) &&
			(a->blend_blue == b->blend_blue))
		{
			return  1 ;
		}
	}
	
	return  0 ;
}

x_picture_t *
x_acquire_bg_picture(
	x_window_t *  win ,
	x_picture_modifier_t *  mod ,
	char *  file_path		/* "root" means transparency. */
	)
{
	u_int  count ;
	x_picture_t **  p ;

	if( strcmp( file_path , "root") != 0) /* Transparent background is not cached. */
	{
		for( count = 0 ; count < num_of_pics ; count++)
		{
			if( strcmp( file_path , pics[count]->file_path) == 0 &&
			    win->disp->display == pics[count]->display &&
			    x_picture_modifiers_equal( mod , pics[count]->mod) &&
			    ACTUAL_WIDTH(win) == pics[count]->width &&
			    ACTUAL_HEIGHT(win) == pics[count]->height)
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Use cached picture(%s).\n" ,
					file_path) ;
			#endif
				pics[count]->ref_count ++ ;

				return  pics[count] ;
			}
		}
	}

	if( ( p = realloc( pics , ( num_of_pics + 1) * sizeof( *pics))) == NULL)
	{
		return  NULL ;
	}

	pics = p ;

	if( ! ( pics[num_of_pics] = create_bg_picture( win , mod , file_path)))
	{
		if( num_of_pics == 0 /* pics == NULL */)
		{
			free( pics) ;
			pics = NULL ;
		}

		return  NULL ;
	}

	return  pics[num_of_pics++] ;
}

int
x_release_picture(
	x_picture_t *  pic
	)
{
	u_int  count ;
	
	for( count = 0 ; count < num_of_pics ; count++)
	{
		if( pic == pics[count])
		{
			if( -- (pic->ref_count) == 0)
			{
				delete_picture( pic) ;
				
				if( --num_of_pics == 0)
				{
				#ifdef  DEBUG
					kik_debug_printf( KIK_DEBUG_TAG
						" All cached bg pictures were free'ed\n") ;
				#endif

					free( pics) ;
					pics = NULL ;
				}
				else
				{
					pics[count] = pics[num_of_pics] ;
				}
			}

			return  1 ;
		}
	}

	return  0 ;
}

x_icon_picture_t *
x_acquire_icon_picture(
	x_display_t *  disp ,
	char *  file_path	/* Don't specify NULL. */
	)
{
	u_int  count ;
	x_icon_picture_t **  p ;

	for( count = 0 ; count < num_of_icon_pics ; count++)
	{
		if( strcmp( file_path , icon_pics[count]->file_path) == 0 &&
			disp == icon_pics[count]->disp)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Use cached icon(%s).\n" , file_path) ;
		#endif
			icon_pics[count]->ref_count ++ ;
			
			return  icon_pics[count] ;
		}
	}

	if( ( p = realloc( icon_pics , ( num_of_icon_pics + 1) * sizeof( *icon_pics))) == NULL)
	{
		return  NULL ;
	}

	icon_pics = p ;

	if( ( icon_pics[num_of_icon_pics] = create_icon_picture( disp , file_path)) == NULL)
	{
		if( num_of_icon_pics == 0 /* icon_pics == NULL */)
		{
			free( icon_pics) ;
			icon_pics = NULL ;
		}
		
		return  NULL ;
	}

	return  icon_pics[num_of_icon_pics++] ;
}

int
x_release_icon_picture(
	x_icon_picture_t *  pic
	)
{
	u_int  count ;
	
	for( count = 0 ; count < num_of_icon_pics ; count++)
	{
		if( pic == icon_pics[count])
		{
			if( -- (pic->ref_count) == 0)
			{
				delete_icon_picture( pic) ;
				
				if( --num_of_icon_pics == 0)
				{
				#ifdef  DEBUG
					kik_debug_printf( KIK_DEBUG_TAG
						" All cached icons were free'ed\n") ;
				#endif

					free( icon_pics) ;
					icon_pics = NULL ;
				}
				else
				{
					icon_pics[count] = icon_pics[num_of_icon_pics] ;
				}
			}

			return  1 ;
		}
	}

	return  0 ;
}

int
x_load_inline_picture(
	x_display_t *  disp ,
	char *  file_path ,
	u_int *  width ,	/* can be 0 */
	u_int *  height ,	/* can be 0 */
	u_int  col_width ,
	u_int  line_height ,
	ml_term_t *  term
	)
{
	int  idx ;
	inline_pic_args_t *  args ;

	/* XXX Don't reuse ~/.mlterm/[pty name].six */
	if( ! strstr( file_path , "mlterm/"))
	{
		for( idx = 0 ; idx < num_of_inline_pics ; idx++)
		{
			if( inline_pics[idx].pixmap &&
			    disp == inline_pics[idx].disp &&
			    strcmp( file_path , inline_pics[idx].file_path) == 0 &&
			    term == inline_pics[idx].term &&
			    /* XXX */ (*width == 0 || *width == inline_pics[idx].width) &&
			    /* XXX */ (*height == 0 || *height == inline_pics[idx].height))
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Use cached picture(%s).\n" ,
					file_path) ;
			#endif

				inline_pics[idx].ref_count ++ ;

				if( inline_pics[idx].pixmap == DUMMY_PIXMAP)
				{
					return  -1 ;
				}
				else
				{
					goto  success ;
				}
			}
		}
	}

	if( ( idx = cleanup_inline_pictures( term)) == -1)
	{
		void *  p ;

		/* XXX pthread_mutex_lock( &mutex) is necessary. */
		if( ! ( p = realloc( inline_pics ,
				(num_of_inline_pics + 1) * sizeof(*inline_pics))))
		{
			return  -1 ;
		}

		inline_pics = p ;
		idx = num_of_inline_pics ++ ;
	}

	inline_pics[idx].file_path = strdup( file_path) ;
	inline_pics[idx].width = *width ;
	inline_pics[idx].height = *height ;
	inline_pics[idx].disp = disp ;
	inline_pics[idx].term = term ;
	inline_pics[idx].col_width = col_width ;
	inline_pics[idx].line_height = line_height ;
	inline_pics[idx].ref_count = 1 ;

	if( ! ( args = malloc( sizeof(inline_pic_args_t))))
	{
		inline_pics[idx].pixmap = None ;	/* mark as empty */

		return  -1 ;
	}

	args->idx = idx ;

#if  defined(HAVE_PTHREAD) || defined(USE_WIN32API)
	if( strstr( file_path , "://"))
	{
		/* Loading a remote file asynchronously. */

	#ifdef  HAVE_WINDOWS_H
		args->ev = CreateEvent( NULL , FALSE , FALSE , NULL) ;
	#else
		int  fds[2] ;

		if( pipe( fds) != 0)
		{
			fds[1] = args->ev = -1 ;
		}
		else
		{
			args->ev = fds[0] ;
		}
	#endif

		inline_pics[idx].pixmap = DUMMY_PIXMAP ;

	#ifdef  USE_WIN32API
		{
			HANDLE  thrd ;
			u_int  tid ;

			if( ( thrd = _beginthreadex( NULL , 0 ,
					load_file_async , args , 0 , &tid)))
			{
				CloseHandle( thrd) ;
			}
		}
	#else
		{
			pthread_t  thrd ;

			pthread_create( &thrd , NULL , load_file_async , args) ;
		}
	#endif

	#ifdef  HAVE_WINDOWS_H
		if( WaitForSingleObject( args->ev , 750) != WAIT_TIMEOUT &&
		    PIXMAP_IS_ACTIVE(inline_pics[idx]))
		{
			goto  success ;
		}
	#else
		if( fds[1] != -1)
		{
			fd_set  read_fds ;
			struct timeval  tval ;
			int  ret ;

			tval.tv_usec = 750000 ;
			tval.tv_sec = 0 ;
			FD_ZERO( &read_fds) ;
			FD_SET( fds[1] , &read_fds) ;

			ret = select( fds[1] + 1 , &read_fds , NULL , NULL , &tval) ;
			close( fds[1]) ;

			if( ret != 0 && PIXMAP_IS_ACTIVE(inline_pics[idx]))
			{
				goto  success ;
			}
		}
	#endif
	}
	else
#endif
	{
		int  ret ;

		ret = load_file(args) ;
		free( args) ;

		if( ret)
		{
			goto  success ;
		}
	}

	return  -1 ;

success:
	*width = inline_pics[idx].width ;
	*height = inline_pics[idx].height ;

	return  idx ;
}

x_inline_picture_t *
x_get_inline_picture(
	int  idx
	)
{
	if( inline_pics && idx < num_of_inline_pics)
	{
		return  inline_pics + idx ;
	}
	else
	{
		return  NULL ;
	}
}

#endif	/* NO_IMAGE */
