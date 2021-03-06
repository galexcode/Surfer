/* $Id$ */
static char const _copyright[] =
"Copyright © 2012-2013 Pierre Pronchery <khorben@defora.org>";
/* This file is part of DeforaOS Desktop Surfer */
static char const _license[] =
"This program is free software: you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation, version 3 of the License.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program.  If not, see <http://www.gnu.org/licenses/>.";



#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <locale.h>
#include <libintl.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <Desktop.h>
#include "ghtml.h"
#include <System.h>
#include "../config.h"
#define _(string) gettext(string)
#define N_(string) string

/* constants */
#ifndef PROGNAME
# define PROGNAME	"helper"
#endif
#ifndef PREFIX
# define PREFIX		"/usr/local"
#endif
#ifndef DATADIR
# define DATADIR	PREFIX "/share"
#endif
#ifndef MANHTMLDIR
# define MANHTMLDIR	DATADIR "/doc/html"
#endif
#ifndef LOCALEDIR
# define LOCALEDIR	DATADIR "/locale"
#endif


/* helper */
/* private */
/* types */
typedef struct _Surfer Helper;

struct _Surfer
{
	/* widgets */
	GtkIconTheme * icontheme;
	GtkWidget * window;
#ifndef EMBEDDED
	GtkWidget * menubar;
#endif
	GtkWidget * notebook;
	GtkWidget * manual;
	GtkWidget * view;
	GtkToolItem * tb_fullscreen;

	/* find */
	GtkWidget * fi_dialog;
	GtkWidget * fi_text;
	GtkWidget * fi_case;
	GtkWidget * fi_back;
	GtkWidget * fi_wrap;

	/* about */
	GtkWidget * ab_window;
};


/* prototypes */
static Helper * _helper_new(void);
void _helper_delete(Helper * helper);

static int _helper_open(Helper * helper, char const * url);
static int _helper_open_contents(Helper * helper, char const * package,
		char const * command);
static int _helper_open_dialog(Helper * helper);
static int _helper_open_man(Helper * helper, int section, char const * page);
static int _helper_open_devel(Helper * helper, char const * package);

static int _error(char const * message, int ret);
static int _usage(void);

/* callbacks */
static void _helper_on_close(gpointer data);
#ifdef EMBEDDED
static void _helper_on_find(gpointer data);
#endif
static gboolean _helper_on_closex(gpointer data);
#ifndef EMBEDDED
static void _helper_on_edit_copy(gpointer data);
static void _helper_on_edit_find(gpointer data);
static void _helper_on_edit_select_all(gpointer data);
static void _helper_on_file_close(gpointer data);
static void _helper_on_file_open(gpointer data);
#endif
static void _helper_on_fullscreen(gpointer data);
#ifndef EMBEDDED
static void _helper_on_help_about(gpointer data);
static void _helper_on_help_contents(gpointer data);
#endif
static void _helper_on_manual_row_activated(GtkWidget * widget,
		GtkTreePath * path, GtkTreeViewColumn * column, gpointer data);
#ifdef EMBEDDED
static void _helper_on_open(gpointer data);
#endif
#ifndef EMBEDDED
static void _helper_on_view_fullscreen(gpointer data);
#endif


/* constants */
#ifndef EMBEDDED
static char const * _authors[] =
{
	"Pierre Pronchery <khorben@defora.org>",
	NULL
};
#endif

#ifdef EMBEDDED
static const DesktopAccel _helper_accel[] =
{
	{ G_CALLBACK(_helper_on_close), GDK_CONTROL_MASK, GDK_KEY_W },
	{ G_CALLBACK(_helper_on_find), GDK_CONTROL_MASK, GDK_KEY_F },
	{ G_CALLBACK(_helper_on_fullscreen), 0, GDK_KEY_F11 },
	{ G_CALLBACK(_helper_on_open), GDK_CONTROL_MASK, GDK_KEY_O },
	{ NULL, 0, 0 }
};
#endif

