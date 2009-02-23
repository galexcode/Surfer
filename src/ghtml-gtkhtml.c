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
/* TODO:
 * - implement http protocol again
 * - fix URL generation for relative path
 * - fix links from directory listing
 * - progressive file load
 * - update the URL and title of the main window
 * - implement selection
 * - need to take care of CSRF? eg remotely load local files */



#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgtkhtml/gtkhtml.h>
#include <libgtkhtml/view/htmlselection.h>
#define GNET_EXPERIMENTAL
#include <gnet.h>
#include <System.h>
#include "ghtml.h"
#include "../config.h"


/* GHtml */
/* private */
typedef struct _GHtmlConn GHtmlConn;

typedef struct _GHtml
{
	Surfer * surfer;

	gchar * html_base;
	gchar * html_url;
	/* FIXME implement history */

	/* connections */
	struct _GHtmlConn ** conns;
	size_t conns_cnt;

	/* html widget */
	HtmlDocument * html_document;
	gchar * html_title;
	GtkWidget * html_view;
} GHtml;

struct _GHtmlConn
{
	GHtml * ghtml;

	char * url;
	guint64 content_length;
	guint64 data_received;
	HtmlStream * stream;

	/* file */
	GIOChannel * file;
	guint64 file_size;
	guint64 file_read;

	/* http */
	GConnHttp * http;
};


/* prototypes */
static GHtmlConn * _ghtmlconn_new(GHtml * ghtml, HtmlStream * stream,
		gchar const * url);
static void _ghtmlconn_delete(GHtmlConn * ghtmlconn);

static int _ghtml_document_load(GHtml * ghtml, gchar const * url);
static gchar * _ghtml_make_url(gchar const * base, gchar const * url);
static int _ghtml_stream_load(GHtml * ghtml, HtmlStream * stream,
		gchar const * url);

/* callbacks */
static void _on_link_clicked(HtmlDocument * document, const gchar * url);
static void _on_request_url(HtmlDocument * document, const gchar * url,
		HtmlStream * stream);
static void _on_set_base(HtmlDocument * document, const gchar * url);
static void _on_submit(HtmlDocument * document, const gchar * url,
		const gchar * method, const gchar * encoding);
static void _on_title_changed(HtmlDocument * document, const gchar * title);


/* public */
/* functions */
GtkWidget * ghtml_new(Surfer * surfer)
{
	GHtml * ghtml;
	GtkWidget * widget;

	if((ghtml = malloc(sizeof(*ghtml))) == NULL)
		return NULL;
	ghtml->surfer = surfer;
	ghtml->html_base = NULL;
	ghtml->html_url = NULL;
	ghtml->conns = NULL;
	ghtml->conns_cnt = 0;
	ghtml->html_view = html_view_new();
	ghtml->html_document = html_document_new();
	ghtml->html_title = NULL;
	g_object_set_data(G_OBJECT(ghtml->html_document), "ghtml", ghtml);
	g_signal_connect(G_OBJECT(ghtml->html_document), "link-clicked",
			G_CALLBACK(_on_link_clicked), NULL);
	g_signal_connect(G_OBJECT(ghtml->html_document), "request-url",
			G_CALLBACK(_on_request_url), NULL);
	g_signal_connect(G_OBJECT(ghtml->html_document), "set-base", G_CALLBACK(
				_on_set_base), NULL);
	g_signal_connect(G_OBJECT(ghtml->html_document), "submit", G_CALLBACK(
				_on_submit), NULL);
	g_signal_connect(G_OBJECT(ghtml->html_document), "title-changed",
			G_CALLBACK(_on_title_changed), NULL);
	html_view_set_document(HTML_VIEW(ghtml->html_view),
			ghtml->html_document);
	widget = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	g_object_set_data(G_OBJECT(widget), "ghtml", ghtml);
	gtk_container_add(GTK_CONTAINER(widget), ghtml->html_view);
	return widget;
}


/* accessors */
/* ghtml_can_go_back */
gboolean ghtml_can_go_back(GtkWidget * ghtml)
{
	/* FIXME implement */
	return FALSE;
}


/* ghtml_can_go_forward */
gboolean ghtml_can_go_forward(GtkWidget * ghtml)
{
	/* FIXME implement */
	return FALSE;
}


