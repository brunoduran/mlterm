/*
 *	$Id$
 */

#include <stdlib.h>	/* system */
#include <sys/stat.h>	/* fstat */
#include <kiklib/kik_util.h>	/* K_MIN */

#ifdef  BUILTIN_IMAGELIB

#ifdef  SIXEL_1BPP
#define  realloc_pixels  realloc_pixels_1bpp
#define  correct_height  correct_height_1bpp
#define  load_sixel_from_file  load_sixel_from_file_1bpp
#define  SIXEL_RGB(r,g,b)  ((9 * (r) + 30 * (g) + (b)) * 51 >= 5120 * 20 ? 1 : 0)
#define  CARD_HEAD_SIZE  0
#define  pixel_t  u_int8_t
#else	/* SIXEL_1BPP */
#define  SIXEL_RGB(r,g,b)  ((((r)*255/100) << 16) | (((g)*255/100) << 8) | ((b)*255/100))
#ifndef  CARD_HEAD_SIZE
#ifdef  GDK_PIXBUF_VERSION
#define  CARD_HEAD_SIZE  0
#else
#define  CARD_HEAD_SIZE  8
#endif
#endif	/* CARD_HEAD_SIZE */
#define  pixel_t  u_int32_t
#endif	/* SIXEL_1BPP */

#define  PIXEL_SIZE  sizeof(pixel_t)

/* for cygwin */
#ifndef  BINDIR
#define  BINDIR  "/bin"
#endif

/* for other platforms */
#ifndef  LIBEXECDIR
#define  LIBEXECDIR  "/usr/local/libexec"
#endif


/* --- static variables --- */

#if  (defined(__NetBSD__) || defined(__OpenBSD__)) && ! defined(SIXEL_1BPP)
static pixel_t  sixel_cmap[256] ;
static u_int  sixel_cmap_size ;
#endif


/* --- static functions --- */

#ifndef  __GET_PARAMS__
#define  __GET_PARAMS__
static size_t
get_params(
	int *  params ,
	size_t  max_params ,
	char **  p
	)
{
	size_t  count ;
	char *  start ;

	memset( params , 0 , sizeof(int) * max_params) ;

	for( start = *p , count = 0 ; count < max_params ; count++)
	{
		while( 1)
		{
			if( '0' <= **p && **p <= '9')
			{
				params[count] = params[count] * 10 + (**p - '0') ;
			}
			else if( **p == ';')
			{
				(*p)++ ;
				break ;
			}
			else
			{
				if( start == *p)
				{
					return  0 ;
				}
				else
				{
					(*p)-- ;

					return  count + 1 ;
				}
			}

			(*p)++ ;
		}
	}

	(*p)-- ;

	return  count ;
}
#endif	/* __GET_PARAMS__ */