#ifndef EMBEDDED
static const DesktopMenu _menu_file[] =
{
	{ N_("_Open..."), G_CALLBACK(_helper_on_file_open), GTK_STOCK_OPEN,
		GDK_CONTROL_MASK, GDK_KEY_O },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Close"), G_CALLBACK(_helper_on_file_close), GTK_STOCK_CLOSE,
		GDK_CONTROL_MASK, GDK_KEY_W },
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenu _menu_edit[] =
{
	{ N_("Cop_y"), G_CALLBACK(_helper_on_edit_copy), GTK_STOCK_COPY,
		GDK_CONTROL_MASK, GDK_KEY_C },
	{ "", NULL, NULL, 0, 0 },
	{ N_("Select _all"), G_CALLBACK(_helper_on_edit_select_all),
# if GTK_CHECK_VERSION(2, 10, 0)
		GTK_STOCK_SELECT_ALL,
# else
		NULL,
# endif
		GDK_CONTROL_MASK, GDK_KEY_A },
	{ "", NULL, NULL, 0, 0 },
	{ N_("_Find"), G_CALLBACK(_helper_on_edit_find), GTK_STOCK_FIND,
		GDK_CONTROL_MASK, GDK_KEY_F },
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenu _menu_view[] =
{
	{ N_("_Fullscreen"), G_CALLBACK(_helper_on_view_fullscreen),
# if GTK_CHECK_VERSION(2, 8, 0)
		GTK_STOCK_FULLSCREEN, 0, GDK_KEY_F11 },
# else
		NULL, 0, GDK_KEY_F11 },
# endif
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenu _menu_help[] =
{
	{ N_("_Contents"), G_CALLBACK(_helper_on_help_contents),
		"help-contents", 0, GDK_KEY_F1 },
	{ N_("_About"), G_CALLBACK(_helper_on_help_about),
# if GTK_CHECK_VERSION(2, 6, 0)
		GTK_STOCK_ABOUT, 0, 0 },
# else
		NULL, 0, 0 },
# endif
	{ NULL, NULL, NULL, 0, 0 }
};

static const DesktopMenubar _helper_menubar[] =
{
	{ N_("_File"), _menu_file },
	{ N_("_Edit"), _menu_edit },
	{ N_("_View"), _menu_view },
	{ N_("_Help"), _menu_help },
	{ NULL, NULL }
};
#endif


/* functions */
/* Helper */
/* helper_new */
static void _new_manual(Helper * helper, char const * manhtmldir);
static void _new_manual_package(Helper * helper, char const * manhtmldir,
		GtkTreeStore * store, char const * package);

static Helper * _helper_new(void)
{
	Helper * helper;
	GtkAccelGroup * group;
	GtkWidget * vbox;
	GtkWidget * widget;
#ifdef EMBEDDED
	GtkToolItem * toolitem;
#endif

	if((helper = object_new(sizeof(*helper))) == NULL)
		return NULL;
	/* widgets */
	helper->icontheme = gtk_icon_theme_get_default();
	/* window */
	group = gtk_accel_group_new();
	helper->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_add_accel_group(GTK_WINDOW(helper->window), group);
	gtk_window_set_default_size(GTK_WINDOW(helper->window), 800, 600);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_window_set_icon_name(GTK_WINDOW(helper->window), "help-browser");
#endif
	gtk_window_set_title(GTK_WINDOW(helper->window), _("Help browser"));
	g_signal_connect_swapped(helper->window, "delete-event", G_CALLBACK(
				_helper_on_closex), helper);
	vbox = gtk_vbox_new(FALSE, 0);
#ifndef EMBEDDED
	/* menubar */
	helper->menubar = desktop_menubar_create(_helper_menubar, helper,
			group);
	gtk_box_pack_start(GTK_BOX(vbox), helper->menubar, FALSE, TRUE, 0);
#else
	desktop_accel_create(_helper_accel, helper, group);
#endif
	/* toolbar */
	widget = gtk_toolbar_new();
#ifdef EMBEDDED
	toolitem = gtk_tool_button_new_from_stock(GTK_STOCK_OPEN);
	g_signal_connect_swapped(toolitem, "clicked", G_CALLBACK(
				_helper_on_open), helper);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), toolitem, -1);
