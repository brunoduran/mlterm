/*
 *	$Id$
 */

#include  <gtk/gtk.h>
#include  <glib.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_str.h>

#include  "mc_char_encoding.h"
#include  "mc_color.h"
#include  "mc_brightness.h"
#include  "mc_fade.h"
#include  "mc_tabsize.h"
#include  "mc_logsize.h"
#include  "mc_fontsize.h"
#include  "mc_line_space.h"
#include  "mc_screen_ratio.h"
#include  "mc_mod_meta.h"
#include  "mc_bel.h"
#include  "mc_vertical.h"
#include  "mc_sb.h"
#include  "mc_xim.h"
#include  "mc_check.h"
#include  "mc_iscii_lang.h"
#include  "mc_sb_view.h"
#include  "mc_wall_pic.h"
#include  "mc_io.h"


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static GtkWidget *  use_aa_check ;
static GtkWidget *  use_vcol_check ;
static GtkWidget *  use_comb_check ;
static GtkWidget *  use_dynamic_comb_check ;
static GtkWidget *  use_multi_col_char_check ;
static GtkWidget *  use_bidi_check ;
static GtkWidget *  copy_paste_via_ucs_check ;
static GtkWidget *  is_tp_check ;


/* --- static functions --- */

static gint
end_application(
	GtkWidget *  widget ,
	GdkEvent *  event ,
	gpointer  data
	)
{
	gtk_main_quit() ;

	return  FALSE ;
}

static gint
cancel_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	gtk_main_quit() ;

	return  FALSE ;
}

static int
update(void)
{
	mc_set_str_value( "encoding" , mc_get_char_encoding()) ;
	mc_set_str_value( "iscii_lang" , mc_get_iscii_lang()) ;
	mc_set_str_value( "fg_color" , mc_get_fg_color()) ;
	mc_set_str_value( "bg_color" , mc_get_bg_color()) ;
	mc_set_str_value( "sb_fg_color" , mc_get_sb_fg_color()) ;
	mc_set_str_value( "sb_bg_color" , mc_get_sb_bg_color()) ;
	mc_set_str_value( "tabsize" , mc_get_tabsize()) ;
	mc_set_str_value( "logsize" , mc_get_logsize()) ;
	mc_set_str_value( "fontsize" , mc_get_fontsize()) ;
	mc_set_str_value( "line_space" , mc_get_line_space()) ;
	mc_set_str_value( "screen_width_ratio" , mc_get_screen_width_ratio()) ;
	mc_set_str_value( "screen_height_ratio" , mc_get_screen_height_ratio()) ;
	mc_set_str_value( "mod_meta_mode" , mc_get_mod_meta_mode()) ;
	mc_set_str_value( "bel_mode" , mc_get_bel_mode()) ;
	mc_set_str_value( "vertical_mode" , mc_get_vertical_mode()) ;
	mc_set_str_value( "scrollbar_mode" , mc_get_sb_mode()) ;
	mc_set_str_value( "brightness" , mc_get_brightness()) ;
	mc_set_str_value( "fade_ratio" , mc_get_fade_ratio()) ;
	mc_set_str_value( "scrollbar_view_name" , mc_get_sb_view_name()) ;
	mc_set_str_value( "xim_name" , mc_get_xim_name()) ;
	mc_set_str_value( "xim_locale" , mc_get_xim_locale()) ;
	mc_set_str_value( "wall_picture" , mc_get_wall_pic()) ;
	mc_set_flag_value( "use_anti_alias" , GTK_TOGGLE_BUTTON(use_aa_check)->active) ;
	mc_set_flag_value( "use_variable_column_width" , GTK_TOGGLE_BUTTON(use_vcol_check)->active) ;
	mc_set_flag_value( "use_combining" , GTK_TOGGLE_BUTTON(use_comb_check)->active) ;
	mc_set_flag_value( "use_dynamic_comb" , GTK_TOGGLE_BUTTON(use_dynamic_comb_check)->active) ;
	mc_set_flag_value( "copy_paste_via_ucs" , GTK_TOGGLE_BUTTON(copy_paste_via_ucs_check)->active) ;
	mc_set_flag_value( "use_transbg" , GTK_TOGGLE_BUTTON(is_tp_check)->active) ;
	mc_set_flag_value( "use_multi_column_char" , GTK_TOGGLE_BUTTON(use_multi_col_char_check)->active) ;
	mc_set_flag_value( "use_bidi" , GTK_TOGGLE_BUTTON(use_bidi_check)->active) ;

	return  1 ;
}