static int
realloc_pixels(
	u_char **  pixels ,
	int  new_width ,
	int  new_height ,
	int  cur_width ,
	int  cur_height
	)
{
	u_char *  p ;
	int  y ;
	int  n_copy_rows ;
	size_t  new_line_len ;
	size_t  cur_line_len ;

	if( new_width == cur_width && new_height == cur_height)
	{
		return  1 ;
	}

	n_copy_rows = K_MIN(new_height,cur_height) ;
	new_line_len = new_width * PIXEL_SIZE ;
	cur_line_len = cur_width * PIXEL_SIZE ;

	if( new_width < cur_width)
	{
		if( new_height > cur_height)
		{
			/* Not supported */

		#ifdef  DEBUG
			kik_error_printf( KIK_DEBUG_TAG
				" Sixel width is shrinked (%d->%d) but"
				" height is lengthen (%d->%d)\n" ,
				cur_width , cur_height , new_width , new_height) ;
		#endif

			return  0 ;
		}
		else /* if( new_height < cur_height) */
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Sixel data: %d %d -> shrink %d %d\n" ,
				cur_width , cur_height , new_width , new_height) ;
		#endif

			for( y = 1 ; y < n_copy_rows ; y++)
			{
				memmove( *pixels + (y * new_line_len) ,
					*pixels + (y * cur_line_len) ,
					new_line_len) ;
			}

			return  1 ;
		}
	}
	else if( new_width == cur_width && new_height < cur_height)
	{
		/* do nothing */

		return  1 ;
	}

	if( new_width > (SSIZE_MAX - CARD_HEAD_SIZE) / PIXEL_SIZE / new_height)
	{
		/* integer overflow */
		return  0 ;
	}

	if( new_width == cur_width /* && new_height > cur_height */)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Sixel data: %d %d -> realloc %d %d\n" ,
			cur_width , cur_height , new_width , new_height) ;
	#endif

		/*
		 * Cast to u_char* is necessary because this function can be
		 * compiled by g++.
		 */
		if( ( p = (u_char*)realloc( *pixels - CARD_HEAD_SIZE ,
				CARD_HEAD_SIZE + new_line_len * new_height)))
		{
			p += CARD_HEAD_SIZE ;
		}
		else
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " realloc failed.\n.") ;
		#endif
			return  0 ;
		}

		memset( p + cur_line_len * cur_height , 0 ,
			new_width * (new_height - cur_height)) ;
	}
	else
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Sixel data: %d %d -> calloc %d %d\n" ,
			cur_width , cur_height , new_width , new_height) ;
	#endif

		/* Cast to u_char* is necessary because this function can be compiled by g++. */
		if( ( p = (u_char*)calloc( CARD_HEAD_SIZE + new_line_len * new_height , 1)))
		{
			p += CARD_HEAD_SIZE ;
		}
		else
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " calloc failed.\n.") ;
		#endif
			return  0 ;
		}

		for( y = 0 ; y < n_copy_rows ; y++)
		{
			memcpy( p + (y * new_line_len) ,
				(*pixels) + (y * cur_line_len) ,
				cur_line_len) ;
		}

		if( *pixels)
		{
			free( (*pixels) - CARD_HEAD_SIZE) ;
		}
	}

	*pixels = p ;

	return  1 ;
}

/*
 * Correct the height which is always multiple of 6, but this doesn't
 * necessarily work.
 */
static void
correct_height(
	pixel_t *  pixels ,
	int  width ,
	int *  height	/* multiple of 6 */
	)
{
	int  x ;
	int  y ;

	pixels += (width * (*height - 1)) ;

	for( y = 0 ; y < 5 ; y++)
	{
		for( x = 0 ; x < width ; x++)
		{
			if( pixels[x])
			{
				return ;
			}
		}

		(*height) -- ;
		pixels -= width ;
	}
}

/*
 * load_sixel_from_file() returns at least 1024*1024 pixels memory even if
 * the actual image size is less than it.
 * It is the caller that should shrink (realloc) it.
 */