#endif
#if GTK_CHECK_VERSION(2, 8, 0)
	helper->tb_fullscreen = gtk_toggle_tool_button_new_from_stock(
			GTK_STOCK_FULLSCREEN);
#else
	helper->tb_fullscreen = gtk_toggle_tool_button_new_from_stock(
			GTK_STOCK_ZOOM_FIT);
#endif
	g_signal_connect_swapped(helper->tb_fullscreen, "toggled", G_CALLBACK(
				_helper_on_fullscreen), helper);
	gtk_toolbar_insert(GTK_TOOLBAR(widget), helper->tb_fullscreen, -1);
	gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, TRUE, 0);
	/* view */
	widget = gtk_hpaned_new();
	gtk_paned_set_position(GTK_PANED(widget), 150);
	helper->notebook = gtk_notebook_new();
	_new_manual(helper, MANHTMLDIR);
	gtk_paned_add1(GTK_PANED(widget), helper->notebook);
	helper->view = ghtml_new(helper);
	ghtml_set_enable_javascript(helper->view, FALSE);
	gtk_paned_add2(GTK_PANED(widget), helper->view);
	gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(helper->window), vbox);
	gtk_widget_grab_focus(helper->view);
	gtk_widget_show_all(helper->window);
	helper->fi_dialog = NULL;
	helper->ab_window = NULL;
	return helper;
}

static void _new_manual(Helper * helper, char const * manhtmldir)
{
	GtkWidget * widget;
	GtkTreeStore * store;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * column;
	DIR * dir;
	struct dirent * de;

	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	store = gtk_tree_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	helper->manual = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(helper->manual), FALSE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(helper->manual), 1);
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
			"pixbuf", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(helper->manual), column);
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Package"),
			renderer, "text", 1, NULL);
	gtk_tree_view_column_set_sort_column_id(column, 1);
	gtk_tree_view_append_column(GTK_TREE_VIEW(helper->manual), column);
	gtk_tree_view_column_clicked(column);
	g_signal_connect(helper->manual, "row-activated", G_CALLBACK(
				_helper_on_manual_row_activated), helper);
	gtk_container_add(GTK_CONTAINER(widget), helper->manual);
	gtk_notebook_append_page(GTK_NOTEBOOK(helper->notebook), widget,
			gtk_label_new(_("Manual pages")));
	/* FIXME perform this while idle */
	if((dir = opendir(manhtmldir)) == NULL)
		return;
	while((de = readdir(dir)) != NULL)
		if(de->d_name[0] != '.')
			_new_manual_package(helper, manhtmldir, store,
					de->d_name);
	closedir(dir);
}

static void _new_manual_package(Helper * helper, char const * manhtmldir,
		GtkTreeStore * store, char const * package)
{
	const char ext[] = ".html";
	gchar * p;
	DIR * dir;
	struct dirent * de;
	size_t len;
	GtkTreeIter parent;
	GtkTreeIter iter;
	gint size = 16;
	GdkPixbuf * pixbuf;

	if((p = g_strdup_printf("%s/%s", manhtmldir, package)) == NULL)
		return;
	dir = opendir(p);
	g_free(p);
	if(dir == NULL)
		return;
	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &size, &size);
	pixbuf = gtk_icon_theme_load_icon(helper->icontheme, "folder", size, 0,
			NULL);
	/* FIXME sort (or use a sorted view) */
	gtk_tree_store_append(store, &parent, NULL);
	gtk_tree_store_set(store, &parent, 0, pixbuf, 1, package, -1);
	if(pixbuf != NULL)
	{
		g_object_unref(pixbuf);
		pixbuf = NULL;
	}
	while((de = readdir(dir)) != NULL)
	{
		if(de->d_name[0] == '.'
				|| (len = strlen(de->d_name)) < sizeof(ext)
				|| strcmp(&de->d_name[len - sizeof(ext) + 1],
					ext) != 0)
			continue;
		de->d_name[len - sizeof(ext) + 1] = '\0';
		if(pixbuf == NULL)
			pixbuf = gtk_icon_theme_load_icon(helper->icontheme,
					"help-contents", size, 0, NULL);
		gtk_tree_store_append(store, &iter, &parent);
		gtk_tree_store_set(store, &iter, 0, pixbuf, 1, de->d_name, -1);
	}
	closedir(dir);
	if(pixbuf != NULL)
		g_object_unref(pixbuf);
}