/* ghtml_get_link_message */
char const * ghtml_get_link_message(GtkWidget * ghtml)
{
	/* FIXME implement */
	return NULL;
}


/* ghtml_get_location */
char const * ghtml_get_location(GtkWidget * widget)
{
	GHtml * ghtml;

	ghtml = g_object_get_data(G_OBJECT(widget), "ghtml");
	return ghtml->html_url;
}


/* ghtml_get_title */
char const * ghtml_get_title(GtkWidget * widget)
{
	GHtml * ghtml;

	ghtml = g_object_get_data(G_OBJECT(widget), "ghtml");
	return ghtml->html_title;
}


/* useful */
/* ghtml_go_back */
gboolean ghtml_go_back(GtkWidget * ghtml)
{
	/* FIXME implement */
	return FALSE;
}


/* ghtml_go_forward */
gboolean ghtml_go_forward(GtkWidget * ghtml)
{
	/* FIXME implement */
	return FALSE;
}


/* ghtml_load_url */
void ghtml_load_url(GtkWidget * widget, char const * url)
{
	GHtml * ghtml;
	gchar * link;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, url);
#endif
	ghtml = g_object_get_data(G_OBJECT(widget), "ghtml");
	if((link = _ghtml_make_url(NULL, url)) != NULL)
		url = link;
	if(_ghtml_document_load(ghtml, url) != 0)
	{
		g_free(link);
		return;
	}
	g_free(ghtml->html_base);
	ghtml->html_base = (link != NULL) ? link : g_strdup(url);
	g_free(ghtml->html_url);
	ghtml->html_url = g_strdup(url);
}


/* ghtml_refresh */
void ghtml_refresh(GtkWidget * widget)
{
	GHtml * ghtml;

	ghtml = g_object_get_data(G_OBJECT(widget), "ghtml");
	if(ghtml->html_url == NULL)
		return;
	_ghtml_document_load(ghtml, ghtml->html_url);
}


/* ghtml_reload */
void ghtml_reload(GtkWidget * ghtml)
{
	ghtml_refresh(ghtml);
}


/* ghtml_select_all */
void ghtml_select_all(GtkWidget * ghtml)
{
	/* FIXME implement */
}


/* ghtml_stop */
void ghtml_stop(GtkWidget * ghtml)
{
	/* FIXME implement */
}


/* ghtml_unselect_all */
void ghtml_unselect_all(GtkWidget * widget)
{
	GHtml * ghtml;

	ghtml = g_object_get_data(G_OBJECT(widget), "ghtml");
	html_selection_clear(HTML_VIEW(ghtml->html_view));
}


/* ghtml_zoom_in */
void ghtml_zoom_in(GtkWidget * widget)
{
	GHtml * ghtml;

	ghtml = g_object_get_data(G_OBJECT(widget), "ghtml");
	html_view_zoom_in(HTML_VIEW(ghtml->html_view));
}


/* ghtml_zoom_out */
void ghtml_zoom_out(GtkWidget * widget)
{
	GHtml * ghtml;

	ghtml = g_object_get_data(G_OBJECT(widget), "ghtml");
	html_view_zoom_out(HTML_VIEW(ghtml->html_view));
}


/* ghtml_zoom_reset */
void ghtml_zoom_reset(GtkWidget * widget)
{
	GHtml * ghtml;

	ghtml = g_object_get_data(G_OBJECT(widget), "ghtml");
	html_view_zoom_reset(HTML_VIEW(ghtml->html_view));
}


/* private */
/* functions */
static GHtmlConn * _ghtmlconn_new(GHtml * ghtml, HtmlStream * stream,
		gchar const * url)
{
	GHtmlConn ** p;
	GHtmlConn * c;

	if((p = realloc(ghtml->conns, sizeof(*p) * (ghtml->conns_cnt + 1)))
			== NULL)
		return NULL;
	ghtml->conns = p;
	if((c = malloc(sizeof(*c))) == NULL)
		return NULL;
	ghtml->conns[ghtml->conns_cnt] = c;
	c->ghtml = ghtml;
	c->url = strdup(url);
	c->content_length = 0;
	c->data_received = 0;
	c->stream = stream;
	c->file = NULL;
	c->file_size = 0;
	c->file_read = 0;
	c->http = NULL;
	if(c->url == NULL)
	{
		_ghtmlconn_delete(c);
		return NULL;
	}
	ghtml->conns_cnt++;
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p, %p, \"%s\") => %p\n", __func__, ghtml,
			stream, url, c);