static u_char *
load_sixel_from_file(
	const char *  path ,
	u_int *  width_ret ,
	u_int *  height_ret
	)
{
	FILE *  fp ;
	struct stat  st ;
	char *  file_data ;
	char *  p ;
	size_t  len ;
	u_char *  pixels ;
	int  params[5] ;
	size_t  n ;	/* number of params */
	int  init_width ;
	int  pix_x ;
	int  pix_y ;
	int  cur_width ;
	int  cur_height ;
	int  width ;
	int  height ;
	int  rep ;
	int  color ;
	int  asp_x ;
	/* VT340 Default Color Map */
	static pixel_t  default_color_tbl[] =
	{
		SIXEL_RGB(0,0,0) ,	/* BLACK */
		SIXEL_RGB(20,20,80) ,	/* BLUE */
		SIXEL_RGB(80,13,13) , /* RED */
		SIXEL_RGB(20,80,20) ,	/* GREEN */
		SIXEL_RGB(80,20,80) ,	/* MAGENTA */
		SIXEL_RGB(20,80,80) , /* CYAN */
		SIXEL_RGB(80,80,20) , /* YELLOW */
		SIXEL_RGB(53,53,53) ,	/* GRAY 50% */
		SIXEL_RGB(26,26,26) ,	/* GRAY 25% */
		SIXEL_RGB(33,33,60) , /* BLUE* */
		SIXEL_RGB(60,26,26) , /* RED* */
		SIXEL_RGB(33,60,33) , /* GREEN* */
		SIXEL_RGB(60,33,60) , /* MAGENTA* */
		SIXEL_RGB(33,60,60) , /* CYAN*/
		SIXEL_RGB(60,60,33) , /* YELLOW* */
		SIXEL_RGB(80,80,80)   /* GRAY 75% */
	} ;
#if  (defined(__NetBSD__) || defined(__OpenBSD__)) && ! defined(SIXEL_1BPP)
#define  color_tbl  sixel_cmap
	sixel_cmap_size = 16 ;
#else
	pixel_t  color_tbl[256] ;
#endif

	if( ! ( fp = fopen( path , "r")))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " failed to open %s\n." , path) ;
	#endif

		return  NULL ;
	}

	fstat( fileno( fp) , &st) ;

	/*
	 * - malloc() should be used here because alloca() could return insufficient memory.
	 * - Cast to char* is necessary because this function can be compiled by g++.
	 */
	if( ! ( p = file_data = (char*)malloc( st.st_size + 1)))
	{
		fclose(fp) ;

		return  NULL ;
	}

	len = fread( p , 1 , st.st_size , fp) ;
	fclose( fp) ;
	p[len] = '\0' ;

	pixels = NULL ;
	init_width = 0 ;
	cur_width = cur_height = 0 ;
	width = 1024 ;
	height = 1024 ;

	/*  Cast to u_char* is necessary because this function can be compiled by g++. */
	if( ! realloc_pixels( &pixels , width , height , 0 , 0))
	{
		free( file_data) ;

		return  NULL ;
	}

	memcpy( color_tbl , default_color_tbl , sizeof(default_color_tbl)) ;
	memset( color_tbl + 16 , 0 , sizeof(color_tbl) - sizeof(default_color_tbl)) ;

restart:
	while( 1)
	{
		if( *p == '\0')
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format\n.") ;
		#endif

			goto  end ;
		}
		else if( *p == '\x90')
		{
			break ;
		}
		else if( *p == '\x1b')
		{
			if( *(++p) == 'P')
			{
				break ;
			}
		}
		else
		{
			p ++ ;
		}
	}

	if( *(++p) != ';')
	{
		/* P1 */
		switch( *p)
		{
		case 'q':
			/* The default value. (2:1 is documented though) */
			asp_x = 1 ;
		#if  0
			asp_y = 1 ;
		#endif
			goto  body ;

	#if  0
		case '0':
		case '1':
		case '5':
		case '6':
			asp_x = 1 ;
			asp_y = 2 ;
			break;

		case '2':
			asp_x = 1 ;
			asp_y = 5 ;
			break;

		case '3':
		case '4':
			asp_x = 1 ;
			asp_y = 3 ;
			break;

		case '7':
		case '8':
		case '9':
			asp_x = 1 ;
			asp_y = 1 ;
			break;

		default:
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;
	#else
		case '\0':
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;

		default:
			asp_x = 1 ;	/* XXX */
	#endif
		}

		if( p[1] == ';')
		{
			p ++ ;
		}
	}
	else
	{
		/* P1 is omitted. */
		asp_x = 1 ;	/* V:H=2:1 */
	#if  0
		asp_y = 2 ;
	#endif
	}

	if( *(++p) != ';')
	{
		/* P2 */
		switch( *p)
		{
		case 'q':
			goto  body ;
	#if  0
		case '0':
		case '2':
			...
			break;

		default:
	#else
		case '\0':
	#endif
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;
		}

		if( p[1] == ';')
		{
			p ++ ;
		}
	}
#if  0
	else
	{
		/* P2 is omitted. */
	}
#endif

	/* Ignoring P3 */
	while( *(++p) != 'q')
	{
		if( *p == '\0')
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
		#endif
			goto  end ;
		}
	}