/* helper_delete */
void _helper_delete(Helper * helper)
{
	if(helper->ab_window != NULL)
		gtk_widget_destroy(helper->ab_window);
	if(helper->fi_dialog != NULL)
		gtk_widget_destroy(helper->fi_dialog);
	gtk_widget_destroy(helper->window);
	object_delete(helper);
}


/* useful */
/* helper_open */
static int _helper_open(Helper * helper, char const * url)
{
	if(url == NULL)
		return _helper_open_dialog(helper);
	ghtml_load_url(helper->view, url);
	return 0;
}


/* helper_open_contents */
static int _helper_open_contents(Helper * helper, char const * package,
		char const * command)
{
	char buf[256];

	if(package == NULL)
		return -1;
	if(command == NULL)
		command = "index";
	/* read a package documentation */
	snprintf(buf, sizeof(buf), "%s%s%s%s%s", "file://" MANHTMLDIR "/",
			package, "/", command, ".html");
	return _helper_open(helper, buf);
}


/* helper_open_devel */
static int _helper_open_devel(Helper * helper, char const * package)
{
	char buf[256];

	/* read a package API documentation */
	snprintf(buf, sizeof(buf), "%s%s%s", "file://" DATADIR "/gtk-doc/html/",
			package, "/index.html");
	return _helper_open(helper, buf);
}


/* helper_open_dialog */
static void _open_dialog_on_entry1_changed(GtkWidget * widget, gpointer data);

static int _helper_open_dialog(Helper * helper)
{
	int ret;
	GtkWidget * dialog;
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * entry1;
	GtkWidget * entry2;
	GtkTreeModel * model;
	char const * package = NULL;
	char const * command;

	dialog = gtk_dialog_new_with_buttons(_("Open page..."),
			GTK_WINDOW(helper->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	vbox = GTK_DIALOG(dialog)->vbox;
#endif
	gtk_box_set_spacing(GTK_BOX(vbox), 4);
	/* package */
	hbox = gtk_hbox_new(FALSE, 4);
	label = gtk_label_new(_("Package: "));
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 0);
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(helper->manual));
#if GTK_CHECK_VERSION(2, 24, 0)
	entry1 = gtk_combo_box_new_with_model_and_entry(model);
	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(entry1), 1);
#else
	entry1 = gtk_combo_box_entry_new_with_model(model, 1);
	gtk_combo_box_entry_set_text_column(GTK_COMBO_BOX_ENTRY(entry1), 1);
#endif
	entry2 = gtk_entry_new();
	g_signal_connect(entry1, "changed", G_CALLBACK(
				_open_dialog_on_entry1_changed), entry2);
	gtk_box_pack_start(GTK_BOX(hbox), entry1, TRUE, TRUE, 0);
	entry1 = gtk_bin_get_child(GTK_BIN(entry1));
	gtk_entry_set_activates_default(GTK_ENTRY(entry1), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	/* command */
	hbox = gtk_hbox_new(FALSE, 4);
	label = gtk_label_new(_("Command: "));
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 0);
	gtk_entry_set_activates_default(GTK_ENTRY(entry2), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), entry2, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show_all(vbox);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		package = gtk_entry_get_text(GTK_ENTRY(entry1));
		command = gtk_entry_get_text(GTK_ENTRY(entry2));
	}
	gtk_widget_hide(dialog);
	if(package == NULL || strlen(package) == 0)
		ret = -1;
	else
		ret = _helper_open_contents(helper, package, command);
	gtk_widget_destroy(dialog);
	return ret;
}