#endif
	return c;
}


/* ghtmlconn_delete */
static void _ghtmlconn_delete_file(GHtmlConn * ghtmlconn);
static void _ghtmlconn_delete_http(GHtmlConn * ghtmlconn);

static void _ghtmlconn_delete(GHtmlConn * ghtmlconn)
{
	GHtml * ghtml = ghtmlconn->ghtml;
	size_t i;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p)\n", __func__, ghtmlconn);
#endif
	for(i = 0; i < ghtml->conns_cnt; i++)
		if(ghtml->conns[i] == ghtmlconn)
		{
			ghtml->conns[i] = NULL; /* don't double free later */
			break;
		}
	if(ghtmlconn->file != NULL)
		_ghtmlconn_delete_file(ghtmlconn);
	else if(ghtmlconn->http != NULL)
		_ghtmlconn_delete_http(ghtmlconn);
	free(ghtmlconn->url);
	if(ghtmlconn->stream != NULL)
		html_stream_close(ghtmlconn->stream);
	free(ghtmlconn);
}

static void _ghtmlconn_delete_file(GHtmlConn * ghtmlconn)
{
	/* FIXME implement */
}

static void _ghtmlconn_delete_http(GHtmlConn * ghtmlconn)
{
	gnet_conn_http_delete(ghtmlconn->http);
}


/* ghtml_document_load */
static int _ghtml_document_load(GHtml * ghtml, gchar const * url)
{
	html_document_open_stream(ghtml->html_document, "text/html");
	return _ghtml_stream_load(ghtml, ghtml->html_document->current_stream,
			url);
}


/* ghtml_make_url */
static gchar * _ghtml_make_url(gchar const * base, gchar const * url)
{
	char * b;
	char * p;

	if(url == NULL)
		return NULL;
	/* XXX use a more generic protocol finder (strchr(':')) */
	if(strncmp("http://", url, 7) == 0)
		return g_strdup(url);
	if(strncmp("ftp://", url, 6) == 0)
		return g_strdup(url);
	if(base != NULL)
	{
		if(url[0] == '/')
			/* FIXME construct from / of base */
			return g_strdup_printf("%s%s", base, url);
		/* construct from basename */
		if((b = strdup(base)) == NULL)
			return NULL;
		if((p = strrchr(b, '/')) != NULL)
			*p = '\0';
		p = g_strdup_printf("%s/%s", b, url);
		free(b);
		return p;
	}
	/* base is NULL, url is not NULL */
	if(url[0] == '/')
		return g_strdup(url);
	/* guess protocol */
	if(strncmp("ftp", url, 3) == 0)
		return g_strdup_printf("%s%s", "ftp://", url);
	return g_strdup_printf("%s%s", "http://", url);
}


/* ghtml_stream_load */
static gboolean _stream_load_idle(gpointer data);
static gboolean _stream_load_idle_directory(GHtmlConn * conn);
static gboolean _stream_load_idle_file(GHtmlConn * conn);
static gboolean _stream_load_watch_file(GIOChannel * source,
		GIOCondition condition, gpointer data);
static gboolean _stream_load_idle_http(GHtmlConn * conn);
static void _stream_load_watch_http(GConnHttp * connhttp,
		GConnHttpEvent * event, gpointer data);
static void _http_data_complete(GConnHttpEventData * event, GHtmlConn * conn);
static void _http_data_partial(GConnHttpEventData * event, GHtmlConn * conn);

static int _ghtml_stream_load(GHtml * ghtml, HtmlStream * stream,
		gchar const * url)
{
	GHtmlConn * conn;

	if((conn = _ghtmlconn_new(ghtml, stream, url)) == NULL)
		return -1;
	g_idle_add(_stream_load_idle, conn);
	return 0;
}

static gboolean _stream_load_idle(gpointer data)
{
	GHtmlConn * conn = data;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(%p) \"%s\"\n", __func__, data, conn->url);
