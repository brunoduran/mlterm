/*
 *	$Id$
 */

#ifndef __MC_BGTYPE_H__
#define __MC_BGTYPE_H__

#include <gtk/gtk.h>

GtkWidget* mc_bgtype_config_widget_new(void);

void mc_update_bgtype(void);

int mc_is_color_bg(void);

#endif