body:
	rep = asp_x ;
	pix_x = pix_y = 0 ;
	color = 0 ;

	while( *(++p) != '\0')
	{
		if( *p == '"')	/* " Pan ; Pad ; Ph ; Pv */
		{
			if( *(++p) == '\0')
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
			#endif
				break ;
			}

			if( ( n = get_params( params , 4 , &p)) == 1)
			{
				params[1] = 1 ;
				n = 2 ;
			}

			/* XXX ignored */
		#if  0
			switch(n)
			{
			case 4:
				height = params[3] ;
			case 3:
				width = params[2] ;
				/* XXX realloc_pixels() is necessary here. */
			case 2:
				/* V:H=params[0]:params[1] */
			#if  0
				asp_x = params[1] ;
				asp_y = params[0] ;
			#else
				rep /= asp_x ;
				if( ( asp_x = params[1] / params[0]) == 0)
				{
					asp_x = 1 ;	/* XXX */
				}
				rep *= asp_x ;
			#endif
			}

			if( asp_x <= 0)
			{
				asp_x = 1 ;
			}
		#endif
		}
		else if( *p == '!')	/* ! Pn Ch */
		{
			if( *(++p) == '\0')
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
			#endif

				break ;
			}

			if( get_params( params , 1 , &p) > 0)
			{
				if( ( rep = params[0]) < 1)
				{
					rep = 1 ;
				}

				rep *= asp_x ;
			}
		}
		else if( *p == '#')	/* # Pc ; Pu; Px; Py; Pz */
		{
			if( *(++p) == '\0')
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " Illegal format.\n.") ;
			#endif
				break ;
			}

			n = get_params( params , 5 , &p) ;

			if( n > 0)
			{
				if( ( color = params[0]) < 0)
				{
					color = 0 ;
				}
				else if( color > 255)
				{
					color = 255 ;
				}
			}

			if( n > 4)
			{
				if( params[1] == 1)
				{
					/* XXX HLS */
				}
				else if( params[1] == 2 )
				{
					/* RGB */
					color_tbl[color] = SIXEL_RGB(K_MIN(params[2],100),
					                             K_MIN(params[3],100),
								     K_MIN(params[4],100)) ;

				#if  (defined(__NetBSD__) || defined(__OpenBSD__)) && ! defined(SIXEL_1BPP)
					if( sixel_cmap_size >= color)
					{
						sixel_cmap_size = color + 1 ;
					}
				#endif

				#ifdef  __DEBUG
					kik_debug_printf( KIK_DEBUG_TAG
						" Set rgb %x for color %d.\n" ,
						color_tbl[color] , color) ;
				#endif
				}
			}
		}
		else if( *p == '$' || *p == '-')
		{
			pix_x = 0 ;
			rep = asp_x ;

			if( ! init_width && width > cur_width && cur_width > 0)
			{
				int  y ;

				for( y = (pix_y == 0 ? 1 : pix_y) ; y < pix_y + 6 ; y++)
				{
					memmove( pixels + y * cur_width * PIXEL_SIZE ,
						pixels + y * width * PIXEL_SIZE ,
						cur_width * PIXEL_SIZE) ;
				}

				width = cur_width ;
				init_width = 1 ;
			}

			if( *p == '-')
			{
				pix_y += 6 ;
			}
		}
		else if( *p >= '?' && *p <= '\x7E')
		{
			u_int  new_width ;
			u_int  new_height ;
			int  a ;
			int  b ;
			int  y ;

			if( ! realloc_pixels( &pixels ,
					(new_width = width < pix_x + rep ? width + 512 : width) ,
					(new_height = height < pix_y + 6 ? height + 512 : height) ,
					width , height))
			{
				break ;
			}

			width = new_width ;
			height = new_height ;

			b = *p - '?' ;
			a = 0x01 ;

			for( y = 0 ; y < 6 ; y++ )
			{
				if( (b & a) != 0)
				{
					int  x ;

					for( x = 0 ; x < rep ; x ++)
					{
					#if  defined(GDK_PIXBUF_VERSION)
						/* RGBA */
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE] =
							(color_tbl[color] >> 16) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE + 1] =
							(color_tbl[color] >> 8) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE + 2] =
							(color_tbl[color]) & 0xff ;
						pixels[((pix_y + y) * width + pix_x + x) *
							PIXEL_SIZE + 3] = 0xff ;
					#elif  defined(SIXEL_1BPP)
						/* 0x80 is opaque mark */
						((pixel_t*)pixels)[(pix_y + y) * width +
							pix_x + x] =
							0x80 | color_tbl[color] ;
					#else
						/* ARGB (cardinal) */
						((pixel_t*)pixels)[(pix_y + y) * width +
							pix_x + x] =
							0xff000000 | color_tbl[color] ;
					#endif
					}
				}

				a <<= 1 ;
			}

			pix_x += rep ;

			if( cur_width < pix_x)
			{
				cur_width = pix_x ;
			}

			if( cur_height < pix_y + 6)
			{
				cur_height = pix_y + 6 ;
			}

			rep = asp_x ;
		}
		else if( *p == '\x1b')
		{
			if( *(++p) == '\\')
			{
			#ifdef  DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " EOF.\n.") ;
			#endif

				if( *(p + 1) != '\0')
				{
					goto  restart ;
				}

				break ;
			}
			else if( *p == '\0')
			{
				break ;
			}
		}
		else if( *p == '\x9c')
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " EOF.\n.") ;
		#endif

			if( *(p + 1) != '\0')
			{
				goto  restart ;
			}

			break ;
		}
	}