#endif
	if(conn->url[0] == '/')
		return _stream_load_idle_file(conn);
	if(strncmp(conn->url, "file:", 5) == 0)
	{
		strcpy(conn->url, &conn->url[5]); /* XXX no corruption? */
		return _stream_load_idle_file(conn);
	}
	if(strncmp(conn->url, "http:", 5) == 0)
		return _stream_load_idle_http(conn);
	surfer_error(conn->ghtml->surfer, "Unknown protocol", 0);
	_ghtmlconn_delete(conn);
	return FALSE;
}

static gboolean _stream_load_idle_directory(GHtmlConn * conn)
{
	const char tail[] = "</ul>\n<hr>\n</body></html>\n";
	char buf[1024];
	DIR * dir;
	struct dirent * de;

	if((dir = opendir(conn->url)) == NULL)
		surfer_error(conn->ghtml->surfer, strerror(errno), 0);
	else
	{
		snprintf(buf, sizeof(buf), "%s%s%s%s%s", "<html><head><title>"
				"Index of ", conn->url, "</title><body>\n"
				"<h1>Index of ", conn->url,
				"</h1>\n<hr>\n<ul>\n");
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s", buf);
#endif
		html_stream_write(conn->stream, buf, strlen(buf));
		while((de = readdir(dir)) != NULL)
		{
			snprintf(buf, sizeof(buf), "%s%s%s%s%s",
					"<li><a href=\"", de->d_name, "\">",
					de->d_name, "</a></li>\n");
#ifdef DEBUG
			fprintf(stderr, "DEBUG: %s", buf);
#endif
			html_stream_write(conn->stream, buf, strlen(buf));
		}
#ifdef DEBUG
		fprintf(stderr, "DEBUG: %s", tail);
#endif
		html_stream_write(conn->stream, tail, sizeof(tail) - 1);
		closedir(dir);
		surfer_set_progress(conn->ghtml->surfer, 1.0);
	}
	_ghtmlconn_delete(conn);
	return FALSE;
}

static gboolean _stream_load_idle_file(GHtmlConn * conn)
{
	int fd;
	struct stat st;
	GIOChannel * channel;

	surfer_set_progress(conn->ghtml->surfer, 0.0);
	if((fd = open(conn->url, O_RDONLY)) < 0)
	{
		surfer_error(conn->ghtml->surfer, "Not implemented yet", 0);
		_ghtmlconn_delete(conn);
	}
	else
	{
		if(fstat(fd, &st) == 0)
		{
			if(S_ISDIR(st.st_mode))
			{
				close(fd);
				return _stream_load_idle_directory(conn);
			}
			conn->file_size = st.st_size;
		}
		channel = g_io_channel_unix_new(fd);
		g_io_add_watch(channel, G_IO_IN, _stream_load_watch_file, conn);
	}
	return FALSE;
}

static gboolean _stream_load_watch_file(GIOChannel * source,
		GIOCondition condition, gpointer data)
{
	GHtmlConn * conn = data;
	gsize len;
	char buf[BUFSIZ];
	gdouble fraction;

	if(condition != G_IO_IN)
	{
		_ghtmlconn_delete(conn);
		return FALSE;
	}
	if(g_io_channel_read(source, buf, sizeof(buf), &len)
			!= G_IO_ERROR_NONE)
	{
		/* FIXME report error */
		_ghtmlconn_delete(conn);
		return FALSE;
	}
	if(len == 0) /* no more data */
	{
		_ghtmlconn_delete(conn);
		return FALSE;
	}
	html_stream_write(conn->stream, buf, len);
	conn->file_read+=len;
	if(conn->file_size > 0)
	{
		fraction = conn->file_read;
		surfer_set_progress(conn->ghtml->surfer,
				fraction / conn->file_size);
	}
	return TRUE;
}

static gboolean _stream_load_idle_http(GHtmlConn * conn)
{
	surfer_set_progress(conn->ghtml->surfer, 0.0);
	conn->http = gnet_conn_http_new();
	gnet_conn_http_set_uri(conn->http, conn->url);
	gnet_conn_http_set_user_agent(conn->http, "DeforaOS " PACKAGE);
	gnet_conn_http_set_method(conn->http, GNET_CONN_HTTP_METHOD_GET, NULL,
			0);
	gnet_conn_http_run_async(conn->http, _stream_load_watch_http, conn);
	return FALSE;
}