static void _open_dialog_on_entry1_changed(GtkWidget * widget, gpointer data)
{
	GtkTreeModel * model;
	GtkTreeIter parent;
	GtkTreeIter iter;
	GtkWidget * entry2 = data;
	gchar * command;

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
	if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter) != TRUE)
		return;
	gtk_tree_model_get(model, &iter, 1, &command, -1);
	if(gtk_tree_model_iter_parent(model, &parent, &iter) == TRUE)
	{
		gtk_entry_set_text(GTK_ENTRY(entry2), command);
		g_free(command);
		gtk_tree_model_get(model, &parent, 1, &command, -1);
	}
	else
		gtk_entry_set_text(GTK_ENTRY(entry2), "");
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(
					GTK_BIN(widget))), command);
	g_free(command);
}


/* helper_open_man */
static int _helper_open_man(Helper * helper, int section, char const * page)
{
	char const * prefix[] =
	{
		DATADIR, PREFIX, "/usr/local/share", "/usr/local", "/usr/share",
		NULL
	};
	char const ** p;
	char buf[256];

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%d, \"%s\")\n", __func__, section, page);
#endif
	if(section <= 0 || section >= 10)
		return -1;
	for(p = prefix; *p != NULL; p++)
	{
		snprintf(buf, sizeof(buf), "%s%s%d%s%s%s", *p, "/man/html",
				section, "/", page, ".html");
		if(access(buf, R_OK) == 0)
			break;
	}
	if(*p == NULL)
		return -1;
	/* read a manual page */
	snprintf(buf, sizeof(buf), "%s%s%s%d%s%s%s", "file://", *p, "/man/html",
			section, "/", page, ".html");
	return _helper_open(helper, buf);
}


/* callbacks */
/* helper_on_close */
static void _helper_on_close(gpointer data)
{
	Helper * helper = data;

	gtk_widget_hide(helper->window);
	gtk_main_quit();
}


/* helper_on_closex */
static gboolean _helper_on_closex(gpointer data)
{
	Helper * helper = data;

	_helper_on_close(helper);
	return TRUE;
}


#ifndef EMBEDDED
/* helper_on_edit_copy */
static void _helper_on_edit_copy(gpointer data)
{
	Helper * helper = data;

	surfer_copy(helper);
}


/* helper_on_edit_find */
static void _helper_on_edit_find(gpointer data)
{
	Helper * helper = data;

	surfer_find(helper, NULL);
}


/* helper_on_edit_select_all */
static void _helper_on_edit_select_all(gpointer data)
{
	Helper * helper = data;

	surfer_select_all(helper);
}


/* helper_on_file_close */
static void _helper_on_file_close(gpointer data)
{
	Helper * helper = data;

	gtk_widget_hide(helper->window);
	gtk_main_quit();
}


/* helper_on_file_open */
static void _helper_on_file_open(gpointer data)
{
	Helper * helper = data;

	_helper_open_dialog(helper);
}
#endif


#ifdef EMBEDDED
/* helper_on_find */
static void _helper_on_find(gpointer data)
{
	Helper * helper = data;

	surfer_find(helper, NULL);
}
#endif


