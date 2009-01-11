/* $Id$ */
/* Copyright (c) 2009 Pierre Pronchery <khorben@defora.org> */
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



#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <gtk/gtk.h>
#define GNET_EXPERIMENTAL
#include <gnet.h>


/* Download */
/* types */
typedef struct _Prefs
{
	char const * output;
	char const * user_agent;
} Prefs;

typedef struct _Download
{
	Prefs * prefs;
	char const * url;
	FILE * fp;

	struct timeval tv;

	GConnHttp * conn;
	guint64 content_length;
	guint64 data_received;

	/* widgets */
	GtkWidget * window;
	GtkWidget * status;
	GtkWidget * speed;
	GtkWidget * progress;

	guint timeout;
	int pulse;
} Download;


/* prototypes */
static int _download(Prefs * prefs, char const * url);
static int _download_error(Download * download, char const * message, int ret);
static void _download_refresh(Download * download);

/* callbacks */
static void _download_on_cancel(GtkWidget * widget, gpointer data);
static gboolean _download_on_closex(GtkWidget * widget, GdkEvent * event,
		gpointer data);
static void _download_on_http(GConnHttp * conn, GConnHttpEvent * event,
		gpointer data);
static gboolean _download_on_idle(gpointer data);
static gboolean _download_on_timeout(gpointer data);


/* functions */
static void _download_label(GtkWidget * vbox, PangoFontDescription * bold,
		GtkSizeGroup * left, GtkSizeGroup * right, char const * label,
		GtkWidget ** widget, char const * text);

static int _download(Prefs * prefs, char const * url)
{
	static Download download;
	char buf[256];
	GtkWidget * vbox;
	GtkWidget * hbox;
	GtkWidget * widget;
	GtkSizeGroup * left;
	GtkSizeGroup * right;
	PangoFontDescription * bold;

	download.prefs = prefs;
	download.url = url;
	if(prefs->output == NULL)
		prefs->output = basename(url);
	if(gettimeofday(&download.tv, NULL) != 0)
		return _download_error(NULL, "gettimeofday", 1);
	download.data_received = 0;
	download.content_length = 0;
	download.pulse = 0;
	/* window */
	download.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	snprintf(buf, sizeof(buf), "%s %s", "Download", url);
	gtk_window_set_title(GTK_WINDOW(download.window), buf);
	g_signal_connect(G_OBJECT(download.window), "delete-event", G_CALLBACK(
				_download_on_closex), NULL);
	vbox = gtk_vbox_new(FALSE, 0);
	bold = pango_font_description_new();
	pango_font_description_set_weight(bold, PANGO_WEIGHT_BOLD);
	left = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	right = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	_download_label(vbox, bold, left, right, "File: ", &download.status,
			prefs->output);
	_download_label(vbox, bold, left, right, "Status: ", &download.status,
			"Resolving...");
	_download_label(vbox, bold, left, right, "Speed: ", &download.speed,
			"0.0 kB/s");
	/* progress bar */
	download.progress = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(vbox), download.progress, TRUE, TRUE, 4);
	/* cancel button */
	hbox = gtk_hbox_new(FALSE, 4);
	widget = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(
				_download_on_cancel), &download);
	gtk_box_pack_end(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(download.window), 4);
	gtk_container_add(GTK_CONTAINER(download.window), vbox);
	g_idle_add(_download_on_idle, &download);
	_download_refresh(&download);
	gtk_widget_show_all(download.window);
	gtk_main();
	return 0;
}