static void _stream_load_watch_http(GConnHttp * connhttp,
		GConnHttpEvent * event, gpointer data)
	/* FIXME handle error cases */
{
	GHtmlConn * conn = data;

	if(conn->http != connhttp)
		return; /* FIXME report error */
	switch(event->type)
	{
		/* FIXME implement the other cases */
		case GNET_CONN_HTTP_DATA_COMPLETE:
			return _http_data_complete((GConnHttpEventData*)event,
					conn);
		case GNET_CONN_HTTP_DATA_PARTIAL:
			return _http_data_partial((GConnHttpEventData*)event,
					conn);
	}
}

static void _http_data_complete(GConnHttpEventData * event, GHtmlConn * conn)
{
	gchar * buf;
	gsize size;

	if(gnet_conn_http_steal_buffer(conn->http, &buf, &size) != TRUE)
	{
		/* FIXME report error */
		surfer_set_progress(conn->ghtml->surfer, 0.0);
	}
	else
	{
		if(size > 0)
			html_stream_write(conn->stream, buf, size);
		surfer_set_progress(conn->ghtml->surfer, 1.0);
	}
	_ghtmlconn_delete(conn);
}

static void _http_data_partial(GConnHttpEventData * event, GHtmlConn * conn)
{
	gchar * buf;
	gsize size;
	gdouble fraction;

	if(gnet_conn_http_steal_buffer(conn->http, &buf, &size) != TRUE)
	{
		/* FIXME report error */
		_ghtmlconn_delete(conn);
		return;
	}
	html_stream_write(conn->stream, buf, size);
	if(conn->content_length > 0)
	{
		fraction = conn->data_received;
		surfer_set_progress(conn->ghtml->surfer,
				fraction / conn->content_length);
	}
}


/* callbacks */
static void _on_link_clicked(HtmlDocument * document, const gchar * url)
{
	GHtml * ghtml;
	gchar * link;

	ghtml = g_object_get_data(G_OBJECT(document), "ghtml");
	if((link = _ghtml_make_url(ghtml->html_base, url)) == NULL)
		return; /* FIXME report error */
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\") base=\"%s\" => \"%s\"\n", __func__,
			url, ghtml->html_base, link);
#endif
	_ghtml_document_load(ghtml, link);
	g_free(link);
}


static void _on_request_url(HtmlDocument * document, const gchar * url,
		HtmlStream * stream)
{
	GHtml * ghtml;
	gchar * link;

	ghtml = g_object_get_data(G_OBJECT(document), "ghtml");
	if((link = _ghtml_make_url(ghtml->html_base, url)) == NULL)
		return; /* FIXME report error */
#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\") base=\"%s\" => \"%s\"\n", __func__,
			url, ghtml->html_base, link);
#endif
	_ghtml_stream_load(ghtml, stream, link);
	g_free(link);
}


static void _on_set_base(HtmlDocument * document, const gchar * url)
{
	GHtml * ghtml;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, url);
#endif
	ghtml = g_object_get_data(G_OBJECT(document), "ghtml");
	g_free(ghtml->html_base);
	ghtml->html_base = g_strdup(url);
}


static void _on_submit(HtmlDocument * document, const gchar * url,
		const gchar * method, const gchar * encoding)
{
	GHtml * ghtml;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\", \"%s\", \"%s\")\n", __func__,
			url, method, encoding);
#endif
	ghtml = g_object_get_data(G_OBJECT(document), "ghtml");
	if(strcmp(method, "GET") == 0)
		_ghtml_document_load(ghtml, url);
	else if(strcmp(method, "POST") == 0)
		_ghtml_document_load(ghtml, url);
	else
	{
		/* FIXME implement */
		surfer_error(ghtml->surfer, "Unsupported method", 0);
		return;
	}
}


static void _on_title_changed(HtmlDocument * document, const gchar * title)
{
	GHtml * ghtml;

#ifdef DEBUG
	fprintf(stderr, "DEBUG: %s(\"%s\")\n", __func__, title);
#endif
	ghtml = g_object_get_data(G_OBJECT(document), "ghtml");
	g_free(ghtml->html_title);
	ghtml->html_title = g_strdup(title);
	surfer_set_title(ghtml->surfer, title);
}