/* helper_on_fullscreen */
static void _helper_on_fullscreen(gpointer data)
{
	Helper * helper = data;
	GdkWindow * window;
	gboolean fullscreen;

#if GTK_CHECK_VERSION(2, 14, 0)
	window = gtk_widget_get_window(helper->window);
#else
	window = helper->window->window;
#endif
	fullscreen = (gdk_window_get_state(window)
			& GDK_WINDOW_STATE_FULLSCREEN)
		== GDK_WINDOW_STATE_FULLSCREEN;
	surfer_set_fullscreen(helper, !fullscreen);
}


#ifndef EMBEDDED
/* helper_on_help_about */
static gboolean _about_on_closex(gpointer data);

static void _helper_on_help_about(gpointer data)
{
	Helper * helper = data;

	if(helper->ab_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(helper->ab_window));
		return;
	}
	helper->ab_window = desktop_about_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(helper->ab_window), GTK_WINDOW(
				helper->window));
	desktop_about_dialog_set_authors(helper->ab_window, _authors);
	desktop_about_dialog_set_comments(helper->ab_window,
			_("Online help for the DeforaOS desktop"));
	desktop_about_dialog_set_copyright(helper->ab_window, _copyright);
	desktop_about_dialog_set_logo_icon_name(helper->ab_window,
			"help-browser");
	desktop_about_dialog_set_license(helper->ab_window, _license);
	desktop_about_dialog_set_name(helper->ab_window, PACKAGE);
	desktop_about_dialog_set_version(helper->ab_window, VERSION);
	g_signal_connect_swapped(G_OBJECT(helper->ab_window), "delete-event",
			G_CALLBACK(_about_on_closex), helper);
	gtk_widget_show(helper->ab_window);
}

static gboolean _about_on_closex(gpointer data)
{
	Helper * helper = data;

	gtk_widget_hide(helper->ab_window);
	return TRUE;
}


/* helper_on_help_contents */
static void _helper_on_help_contents(gpointer data)
{
	desktop_help_contents(PACKAGE, PROGNAME);
}
#endif


/* helper_on_manual_row_activated */
static void _helper_on_manual_row_activated(GtkWidget * widget,
		GtkTreePath * path, GtkTreeViewColumn * column, gpointer data)
{
	Helper * helper = data;
	GtkTreeModel * model;
	GtkTreeIter iter;
	GtkTreeIter parent;
	gchar * package;
	gchar * command;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
	gtk_tree_model_get_iter(model, &iter, path);
	if(gtk_tree_model_iter_parent(model, &parent, &iter) == FALSE)
	{
		if(gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
			gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
		else
			gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path,
					FALSE);
		return;
	}
	gtk_tree_model_get(model, &parent, 1, &package, -1);
	gtk_tree_model_get(model, &iter, 1, &command, -1);
	_helper_open_contents(helper, package, command);
	g_free(package);
	g_free(command);
}


#ifdef EMBEDDED
/* helper_on_open */
static void _helper_on_open(gpointer data)
{
	Helper * helper = data;

	_helper_open_dialog(helper);
}
#endif


#ifndef EMBEDDED
/* helper_on_view_fullscreen */
static void _helper_on_view_fullscreen(gpointer data)
{
	Helper * helper = data;

	_helper_on_fullscreen(helper);
}
#endif


/* error */
static int _error(char const * message, int ret)
{
	fputs(PROGNAME ": ", stderr);
	perror(message);
	return ret;
}


/* usage */
static int _usage(void)
{
	fprintf(stderr, _("Usage: %s [-c][-p package] command\n"
"       helper -d package\n"
"       helper -s section page\n"
"  -s	Section of the manual page to read from\n"), PROGNAME);
	return 1;
}


/* public */
/* surfer */
/* essential */
/* surfer_new */
Surfer * surfer_new(char const * url)
{
	return NULL;
}


/* surfer_delete */
void surfer_delete(Surfer * surfer)
{
}


/* accessors */
/* surfer_get_view */
GtkWidget * surfer_get_view(Surfer * surfer)
{
	/* FIXME remove from the API? */
	return surfer->view;
}