static void _download_label(GtkWidget * vbox, PangoFontDescription * bold,
		GtkSizeGroup * left, GtkSizeGroup * right, char const * label,
		GtkWidget ** widget, char const * text)
{
	GtkWidget * hbox;

	hbox = gtk_hbox_new(FALSE, 0);
	*widget = gtk_label_new(label);
	gtk_widget_modify_font(*widget, bold);
	gtk_misc_set_alignment(GTK_MISC(*widget), 0, 0);
	gtk_size_group_add_widget(left, *widget);
	gtk_box_pack_start(GTK_BOX(hbox), *widget, TRUE, TRUE, 0);
	*widget = gtk_label_new(text);
	gtk_misc_set_alignment(GTK_MISC(*widget), 0, 0);
	gtk_size_group_add_widget(right, *widget);
	gtk_box_pack_start(GTK_BOX(hbox), *widget, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
}


/* download_error */
static int _download_error(Download * download, char const * message, int ret)
{
	GtkWidget * dialog;

	if(ret < 0)
	{
		fputs("download: ", stderr);
		perror(message);
		return -ret;
	}
	dialog = gtk_message_dialog_new(download != NULL
			? GTK_WINDOW(download->window) : NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, "%s: %s", message, strerror(errno));
	gtk_window_set_title(GTK_WINDOW(dialog), "Error");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return ret;
}


/* download_refresh */
static void _download_refresh(Download * download)
{
	char buf[256]; /* FIXME convert to UTF-8 */
	struct timeval tv;
	double rate;
	double fraction;

	/* XXX should check gettimeofday() return value explicitly */
	if(download->data_received > 0 && gettimeofday(&tv, NULL) == 0)
	{
		if((tv.tv_sec = tv.tv_sec - download->tv.tv_sec) < 0)
			tv.tv_sec = 0;
		if((tv.tv_usec = tv.tv_usec - download->tv.tv_usec) < 0)
		{
			tv.tv_sec--;
			tv.tv_usec += 1000000;
		}
		rate = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
		rate = download->data_received / rate;
		snprintf(buf, sizeof(buf), "%.1f kB/s", rate);
		gtk_label_set_text(GTK_LABEL(download->speed), buf);
	}
	if(download->content_length == 0)
	{
		buf[0] = '\0';
		if(download->pulse != 0)
		{
			gtk_progress_bar_pulse(GTK_PROGRESS_BAR(
						download->progress));
			download->pulse = 0;
		}
	}
	else
	{
		fraction = (double)download->data_received
			/ (double)download->content_length;
		snprintf(buf, sizeof(buf), "%.1f%%", fraction * 100);
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(
					download->progress), fraction);
	}
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(download->progress), buf);
}


/* callbacks */
static void _download_on_cancel(GtkWidget * widget, gpointer data)
{
	Download * download = data;

	gtk_widget_hide(download->window);
	/* FIXME cleanup, unlink... */
	gtk_main_quit();
}


static gboolean _download_on_closex(GtkWidget * widget, GdkEvent * event,
		gpointer data)
{
	gtk_widget_hide(widget);
	/* FIXME cleanup, unlink... */
	gtk_main_quit();
	return FALSE;
}


/* download_on_http */
static void _http_connected(Download * download);
static void _http_error(GConnHttpEventError * event, Download * download);
static void _http_data_complete(GConnHttpEventData * event,
		Download * download);
static void _http_data_partial(GConnHttpEventData * event, Download * download);
static void _http_redirect(GConnHttpEventRedirect * event, Download * download);
static void _http_resolved(GConnHttpEventResolved * event, Download * download);
static void _http_response(GConnHttpEventResponse * event, Download * download);
static void _http_timeout(Download * download);

static void _download_on_http(GConnHttp * conn, GConnHttpEvent * event,
		gpointer data)
{
	Download * download = data;

	switch(event->type)
	{
		case GNET_CONN_HTTP_CONNECTED:
			return _http_connected(download);
		case GNET_CONN_HTTP_ERROR:
			return _http_error((GConnHttpEventError*)event,
					download);
		case GNET_CONN_HTTP_DATA_COMPLETE:
			return _http_data_complete((GConnHttpEventData*)event,
					download);
		case GNET_CONN_HTTP_DATA_PARTIAL:
			return _http_data_partial((GConnHttpEventData*)event,
			       download);
		case GNET_CONN_HTTP_REDIRECT:
			return _http_redirect((GConnHttpEventRedirect*)event,
					download);
		case GNET_CONN_HTTP_RESOLVED:
			return _http_resolved((GConnHttpEventResolved*)event,
					download);
		case GNET_CONN_HTTP_RESPONSE:
			return _http_response((GConnHttpEventResponse*)event,
					download);
		case GNET_CONN_HTTP_TIMEOUT:
			return _http_timeout(download);
	}
}

static void _http_connected(Download * download)
{
	gtk_label_set_text(GTK_LABEL(download->status), "Connected");
	/* FIXME implement */
}

static void _http_error(GConnHttpEventError * event, Download * download)
{
	char buf[10];

	snprintf(buf, sizeof(buf), "%s %u", "Error ", event->code);
	gtk_label_set_text(GTK_LABEL(download->status), buf);
	/* FIXME implement */
}

