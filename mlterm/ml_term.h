/*
 *	$Id$
 */

#ifndef  __ML_TERM_H__
#define  __ML_TERM_H__


#include  "ml_pty.h"
#include  "ml_vt100_parser.h"
#include  "ml_screen.h"
#include  "ml_config_menu.h"


typedef enum  ml_special_visual
{
	VIS_ISCII = 0x1 ,
	VIS_DYNAMIC_COMB = 0x2 ,
	VIS_BIDI = 0x4 ,
	VIS_VERT = 0x8 ,
	
} ml_special_visual_t ;

typedef struct ml_pty_event_listener
{
	void *  self ;
	void  (*closed)( void *) ;
	
} ml_pty_event_listener_t ;

typedef struct ml_term
{
	ml_pty_t *  pty ;
	ml_pty_event_listener_t *  pty_listener ;
	
	ml_vt100_parser_t *  parser ;
	ml_screen_t *  screen ;
	ml_config_menu_t  config_menu ;

} ml_term_t ;


ml_term_t *  ml_term_new( u_int  cols , u_int  rows ,
	u_int  tab_size , u_int  log_size , ml_char_encoding_t  encoding ,
	int  not_use_unicode_font , int  only_use_unicode_font , int  col_size_a ,
	int  use_char_combining , int  use_multi_col_char , int  use_bce , ml_bs_mode_t  bs_mode) ;

int  ml_term_delete( ml_term_t *  term) ;

int  ml_term_open_pty( ml_term_t *  term , char *  cmd_path , char **  argv ,
	char **  env , char *  host) ;

int  ml_term_set_listener( ml_term_t *  term , ml_xterm_event_listener_t *  xterm_listener ,
	ml_config_event_listener_t *  config_listener , ml_screen_event_listener_t *  screen_listener ,
	ml_pty_event_listener_t *  pty_listner) ;

int  ml_term_parse_vt100_sequence( ml_term_t *  term) ;

int  ml_term_change_encoding( ml_term_t *  term , ml_char_encoding_t  encoding) ;

ml_char_encoding_t  ml_term_get_encoding( ml_term_t *  term) ;

size_t  ml_term_convert_to( ml_term_t *  term , u_char *  dst , size_t  len , mkf_parser_t *  parser) ;

int  ml_term_init_encoding_parser( ml_term_t *  term) ;

int  ml_term_init_encoding_conv( ml_term_t *  term) ;

int  ml_term_get_pty_fd( ml_term_t *  term) ;

pid_t  ml_term_get_child_pid( ml_term_t *  term) ;

size_t  ml_term_write( ml_term_t *  term , u_char *  buf , size_t  len , int  to_cfg) ;

int  ml_term_flush( ml_term_t *  term) ;

int  ml_term_resize( ml_term_t *  term , u_int  cols , u_int  rows) ;

int  ml_term_cursor_col( ml_term_t *  term) ;

int  ml_term_cursor_char_index( ml_term_t *  term) ;

int  ml_term_cursor_row( ml_term_t *  term) ;

int  ml_term_cursor_row_in_screen( ml_term_t *  term) ;

u_int  ml_term_get_cols( ml_term_t *  term) ;

u_int  ml_term_get_rows( ml_term_t *  term) ;

u_int  ml_term_get_logical_cols( ml_term_t *  term) ;

u_int  ml_term_get_logical_rows( ml_term_t *  term) ;

u_int  ml_term_get_log_size( ml_term_t *  term) ;

int  ml_term_change_log_size( ml_term_t *  term , u_int  log_size) ;

u_int  ml_term_get_num_of_logged_lines( ml_term_t *  term) ;

int  ml_term_convert_scr_row_to_abs( ml_term_t *  term , int  row) ;

ml_line_t *  ml_term_get_line( ml_term_t *  term , int  row) ;

ml_line_t *  ml_term_get_line_in_screen( ml_term_t *  term , int  row) ;

ml_line_t *  ml_term_get_cursor_line( ml_term_t *  term) ;

int  ml_term_set_modified_all( ml_term_t *  term) ;

int  ml_term_enable_special_visual( ml_term_t *  term ,
	ml_special_visual_t  visual , int  adhoc_right_align ,
	ml_iscii_lang_t  iscii_lang , ml_vertical_mode_t  vertical_mode) ;

int  ml_term_disable_special_visual( ml_term_t *  term) ;

ml_bs_mode_t  ml_term_is_backscrolling( ml_term_t *  term) ;

int  ml_term_set_backscroll_mode( ml_term_t *  term , ml_bs_mode_t  mode) ;

int  ml_term_enter_backscroll_mode( ml_term_t *  term) ;

int  ml_term_exit_backscroll_mode( ml_term_t *  term) ;

int  ml_term_backscroll_to( ml_term_t *  term , int  row) ;

int  ml_term_backscroll_upward( ml_term_t *  term , u_int  size) ;

int  ml_term_backscroll_downward( ml_term_t *  term , u_int  size) ;

u_int  ml_term_get_tab_size( ml_term_t *  term) ;

int  ml_term_set_tab_size( ml_term_t *  term , u_int  size) ;

int  ml_term_reverse_color( ml_term_t *  term , int  beg_char_index , int  beg_row ,
	int  end_char_index , int  end_row) ;

int  ml_term_restore_color( ml_term_t *  term , int  beg_char_index , int  beg_row ,
	int  end_char_index , int  end_row) ;

u_int  ml_term_copy_region( ml_term_t *  term , ml_char_t *  chars ,
	u_int  num_of_chars , int  beg_char_index , int  beg_row , int  end_char_index , int  end_row) ;

u_int  ml_term_get_region_size( ml_term_t *  term , int  beg_char_index ,
	int  beg_row , int  end_char_index , int  end_row) ;

int  ml_term_get_line_region( ml_term_t *  term , int *  beg_row ,
	int *  end_char_index , int *  end_row , int  base_row) ;

int  ml_term_get_word_region( ml_term_t *  term , int *  beg_char_index ,
	int *  beg_row , int *  end_char_index , int *  end_row , int  base_char_index , int  base_row) ;

int  ml_term_set_char_combining_flag( ml_term_t *  term , int  flag) ;

int  ml_term_is_using_char_combining( ml_term_t *  term) ;

int  ml_term_set_multi_col_char_flag( ml_term_t *  term , int  flag) ;

int  ml_term_is_using_multi_col_char( ml_term_t *  term) ;

int  ml_term_start_config_menu( ml_term_t *  term , char *  cmd_path , int  x , int  y) ;


#endif
