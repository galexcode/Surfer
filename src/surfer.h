/* $Id$ */
/* Copyright (c) 2007 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Surfer */
/* Surfer is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * Surfer is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Surfer; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA  02111-1307  USA */



#ifndef SURFER_SURFER_H
# define SURFER_SURFER_H

# include <gtk/gtk.h>
# include <gtkmozembed.h>


/* Surfer */
/* constants */
# define SURFER_DEFAULT_MINIMUM_FONT_SIZE	8.0
# define SURFER_DEFAULT_FONT_SIZE		12.0
# define SURFER_DEFAULT_FIXED_FONT_SIZE		12.0
# define SURFER_DEFAULT_ENCODING		"ISO-8859-1"
# define SURFER_DEFAULT_SERIF_FONT		"Serif"
# define SURFER_DEFAULT_SANS_FONT		"Sans"
# define SURFER_DEFAULT_STANDARD_FONT		SURFER_DEFAULT_SANS_FONT
# define SURFER_DEFAULT_FIXED_FONT		"Monospace"
# define SURFER_DEFAULT_FANTASY_FONT		"Comic Sans MS"

# define SURFER_GTKMOZEMBED_COMPPATH		"/usr/pkg/lib/firefox"

/* types */
typedef struct _Surfer
{
	/* widgets */
	/* main window */
	GtkWidget * window;
	GtkWidget * menubar;
	GtkToolItem * tb_back;
	GtkToolItem * tb_forward;
	GtkToolItem * tb_stop;
	GtkToolItem * tb_refresh;
	GtkWidget * tb_path;
	GtkWidget * view;
	GtkWidget * statusbar;
	guint statusbar_id;
} Surfer;


/* functions */
Surfer * surfer_new(char const * url);
void surfer_delete(Surfer * surfer);


/* useful */
int surfer_error(Surfer * surfer, char const * message, int ret);

#endif /* !SURFER_SURFER_H */