static gint
apply_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	update() ;
	
	return  1 ;
}

static gint
ok_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	update() ;

	gtk_main_quit() ;
	
	return  1 ;
}

static gint
larger_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "fontsize" , "larger") ;
	
	return  1 ;
}

static gint
smaller_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "fontsize" , "smaller") ;
		
	return  1 ;
}

static gint
full_reset_clicked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	mc_set_str_value( "fullreset" , "") ;

	return  1 ;
}

static GtkWidget *
apply_cancel_button(
	void
	)
{
	GtkWidget * button ;
	GtkWidget * hbox ;

	hbox = gtk_hbox_new(FALSE , 5) ;
	gtk_widget_show(hbox) ;

	button = gtk_button_new_with_label("OK") ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(ok_clicked) , NULL) ;
	gtk_widget_show(button) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 5) ;

	button = gtk_button_new_with_label("Apply") ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(apply_clicked) , NULL) ;
	gtk_widget_show(button) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 5) ;

	button = gtk_button_new_with_label("Cancel") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(cancel_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 5) ;


	return hbox;
}

static GtkWidget *
font_large_small(void)
{
	GtkWidget * frame;
	GtkWidget * hbox;
	GtkWidget * button;

	frame = gtk_frame_new("Font size") ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	button = gtk_button_new_with_label("Larger") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(larger_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	button = gtk_button_new_with_label("Smaller") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(smaller_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;

	return frame;
}

static GtkWidget *
full_reset(void)
{
	GtkWidget *  frame ;
	GtkWidget *  hbox ;
	GtkWidget *  button ;

	frame = gtk_frame_new( "Full reset") ;
	gtk_widget_show(frame) ;

	hbox = gtk_hbox_new( FALSE , 5) ;
	gtk_container_set_border_width(GTK_CONTAINER(hbox) , 5) ;
	gtk_widget_show(hbox) ;
	gtk_container_add(GTK_CONTAINER(frame) , hbox) ;

	button = gtk_button_new_with_label( "Full reset") ;
	gtk_widget_show(button) ;
	gtk_signal_connect(GTK_OBJECT(button) , "clicked" , GTK_SIGNAL_FUNC(full_reset_clicked) , NULL) ;
	gtk_box_pack_start(GTK_BOX(hbox) , button , TRUE , TRUE , 0) ;
	
	return frame;
}

static int
show(
	int  x ,
	int  y
	)
{
	GtkWidget *  window ;
	GtkWidget *  vbox ;
	GtkWidget *  hbox ;
	GtkWidget *  notebook ;
	GtkWidget *  frame ;
	GtkWidget *  label ;
	GtkWidget *  config_widget ;
	GtkWidget *  separator ;
	
	window = gtk_window_new( GTK_WINDOW_TOPLEVEL) ;
	gtk_signal_connect( GTK_OBJECT(window) , "delete_event" ,
		GTK_SIGNAL_FUNC(end_application) , NULL) ;
	gtk_window_set_title( GTK_WINDOW(window) , "mlterm configuration") ;
	gtk_container_set_border_width( GTK_CONTAINER(window) , 0) ;
	gtk_widget_show( window) ;
	gdk_window_move( window->window , x , y) ;
	gtk_window_set_policy( GTK_WINDOW(window) , 0 , 0 , 0) ;

	vbox = gtk_vbox_new( FALSE , 10) ;
	gtk_widget_show( vbox) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_container_add( GTK_CONTAINER(window) , vbox) ;


	/* whole screen (except for the contents of notebook) */

	notebook = gtk_notebook_new() ;
	gtk_notebook_set_tab_pos( GTK_NOTEBOOK(notebook) , GTK_POS_TOP) ;
	gtk_widget_show( notebook) ;
	gtk_box_pack_start( GTK_BOX(vbox) , notebook , TRUE , TRUE , 0) ;

	separator = gtk_hseparator_new() ;
	gtk_widget_show( separator) ;
	gtk_box_pack_start( GTK_BOX(vbox) , separator , FALSE , FALSE , 0) ;

	hbox = apply_cancel_button();
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	
	hbox = gtk_hbox_new( FALSE , 0) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	frame = font_large_small();
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;
	frame = full_reset();
	gtk_box_pack_start( GTK_BOX(hbox) , frame , TRUE , TRUE , 5) ;


	/* contents of the "Encoding" tab */

	label = gtk_label_new( "Encoding") ;
	gtk_widget_show( label) ;

	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;

	if( ! ( config_widget = mc_char_encoding_config_widget_new( mc_get_str_value( "encoding"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_iscii_lang_config_widget_new( mc_get_str_value( "iscii_lang"))))
	{
		return  0 ;
	}
#ifndef USE_IND
	GTK_WIDGET_UNSET_FLAGS(config_widget, GTK_SENSITIVE);
#endif
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_xim_config_widget_new(
				mc_get_str_value( "xim") , mc_get_str_value( "locale"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	hbox = gtk_hbox_new( TRUE , 5) ;
	gtk_widget_show( hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;	

	if( ! ( use_bidi_check = mc_check_config_widget_new(
					"Bidi (UTF8 only)" , mc_get_flag_value( "use_bidi"))))
	{
		return  0 ;
	}
#ifndef USE_FRIBIDI
	GTK_WIDGET_UNSET_FLAGS(use_bidi_check, GTK_SENSITIVE);
#endif
	gtk_widget_show( use_bidi_check) ;
	gtk_box_pack_start( GTK_BOX(hbox) , use_bidi_check , TRUE , TRUE , 0) ;

	if( ! ( use_comb_check = mc_check_config_widget_new(
					"Combining" , mc_get_flag_value( "use_combining"))))
	{
		return  0 ;
	}
	gtk_widget_show( use_comb_check) ;
	gtk_box_pack_start( GTK_BOX(hbox) , use_comb_check , TRUE , TRUE , 0) ;

	if( ! ( copy_paste_via_ucs_check = mc_check_config_widget_new(
		"Process received strings via Unicode" , mc_get_flag_value( "copy_paste_via_ucs"))))
	{
		return  0 ;
	}
	gtk_widget_show( copy_paste_via_ucs_check) ;
	gtk_box_pack_start( GTK_BOX(vbox) , copy_paste_via_ucs_check , FALSE , FALSE , 0) ;


	/* contents of the "Appearance" tab */

	label = gtk_label_new( "Appearance") ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;

	if( ! ( config_widget = mc_fontsize_config_widget_new( mc_get_str_value( "fontsize"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_line_space_config_widget_new( mc_get_str_value( "line_space"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_screen_width_ratio_config_widget_new(
				mc_get_str_value( "screen_width_ratio"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_screen_height_ratio_config_widget_new(
				mc_get_str_value( "screen_height_ratio"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	hbox = gtk_hbox_new( FALSE , 0) ;
	
	if( ! ( use_aa_check = mc_check_config_widget_new(
				"Anti Alias" , mc_get_flag_value( "use_anti_alias"))))
	{
		return  0 ;
	}
	gtk_widget_show( use_aa_check) ;
	gtk_box_pack_start( GTK_BOX(hbox) , use_aa_check , TRUE , TRUE , 0) ;
	
	if( ! ( use_vcol_check = mc_check_config_widget_new(
				"Variable column width" , mc_get_flag_value( "use_variable_column_width"))))
	{
		return  0 ;
	}
	gtk_widget_show( use_vcol_check) ;
	gtk_box_pack_start( GTK_BOX(hbox) , use_vcol_check , TRUE , TRUE , 0) ;
	
#ifndef ANTI_ALIAS
	GTK_WIDGET_UNSET_FLAGS(hbox, GTK_SENSITIVE);
#endif
	gtk_widget_show(hbox) ;
	gtk_box_pack_start( GTK_BOX(vbox) , hbox , FALSE , FALSE , 0) ;
	
	/* contents of the "Color" tab */
	
	label = gtk_label_new( "Color") ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;

	if ( ! ( config_widget = mc_fg_color_config_widget_new( mc_get_str_value( "fg_color"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_bg_color_config_widget_new( mc_get_str_value( "bg_color"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_fade_config_widget_new( mc_get_str_value( "fade_ratio"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_wall_pic_config_widget_new( mc_get_str_value( "wall_picture"))))
	{
		return  0 ;
	}
#if !defined(USE_IMLIB) && !defined(USE_GDK_PIXBUF)
	GTK_WIDGET_UNSET_FLAGS(config_widget, GTK_SENSITIVE);
#endif
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_brightness_config_widget_new( mc_get_str_value( "brightness"))))
	{
		return  0 ;
	}
#if !defined(USE_IMLIB) && !defined(USE_GDK_PIXBUF)
	GTK_WIDGET_UNSET_FLAGS(config_widget, GTK_SENSITIVE);
#endif
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( is_tp_check = mc_check_config_widget_new(
				"Transparent background" , mc_get_flag_value( "use_transbg"))))
	{
		return  0 ;
	}
	gtk_widget_show( is_tp_check) ;
	gtk_box_pack_start( GTK_BOX(vbox) , is_tp_check , FALSE , FALSE , 0) ;


	/* contents of the "Scrollbar" tab */
	
	label = gtk_label_new( "Scrollbar") ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;

	if( ! ( config_widget = mc_sb_config_widget_new( mc_get_str_value( "scrollbar_mode"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_sb_view_config_widget_new( mc_get_str_value( "scrollbar_view_name"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_sb_fg_color_config_widget_new( mc_get_str_value( "sb_fg_color"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
	
	if( ! ( config_widget = mc_sb_bg_color_config_widget_new( mc_get_str_value( "sb_bg_color"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;
		
	
	/* contents of the "Others" tab */

	label = gtk_label_new( "Others") ;
	gtk_widget_show( label) ;
	vbox = gtk_vbox_new( FALSE , 3) ;
	gtk_container_set_border_width( GTK_CONTAINER(vbox) , 5) ;
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook) , vbox , label) ;
	gtk_widget_show( vbox) ;

	if( ! ( config_widget = mc_tabsize_config_widget_new( mc_get_str_value( "tabsize"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! (config_widget = mc_logsize_config_widget_new( mc_get_str_value( "logsize"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! (config_widget = mc_mod_meta_config_widget_new( mc_get_str_value( "mod_meta_mode"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_vertical_config_widget_new( mc_get_str_value( "vertical_mode"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( config_widget = mc_bel_config_widget_new( mc_get_str_value( "bel_mode"))))
	{
		return  0 ;
	}
	gtk_widget_show( config_widget) ;
	gtk_box_pack_start( GTK_BOX(vbox) , config_widget , FALSE , FALSE , 0) ;

	if( ! ( use_dynamic_comb_check = mc_check_config_widget_new(
		"Combining = 1 (or 0) logical column(s)" , mc_get_flag_value( "use_dynamic_comb"))))
	{
		return  0 ;
	}
	gtk_widget_show( use_dynamic_comb_check) ;
	gtk_box_pack_start( GTK_BOX(vbox) , use_dynamic_comb_check , FALSE , FALSE , 0) ;

	if( ! ( use_multi_col_char_check = mc_check_config_widget_new(
		"Fullwidth = 2 (or 1) logical column(s)" , mc_get_flag_value( "use_multi_column_char"))))
	{
		return  0 ;
	}
	gtk_widget_show( use_multi_col_char_check) ;
	gtk_box_pack_start( GTK_BOX(vbox) , use_multi_col_char_check , FALSE , FALSE , 0) ;

	gtk_main() ;

	return  1 ;
}


/* --- global functions --- */

int
main(
	int  argc ,
	char **  argv
	)
{
	int  x ;
	int  y ;
		
	gtk_init( &argc , &argv) ;

	if( argc != 3 ||
		! kik_str_to_int( &x , argv[1]) ||
		! kik_str_to_int( &y , argv[2]))
	{
		kik_msg_printf( "usage: mlconfig [x] [y]\n") ;
		
		return  0 ;
	}

	if( show( x , y) == 0)
	{
		kik_msg_printf( "Starting mlconfig failed.\n") ;
		
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}