end:
	free( file_data) ;

	if( cur_width == 0 ||
	    ! realloc_pixels( &pixels , cur_width , cur_height , width , height))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " Nothing is drawn.\n") ;
	#endif

		free( pixels - CARD_HEAD_SIZE) ;

		return  NULL ;
	}

	correct_height( (pixel_t*)pixels , cur_width , &cur_height) ;

	*width_ret = cur_width ;
	*height_ret = cur_height ;

	return  pixels ;
}

#ifdef  USE_WIN32API

static int
convert_regis_to_bmp(
	char *  path
	)
{
	size_t  len = strlen( path) ;
	char  cmd[17 + len * 2] ;
	STARTUPINFO  si ;
	PROCESS_INFORMATION  pi ;

	path[len - 4] = '\0' ;
	sprintf( cmd , "registobmp.exe %s.rgs %s.bmp" , path , path) ;
	strcat( path , ".bmp") ;

	ZeroMemory(&si,sizeof(STARTUPINFO)) ;
	si.cb = sizeof(STARTUPINFO) ;

	if( CreateProcess( NULL , cmd , NULL , NULL , FALSE , CREATE_NO_WINDOW ,
			NULL , NULL , &si , &pi))
	{
		DWORD  code ;

		WaitForSingleObject( pi.hProcess , INFINITE) ;
		GetExitCodeProcess( pi.hProcess , &code) ;
		CloseHandle( pi.hProcess) ;
		CloseHandle( pi.hThread) ;

		if( code == 0)
		{
			return  1 ;
		}
	}

	return  0 ;
}

#else

#include  <sys/wait.h>

static int
convert_regis_to_bmp(
	char *  path
	)
{
	pid_t  pid ;
	int  status ;

	if( ( pid = fork()) == -1)
	{
		return  0 ;
	}

	if( pid == 0)
	{
		char *  new_path ;
		size_t  len ;
	#if  defined(__CYGWIN__) || defined(__MSYS__)
		/* To make registobmp work even if it (or SDL) doesn't depend on cygwin. */
		char *  file ;

		file = kik_basename( path) ;
		if( file && path < file)
		{
			*(file - 1) = '\0' ;
			chdir( path) ;
			path = file ;
		}
	#endif

		len = strlen( path) ;

		if( ( new_path = (char*)malloc( len + 1)))
		{
			char *  argv[4] ;

			strncpy( new_path , path , len - 4) ;
			strcpy( new_path + len - 4 , ".bmp") ;

		#if  defined(__CYGWIN__) || defined(__MSYS__)
			argv[0] = BINDIR "/registobmp" ;
		#else
			argv[0] = LIBEXECDIR "/mlterm/registobmp" ;
		#endif
			argv[1] = path ;
			argv[2] = new_path ;
			argv[3] = NULL ;

			execve( argv[0] , argv , NULL) ;
		}

		exit(1) ;
	}

	waitpid( pid , &status , 0) ;

	if( WEXITSTATUS(status) == 0)
	{
		strcpy( path + strlen(path) - 4 , ".bmp") ;

		return  1 ;
	}

	return  0 ;
}

#endif


#ifndef  SIXEL_1BPP
#ifdef  GDK_PIXBUF_VERSION

