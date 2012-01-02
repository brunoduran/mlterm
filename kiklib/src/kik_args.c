/*
 *	$Id$
 */

#include  "kik_args.h"

#include  <string.h>	/* strchr */

#include  "kik_debug.h"


#if  0
#define  __DEBUG
#endif


/* --- global functions --- */

/*
 * supported option syntax.
 *
 *  -x(=xxx)
 *  --x(=xxx)
 *  -xxx(=xxx)
 *  --xxx(=xxx)
 *
 *  "--" cancels parsing options.
 *
 * !! NOTICE !!
 * after kik_parse_options() , argv points to an argument next to a successfully parsed one.
 */

int
kik_parse_options(
	char **  opt ,
	char **  opt_val ,
	int *  argc ,
	char ***  argv
	)
{
	char *  arg_p ;

	if( *argc == 0 || ( arg_p = (*argv)[0]) == NULL)
	{
		/* end of argv */
		
		return  0 ;
	}

	if( *arg_p != '-')
	{
		/* not option */

		return  0 ;
	}
	arg_p ++ ;

	if( *arg_p == '-')
	{
		arg_p ++ ;

		if( *arg_p == '\0')
		{
			/* "--" */

			return  0 ;
		}
	}

	*opt = arg_p ;

	if( ( arg_p = strchr( arg_p , '=')) == NULL)
	{
		*opt_val = NULL ;
	}
	else
	{
		*arg_p = '\0' ;
		*opt_val = arg_p + 1 ;
	}

	(*argv) ++ ;
	(*argc) -- ;

	return  1 ;
}

char **
_kik_arg_str_to_array(
	char **  argv ,
	int *  argc ,
	char *  args
	)
{
	char *  args_dup ;
	char *  p ;
	
	/*
	 * parsing options.
	 */

	*argc = 0 ;
	args_dup = args ;
	if( ( args = kik_str_alloca_dup( args)) == NULL)
	{
		return  NULL ;
	}
	
	p = args_dup ;

	while( *args)
	{
		int  quoted ;

		while( *args == ' ' /* || *args == '\t' */)
		{
			if( *(++args) == '\0')
			{
				goto  parse_end ;
			}
		}

		if( *args == '\"' || *args == '\'')
		{
			quoted = 1 ;
			args ++ ;
		}
		else
		{
			quoted = 0 ;
		}
		
		while( *args)
		{
			if( quoted)
			{
				if( *args == '\"' || *args == '\'')
				{
					args ++ ;
					
					break ;
				}
			}
			else
			{
				if( *args == ' ' /* || *args == '\t' */)
				{
					args ++ ;
					
					break ;
				}
			}
			
			if( *args == '\\' && ( *(args + 1) == '\"' || *(args + 1) == '\''))
			{
				*(p ++) = *(++ args) ;
			}
			else
			{
				*(p ++) = *args ;
			}

			args ++ ;
		}

		*(p ++) = '\0' ;
		argv[(*argc) ++] = args_dup ;
		args_dup = p ;
	}

parse_end:
	/* NULL terminator (POSIX exec family style) */
	argv[*argc] = NULL ;

	return  argv ;
}


#ifdef  __DEBUG
void
main(void)
{
	int  argc ;
	char **  argv ;
	char  args[] = "mlclient -l \"hoge fuga \\\" \" \' a b c \' \\\' \\\"" ;
	int  count ;

	argv = kik_arg_str_to_array( &argc , args) ;

	printf( "%d\n" , argc) ;
	for( count = 0 ; count < argc ; count++)
	{
		printf( "=>%s<=\n" , argv[count]) ;
	}
}
#endif