static void _http_data_complete(GConnHttpEventData * event,
		Download * download)
{
	gchar * buf;
	gsize size;

	gtk_label_set_text(GTK_LABEL(download->status), "Complete");
	if(gnet_conn_http_steal_buffer(download->conn, &buf, &size))
	{
		/* FIXME use a GIOChannel instead, check errors */
		fwrite(buf, sizeof(*buf), size, download->fp);
		g_free(buf);
	}
	download->data_received = event->data_received;
	if(event->content_length == 0)
	download->content_length = (event->content_length != 0)
		? event->content_length : event->data_received;
	if(fclose(download->fp) != 0)
		_download_error(download, download->prefs->output, 0);
	_download_refresh(download);
	g_source_remove(download->timeout);
	download->timeout = 0;
}

static void _http_data_partial(GConnHttpEventData * event, Download * download)
{
	gchar * buf;
	gsize size;

	if(download->content_length == 0 && download->data_received == 0)
		gtk_label_set_text(GTK_LABEL(download->status), "Downloading");
	/* FIXME code duplication with data_complete */
	download->data_received = event->data_received;
	download->content_length = event->content_length;
	if(gnet_conn_http_steal_buffer(download->conn, &buf, &size))
	{
		/* FIXME use a GIOChannel instead, check errors */
		fwrite(buf, sizeof(*buf), size, download->fp);
		g_free(buf);
		download->pulse = 1;
	}
}

static void _http_redirect(GConnHttpEventRedirect * event, Download * download)
{
	char buf[256];

	if(event->new_location != NULL)
		snprintf(buf, sizeof(buf), "%s %s", "Redirected to",
				event->new_location);
	else
		strcpy(buf, "Redirected");
	gtk_label_set_text(GTK_LABEL(download->status), buf);
	/* FIXME implement */
}

static void _http_resolved(GConnHttpEventResolved * event, Download * download)
{
	gtk_label_set_text(GTK_LABEL(download->status), "Resolved");
	/* FIXME implement */
}

static void _http_response(GConnHttpEventResponse * event, Download * download)
{
	char buf[10];

	snprintf(buf, sizeof(buf), "%s %u", "Code ", event->response_code);
	gtk_label_set_text(GTK_LABEL(download->status), buf);
	/* FIXME implement */
}

static void _http_timeout(Download * download)
{
	gtk_label_set_text(GTK_LABEL(download->status), "Timeout");
	/* FIXME implement */
}


static gboolean _download_on_idle(gpointer data)
{
	Download * download = data;
	Prefs * prefs = download->prefs;

	if((download->fp = fopen(prefs->output, "w")) == NULL)
	{
		_download_error(download, prefs->output, 0);
		/* FIXME cleanup */
		gtk_main_quit();
		return FALSE;
	}
	download->conn = gnet_conn_http_new();
	if(gnet_conn_http_set_method(download->conn, GNET_CONN_HTTP_METHOD_GET,
			NULL, 0) != TRUE)
		return _download_error(download, "Unknown error", FALSE);
	gnet_conn_http_set_uri(download->conn, download->url);
	if(prefs->user_agent != NULL)
		gnet_conn_http_set_user_agent(download->conn,
				prefs->user_agent);
	gnet_conn_http_run_async(download->conn, _download_on_http, download);
	download->timeout = g_timeout_add(250, _download_on_timeout, download);
	return FALSE;
}


static gboolean _download_on_timeout(gpointer data)
{
	Download * download = data;

	_download_refresh(download);
	return TRUE;
}


/* usage */
static int _usage(void)
{
	fputs("Usage: download [-O output][-U user-agent] url\n"
"  -O	file to write document to\n"
"  -U	user agent string to send\n", stderr);
	return 1;
}


/* main */
int main(int argc, char * argv[])
{
	Prefs prefs;
	int o;

	memset(&prefs, 0, sizeof(prefs));
	gtk_init(&argc, &argv);
	while((o = getopt(argc, argv, "O:U:")) != -1)
		switch(o)
		{
			case 'O':
				prefs.output = optarg;
				break;
			case 'U':
				prefs.user_agent = optarg;
				break;
			default:
				return _usage();
		}
	if(optind + 1 != argc)
		return _usage();
	return _download(&prefs, argv[optind]) == 0 ? 0 : 2;
}