/* surfer_set_favicon */
void surfer_set_favicon(Surfer * surfer, GdkPixbuf * pixbuf)
{
	/* FIXME implement */
}


/* surfer_set_fullscreen */
void surfer_set_fullscreen(Surfer * surfer, gboolean fullscreen)
{
	Helper * helper = surfer;

	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(
				helper->tb_fullscreen), fullscreen);
	if(fullscreen)
	{
#ifndef EMBEDDED
		gtk_widget_hide(helper->menubar);
#endif
		gtk_window_fullscreen(GTK_WINDOW(helper->window));
	}
	else
	{
#ifndef EMBEDDED
		gtk_widget_show(helper->menubar);
#endif
		gtk_window_unfullscreen(GTK_WINDOW(helper->window));
	}
}


/* surfer_set_location */
void surfer_set_location(Surfer * surfer, char const * location)
{
	/* FIXME implement? */
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, location);
#endif
}


/* surfer_set_progress */
void surfer_set_progress(Surfer * surfer, gdouble fraction)
{
	/* FIXME implement? */
}


/* surfer_set_security */
void surfer_set_security(Surfer * surfer, SurferSecurity security)
{
	/* FIXME implement? */
}


/* surfer_set_status */
void surfer_set_status(Surfer * surfer, char const * status)
{
	/* FIXME really implement? */
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, status);
#endif
}


/* surfer_set_title */
void surfer_set_title(Surfer * surfer, char const * title)
{
	Helper * helper = surfer;
	char buf[256];

	snprintf(buf, sizeof(buf), "%s%s%s", _("Help browser"),
			(title != NULL) ? " - " : "",
			(title != NULL) ? title : "");
	gtk_window_set_title(GTK_WINDOW(helper->window), buf);
}