#include <kiklib/kik_str.h>	/* kik_str_alloca_dup */

static void
pixbuf_destroy_notify(
	guchar *  pixels ,
	gpointer  data
	)
{
	free( pixels) ;
}

static GdkPixbuf *
gdk_pixbuf_new_from_sixel(
	const char *  path
	)
{
	u_char *  pixels ;
	u_int  width ;
	u_int  height ;

	if( ! ( pixels = load_sixel_from_file( path , &width , &height)))
	{
		return  NULL ;
	}

	return  gdk_pixbuf_new_from_data( pixels , GDK_COLORSPACE_RGB , TRUE , 8 ,
			width , height , width * PIXEL_SIZE , pixbuf_destroy_notify , NULL) ;
}

#define  create_cardinals_from_sixel( path , width , height)  (NULL)

/* create an CARDINAL array for_NET_WM_ICON data */
static u_int32_t *
create_cardinals_from_pixbuf(
	GdkPixbuf *  pixbuf
	)
{
	u_int  width ;
	u_int  height ;
	u_int32_t *  cardinal ;
	int  rowstride ;
	u_char *  line ;
	u_char *  pixel ;
	u_int i , j ;

	width = gdk_pixbuf_get_width( pixbuf) ;
	height = gdk_pixbuf_get_height( pixbuf) ;

	if( width > ((SSIZE_MAX / sizeof(*cardinal)) - 2) / height ||	/* integer overflow */
	    ! ( cardinal = malloc( ( width * height + 2) * sizeof(*cardinal))))
	{
		return  NULL ;
	}

	rowstride = gdk_pixbuf_get_rowstride( pixbuf) ;
	line = gdk_pixbuf_get_pixels( pixbuf) ;

	/* format of the array is {width, height, ARGB[][]} */
	cardinal[0] = width ;
	cardinal[1] = height ;
	if( gdk_pixbuf_get_has_alpha( pixbuf))
	{
		for( i = 0 ; i < height ; i++)
		{
			pixel = line ;
			line += rowstride;
			for( j = 0 ; j < width ; j++)
			{
				/* RGBA to ARGB */
				cardinal[(i*width+j)+2] = ((((((u_int32_t)(pixel[3]) << 8)
								+ pixel[0]) << 8)
								+ pixel[1]) << 8) + pixel[2] ;
				pixel += 4 ;
			}
		}
	}
	else
	{
		for( i = 0 ; i < height ; i++)
		{
			pixel = line ;
			line += rowstride;
			for( j = 0 ; j < width ; j++)
			{
				/* all pixels are completely opaque (0xFF) */
				cardinal[(i*width+j)+2] = ((((((u_int32_t)(0x0000FF) <<8)
								+ pixel[0]) << 8)
								+ pixel[1]) << 8) + pixel[2] ;
				pixel += 3 ;
			}
		}
	}

	return  cardinal ;
}