/* useful */
/* surfer_confirm */
int surfer_confirm(Surfer * surfer, char const * message, gboolean * confirmed)
{
	Helper * helper = surfer;
	int ret = 0;
	GtkWidget * dialog;
	int res;

	dialog = gtk_message_dialog_new((helper != NULL)
			? GTK_WINDOW(helper->window) : NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Question"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			"%s", message);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
	res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if(res == GTK_RESPONSE_YES)
		*confirmed = TRUE;
	else if(res == GTK_RESPONSE_NO)
		*confirmed = FALSE;
	else
		ret = 1;
	return ret;
}


/* surfer_console_message */
void surfer_console_message(Surfer * surfer, char const * message,
		char const * source, long line)
{
	/* FIXME really implement */
	fprintf(stderr, "%s: %s:%ld: %s\n", PROGNAME, source, line, message);
}


/* surfer_copy */
void surfer_copy(Surfer * surfer)
{
	/* FIXME really implement */
	ghtml_copy(surfer->view);
}


/* surfer_download */
int surfer_download(Surfer * surfer, char const * url, char const * suggested)
{
	/* FIXME really implement */
	return 0;
}


/* surfer_error */
int surfer_error(Surfer * surfer, char const * message, int ret)
{
	Helper * helper = surfer;
	GtkWidget * dialog;

	dialog = gtk_message_dialog_new((helper != NULL)
			? GTK_WINDOW(helper->window) : NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Error"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			"%s", (message != NULL) ? message : _("Unknown error"));
	gtk_window_set_title(GTK_WINDOW(dialog), _("Error"));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return ret;
}


/* surfer_find */
#include "../src/common/find.c"


/* surfer_go_back */
gboolean surfer_go_back(Surfer * surfer)
{
	return ghtml_go_back(surfer->view);
}


/* surfer_go_forward */
gboolean surfer_go_forward(Surfer * surfer)
{
	return ghtml_go_forward(surfer->view);
}


/* surfer_open */
void surfer_open(Surfer * surfer, char const * url)
{
}


/* surfer_open_tab */
void surfer_open_tab(Surfer * surfer, char const * url)
{
	surfer_open(surfer, url);
}


/* surfer_prompt */
int surfer_prompt(Surfer * surfer, char const * message,
		char const * default_value, char ** value)
{
	Helper * helper = surfer;
	int ret = 0;
	GtkWidget * dialog;
	GtkWidget * vbox;
	GtkWidget * entry;
	int res;

	dialog = gtk_message_dialog_new((helper != NULL)
			? GTK_WINDOW(helper->window) : NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", _("Question"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			"%s", message);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
#if GTK_CHECK_VERSION(2, 14, 0)
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
	vbox = GTK_DIALOG(dialog)->vbox;
#endif
	entry = gtk_entry_new();
	if(default_value != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), default_value);
	gtk_widget_show(entry);
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, TRUE, 0);
	if((res = gtk_dialog_run(GTK_DIALOG(dialog))) == GTK_RESPONSE_OK)
		*value = strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	if(res != GTK_RESPONSE_OK || value == NULL)
		ret = 1;
	gtk_widget_destroy(dialog);
	return ret;
}


/* surfer_print */
void surfer_print(Surfer * surfer)
{
	ghtml_print(surfer->view);
}


/* surfer_refresh */
void surfer_refresh(Surfer * surfer)
{
	ghtml_refresh(surfer->view);
}


/* surfer_resize */
void surfer_resize(Surfer * surfer, gint width, gint height)
{
	Helper * helper = surfer;

	gtk_window_resize(GTK_WINDOW(helper->window), width, height);
}


/* surfer_save_dialog */
void surfer_save_dialog(Surfer * surfer)
{
}


/* surfer_select_all */
void surfer_select_all(Surfer * surfer)
{
	ghtml_select_all(surfer->view);
}


/* surfer_show_console */
void surfer_show_console(Surfer * surfer, gboolean show)
{
}


/* surfer_show_location */
void surfer_show_location(Surfer * surfer, gboolean show)
{
}


/* surfer_show_menubar */
void surfer_show_menubar(Surfer * surfer, gboolean show)
{
}


/* surfer_show_statusbar */
void surfer_show_statusbar(Surfer * surfer, gboolean show)
{
}


/* surfer_show_toolbar */
void surfer_show_toolbar(Surfer * surfer, gboolean show)
{
}


/* surfer_show_window */
void surfer_show_window(Surfer * surfer, gboolean show)
{
}


/* surfer_view_source */
void surfer_view_source(Surfer * surfer)
{
}


/* surfer_warning */
void surfer_warning(Surfer * surfer, char const * message)
{
	fprintf(stderr, "%s: %s\n", PROGNAME, message);
}


/* helper */
/* main */
int main(int argc, char * argv[])
{
	int o;
	int devel = 0;
	char const * package = NULL;
	int section = -1;
	char * p;
	Helper * helper;

	if(setlocale(LC_ALL, "") == NULL)
		_error("setlocale", 1);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "cdp:s:")) != -1)
		switch(o)
		{
			case 'c':
				section = -1;
				devel = 0;
				break;
			case 'd':
				section = -1;
				devel = 1;
				break;
			case 'p':
				package = optarg;
				break;
			case 's':
				section = strtol(optarg, &p, 10);
				if(optarg[0] == '\0' || *p != '\0'
						|| section < 0 || section > 9)
					return _usage();
				break;
			default:
				return _usage();
		}
	if(optind != argc && (optind + 1) != argc)
		return _usage();
	if((helper = _helper_new()) == NULL)
		return 2;
	if(section > 0)
		/* XXX check for errors */
		_helper_open_man(helper, section, argv[optind]);
	else if(argv[optind] != NULL && devel != 0)
		_helper_open_devel(helper, argv[optind]);
	else if(argv[optind] != NULL)
		_helper_open_contents(helper, (package != NULL) ? package
				: argv[optind], argv[optind]);
	else
		_helper_open_dialog(helper);
	gtk_main();
	_helper_delete(helper);
	return 0;
}