static GdkPixbuf *
gdk_pixbuf_new_from(
	const char *  path
	)
{
	GdkPixbuf *  pixbuf ;

	if( ! strstr( path , ".six") || ! ( pixbuf = gdk_pixbuf_new_from_sixel( path)))
	{
	#if GDK_PIXBUF_MAJOR >= 2

		if( strstr( path , "://"))
		{
		#ifdef  __G_IO_H__
			/*
			 * gdk-pixbuf depends on gio. (__G_IO_H__ is defined if
			 * gdk-pixbuf-core.h includes gio.h)
			 */
			GFile *  file ;
			GInputStream *  in ;

			if( ( in = (GInputStream*)g_file_read(
					( file = g_vfs_get_file_for_uri(
							g_vfs_get_default() , path)) ,
					NULL , NULL)))
			{
				pixbuf = gdk_pixbuf_new_from_stream( in , NULL , NULL) ;
				g_object_unref( in) ;
			}
			else
		#endif
			{
				char *  cmd ;

				pixbuf = NULL ;

				if( ( cmd = alloca( 11 + strlen( path) + 1)))
				{
					FILE *  fp ;

					sprintf( cmd , "curl -L -k -s %s" , path) ;
					if( ( fp = popen( cmd , "r")))
					{
						GdkPixbufLoader *  loader ;
						guchar  buf[65536] ;
						size_t  len ;

						loader = gdk_pixbuf_loader_new() ;
						while( ( len = fread( buf , 1 , sizeof(buf) , fp))
						       > 0)
						{
							gdk_pixbuf_loader_write( loader ,
								buf , len , NULL) ;
						}
						gdk_pixbuf_loader_close( loader , NULL) ;

						pclose( fp) ;

						if( ( pixbuf = gdk_pixbuf_loader_get_pixbuf(
									loader)))
						{
							g_object_ref( pixbuf) ;
						}

						g_object_unref( loader) ;
					}
				}
			}

		#ifdef  __G_IO_H__
			g_object_unref( file) ;
		#endif
		}
		else
		{
			if( strstr( path , ".rgs"))
			{
				char *  new_path ;

				new_path = kik_str_alloca_dup( path) ;
				if( convert_regis_to_bmp( new_path))
				{
					path = new_path ;
				}
			}

			pixbuf = gdk_pixbuf_new_from_file( path , NULL) ;
		}

	#else	/* GDK_PIXBUF_MAJOR */

		pixbuf = gdk_pixbuf_new_from_file( path) ;

	#endif	/* GDK_PIXBUF_MAJOR */
	}

	return  pixbuf ;
}

#else	/* GDK_PIXBUF_VERSION */

#define  gdk_pixbuf_new_from_sixel(path)  (NULL)

#if  CARD_HEAD_SIZE == 8
static u_int32_t *
create_cardinals_from_sixel(
	const char *  path
	)
{
	u_int  width ;
	u_int  height ;
	u_int32_t *  cardinal ;

	if( ! ( cardinal = (u_int32_t*)load_sixel_from_file( path , &width , &height)))
	{
		return  NULL ;
	}

	cardinal -= 2 ;

	cardinal[0] = width ;
	cardinal[1] = height ;

	return  cardinal ;
}
#endif

#endif	/* GDK_PIXBUF_VERSION */
#endif  /* SIXEL_1BPP */

#undef  SIXEL_RGB
#undef  PIXEL_SIZE
#undef  CARD_HEAD_SIZE

#endif  /* BUILTIN_IMAGELIB */


#ifdef  USE_X11

/* seek the closest color */
static int
closest_color_index(
	XColor *  color_list ,
	int  len ,
	int  red ,
	int  green ,
	int  blue
	)
{
	int  closest = 0 ;
	int  i ;
	u_long  min = 0xffffff ;
	u_long  diff ;
	int  diff_r , diff_g , diff_b ;

	for( i = 0 ; i < len ; i++)
	{
		/* lazy color-space conversion*/
		diff_r = red - (color_list[i].red >> 8) ;
		diff_g = green - (color_list[i].green >> 8) ;
		diff_b = blue - (color_list[i].blue >> 8) ;
		diff = diff_r * diff_r *9 + diff_g * diff_g * 30 + diff_b * diff_b ;
		if ( diff < min)
		{
			min = diff ;
			closest = i ;
			/* no one may notice the difference (4[2^3/2]*4*9+4*4*30+4*4) */
			if ( diff < 640)
			{
				break ;
			}
		}
	}

	return  closest ;
}

/**Return position of the least significant bit
 *
 *\param val value to count
 *
 */
static int
lsb(
	u_int  val
	)
{
	int nth = 0 ;

	if( val == 0)
	{
		return  0 ;
	}

	while((val & 1) == 0)
	{
		val = val >> 1 ;
		nth ++ ;
	}

	return  nth ;
}

/**Return  position of the most significant bit
 *
 *\param val value to count
 *
 */
static int
msb(
	u_int val
	)
{
	int nth ;

	if( val == 0)
	{
		return  0 ;
	}

	nth = lsb( val) + 1 ;

	while(val & (1 << nth))
	{
		nth++ ;
	}

	return  nth ;
}

#endif	/* USE_X11 */


#undef  color_tbl
#undef  realloc_pixels
#undef  correct_height
#undef  load_sixel_from_file
#undef  SIXEL_RGB
#undef  CARD_HEAD_SIZE
#undef  pixel_t
