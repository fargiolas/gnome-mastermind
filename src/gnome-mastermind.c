/* -*- Mode: C; indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8 -*- */

/*
 * GNOME Mastermind
 *
 * Authors:
 *  Filippo Argiolas <filippo.argiolas@gmail.com>  (2007)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Other copyright information shoud be found on copyright files
 * shipped with this package
 *
 */

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gconf/gconf-client.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef ENABLE_NLS
#include <glib/gi18n.h>
#else /* !ENABLE_NLS */
#define _(String) (String)
#define N_(String) (String)
#endif /* ENABLE_NLS */

#define TRAY_ROWS 1
#define TRAY_COLS 8
#define TRAY_PAD 4
#define GRID_COLS 4
#define GRID_SZ 40
#define BM GRID_SZ
#define BS BM*0.75
#define TRAY_SZ 32
#define GRID_XPAD 40
#define GRID_YPAD 10

GtkWidget *window; //main window 
GtkWidget *vbox; //main vbox

GtkWidget *drawing_area = NULL;
GtkWidget *toolbar;
GtkAction *show_toolbar_action;
GtkAction *undo_action;
GtkAction *redo_action;

gboolean gm_debug_on;

GtkWidget *status;

gint solution[4];
gint tmp[4];
gint guess[4];
gint bulls;
gint cows;
gint newgame;
gint pressed = 0;
gint mstarted = 0;
gint xbk, ybk;
gint stx, sty;
gint motion_x_shift, motion_y_shift;

gint xpos, ypos;
gint old_xpos, old_ypos;
gint selectedcolor;
gint filled[4];
gint grid_rows;
gint frame_min_w;
gint frame_min_h;
gint grid_xpad;
gint grid_ypad;
gint grid_sz;
gint ballmed, ballsm;
gint tray_sz;
gint tray_w, tray_h;

gchar *gc_theme;
gchar *gc_fgcolor;
gchar *gc_bgcolor;
gint gc_max_tries;
gboolean gc_gtkstyle_colors = TRUE;
gboolean gc_show_toolbar = TRUE;

gint confcount = 0;

guint timeout_id = 0;

gint movecount;

gint **movearray = NULL;
gint *lastmove = NULL;
GList *undolist = NULL;
GList *current = NULL;



/* Backing pixmap for drawing area */
static GdkPixmap *pixmap = NULL;
static GdkPixmap *traymapbk = NULL; // quick way to store clean tray
static GdkPixmap *motionbk = NULL; // save pixmap to redrawing it after motion
static GdkPixmap *cellbk = NULL;
static GdkPixbuf *pixbuf = NULL;
static GdkRectangle rect;
static GError *error = 0;

static GdkPixbuf *tileset_bg = NULL; /* main tileset */
static GdkPixbuf *tileset_sm = NULL; /* small tileset */

GList *theme_list = NULL;

GdkColor fgcolor;
GdkColor bgcolor;
GdkColor savedcolor;

GConfClient *settings;

GtkWidget *pref_dialog;

GtkWidget *fg_colorbutton = NULL;
GtkWidget *bg_colorbutton = NULL;

static gboolean redraw_current_game();
gboolean start_new_gameboard (GtkWidget *widget);
void new_game (void);

gint gm_debug (const gchar *format, ...) {
	va_list args;
	g_return_val_if_fail (format != NULL, -1);
	if (gm_debug_on) {
		va_start (args, format);
		g_vprintf (format, args);
		va_end(args);
	}
	return 0;
}

void reset_default_settings (void) {
	gconf_client_set_string (settings, "/apps/gnome-mastermind/theme", "simple.svg", NULL);
/* gconf_client_set_int (settings, "/apps/gnome-mastermind/big_ball_size", 38, NULL);
   gconf_client_set_int (settings, "/apps/gnome-mastermind/small_ball_size", 30, NULL); */
	gconf_client_set_int (settings, "/apps/gnome-mastermind/maximum_tries", 10, NULL);
	
	gconf_client_set_bool (settings, "/apps/gnome-mastermind/gtkstyle_colors", TRUE, NULL);
	gconf_client_set_string (settings, "/apps/gnome-mastermind/bgcolor", "#e4dfed", NULL);
	gconf_client_set_string (settings, "/apps/gnome-mastermind/fgcolor", "#3d2b78", NULL);
	gconf_client_set_bool (settings, "/apps/gnome-mastermind/show_toolbar", TRUE, NULL);
}


static void
max_tries_notify_func (GConfClient *client,
		       guint cnxn_id,
		       GConfEntry *entry,
		       gpointer user_data)
{
	GConfValue *value;
	GtkWidget *dialog;
	value = gconf_entry_get_value (entry);
	gc_max_tries = gconf_value_get_int (value);
	if (gc_max_tries != grid_rows) {
		gm_debug ("maximum tries are changed to %d\n", gc_max_tries);
		dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
							     GTK_DIALOG_DESTROY_WITH_PARENT,
							     GTK_MESSAGE_WARNING,
							     GTK_BUTTONS_YES_NO,
							     _("<span size=\"medium\" weight=\"bold\">Settings have changed!</span>\n\n"
							       "Maximum number of tries has been changed.\n"
							       "The game has to be restarted for this change to apply\n\n"
							       "Start a new game?"));
		gint response = gtk_dialog_run (GTK_DIALOG (dialog));
		if ( response == GTK_RESPONSE_YES) {

			new_game();
		}

		gtk_widget_destroy (dialog);
	}
}

static void
theme_notify_func (GConfClient *client,
		   guint cnxn_id,
		   GConfEntry *entry,
		   gpointer user_data)
{
	GtkWidget *dialog;
	GConfValue *value;
	value = gconf_entry_get_value (entry);
	gc_theme = g_strdup_printf ("%s%s%s", PKGDATA_DIR, "/themes/", gconf_value_get_string (value));
	gm_debug ("theme changed\n");

	if (!g_file_test (gc_theme, G_FILE_TEST_EXISTS)) {
		dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
							     GTK_DIALOG_DESTROY_WITH_PARENT,
							     GTK_MESSAGE_WARNING,
							     GTK_BUTTONS_OK,
							     _("<span size=\"medium\" weight=\"bold\">Unable to load theme!</span>\n\n"
							       "<i>'%s'</i> does not exist!!\n\n"
							       "Default theme will be loaded instead."), gc_theme);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		gconf_client_set_string (settings, "/apps/gnome-mastermind/theme", "simple.svg", NULL);
		gc_theme = g_strdup_printf ("%s%s", PKGDATA_DIR, "/themes/simple.svg");
	}

	old_xpos = xpos;
	old_ypos = ypos;
	selectedcolor = -1;
	gtk_widget_queue_resize (drawing_area);
}

static void
color_notify_func (GConfClient *client,
		   guint cnxn_id,
		   GConfEntry *entry,
		   gpointer user_data)
{
	GConfValue *value;
	gchar *key;
	key = g_strdup ( gconf_entry_get_key (entry) );
	value = gconf_entry_get_value (entry);
	if (!g_ascii_strcasecmp ("/apps/gnome-mastermind/bgcolor", key)) 
		gc_bgcolor = g_strdup (gconf_value_get_string (value));
	else if (!g_ascii_strcasecmp ("/apps/gnome-mastermind/fgcolor", key)) 
		gc_fgcolor = g_strdup (gconf_value_get_string (value));
	else if (!g_ascii_strcasecmp ("/apps/gnome-mastermind/gtkstyle_colors", key)) 
		gc_gtkstyle_colors = gconf_value_get_bool (value);

	g_free(key);

	old_xpos = xpos;
	old_ypos = ypos;
	gtk_widget_queue_resize (drawing_area);
}

static void
toolbar_notify_func (GConfClient *client,
		     guint cnxn_id,
		     GConfEntry *entry,
		     gpointer user_data)
{
	GConfValue *value;
	gchar *key;

	key = g_strdup ( gconf_entry_get_key (entry) );
	value = gconf_entry_get_value (entry);
	gc_show_toolbar = gconf_value_get_bool (value);
	g_free(key);
	
	if (gc_show_toolbar)
		gtk_widget_show (toolbar);
	else
		gtk_widget_hide (toolbar);

	gtk_window_resize (GTK_WINDOW(window), 1, 1);
}

void init_gconf (void) {
	gchar *tmp;
	gchar *tmp2;
	GtkWidget *dialog;
 
	settings = gconf_client_get_default();

	if (gconf_client_dir_exists (settings, "/apps/gnome-mastermind", NULL)) {
		gm_debug ("dir exists\n");
		tmp = gconf_client_get_string (settings, "/apps/gnome-mastermind/theme", NULL);
		tmp2 = g_strdup_printf ("%s%s%s", PKGDATA_DIR, "/themes/", tmp);
		gm_debug ("checking %s\n", tmp2);
		if (g_file_test (tmp2, G_FILE_TEST_EXISTS)) {
			gm_debug ("seems to exist\n");
			gc_theme = g_strdup (tmp2);
		}
		else { 
			dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
								     GTK_DIALOG_DESTROY_WITH_PARENT,
								     GTK_MESSAGE_WARNING,
								     GTK_BUTTONS_OK,
								     _("<span size=\"medium\" weight=\"bold\">Unable to load theme!</span>\n\n"
								       "<i>'%s'</i> does not exist!!\n\n"
								       "Default theme will be loaded instead."), tmp2);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			gconf_client_set_string (settings, "/apps/gnome-mastermind/theme", "simple.svg", NULL);
			gc_theme = g_strdup_printf ("%s%s", PKGDATA_DIR, "/themes/simple.svg");
		}
		g_free (tmp);
		g_free (tmp2);
	 
/*	 gc_small_size = gconf_client_get_int (settings, "/apps/gnome-mastermind/small_ball_size", NULL);
	 gc_big_size = gconf_client_get_int (settings, "/apps/gnome-mastermind/big_ball_size", NULL); */
		gc_max_tries = gconf_client_get_int (settings, "/apps/gnome-mastermind/maximum_tries", NULL);
		gc_gtkstyle_colors = gconf_client_get_bool (settings, "/apps/gnome-mastermind/gtkstyle_colors", NULL);
		gc_bgcolor = gconf_client_get_string (settings, "/apps/gnome-mastermind/bgcolor", NULL);
		gc_fgcolor = gconf_client_get_string (settings, "/apps/gnome-mastermind/fgcolor", NULL);
		gc_show_toolbar = gconf_client_get_bool (settings, "/apps/gnome-mastermind/show_toolbar", NULL);
		gm_debug ("settings: \n");
		gm_debug ("theme: %s\n", gc_theme);
		gm_debug ("gc_max_tries: %d\n", gc_max_tries);
		gm_debug ("gc_gtkstyle_colors: %d\n", gc_gtkstyle_colors);
		gm_debug ("gc_fgcolor: %s\n", gc_fgcolor);
		gm_debug ("gc_bgcolor: %s\n", gc_bgcolor);
		gm_debug ("gc_show_toolbar: %d\n", gc_show_toolbar);
		gconf_client_add_dir (settings,
				      "/apps/gnome-mastermind",
				      GCONF_CLIENT_PRELOAD_NONE,
				      NULL);
		gconf_client_notify_add (settings, "/apps/gnome-mastermind/maximum_tries",
					 max_tries_notify_func, NULL, NULL, NULL);
		gconf_client_notify_add (settings, "/apps/gnome-mastermind/theme",
					 theme_notify_func, NULL, NULL, NULL);
		gconf_client_notify_add (settings, "/apps/gnome-mastermind/gtkstyle_colors",
					 color_notify_func, NULL, NULL, NULL);
		gconf_client_notify_add (settings, "/apps/gnome-mastermind/bgcolor",
					 color_notify_func, NULL, NULL, NULL);
		gconf_client_notify_add (settings, "/apps/gnome-mastermind/fgcolor",
					 color_notify_func, NULL, NULL, NULL);
		gconf_client_notify_add (settings, "/apps/gnome-mastermind/show_toolbar",
					 toolbar_notify_func, NULL, NULL, NULL);
	}
	else {
		gm_debug ("dir not exists\n");
		reset_default_settings();
		init_gconf();
	}
}

gint * init_lastmove (void) {
	
	gint *lm = NULL;
	int i;

	lm = g_try_malloc (GRID_COLS * sizeof (gint));
	for (i = 0; i < GRID_COLS; i++)
		lm[i] = -1;
	
	return lm;
}

void init_game (void) {
	int i, j;
	GRand *rand;
	gm_debug ("initializing new game\n");
	cows = bulls = newgame = movecount = 0;

	grid_rows = gc_max_tries;

	gm_debug ("before\n");
	for (i = 0; i < grid_rows; i++) 
		for (j = 0; j < GRID_COLS+2; j++) 
			movearray[i][j] = -1;
	gm_debug ("after\n");

	if (undolist) {
		undolist = g_list_first (undolist);
		g_list_foreach (undolist, (GFunc) g_free, NULL);
		g_list_free (undolist);
	}

	undolist = NULL;

	undolist = g_list_append (undolist, init_lastmove());


	gtk_widget_set_size_request (GTK_WIDGET (drawing_area), frame_min_w, frame_min_h);

	selectedcolor = -1;
// gc_theme = g_strdup_printf ("%s%s", PKGDATA_DIR, "/themes/simple");
// gm_debug ("%s\n", gc_theme);

	rand = g_rand_new();
	for (i = 0; i < 4; i++) {
		solution[i] = g_rand_int_range (rand, 0, TRAY_ROWS*TRAY_COLS);
		filled[i] = 0;
	} 
	g_free (rand);
}


void traycl2xy (int c, int l, int *x, int *y, GtkWidget *widget) {
	*x = widget->allocation.width/2 -tray_sz*TRAY_COLS/2+tray_sz*c+1; // questo +1 mi sa che serve perchè 
	*y = widget->allocation.height - tray_h/2-tray_sz*TRAY_ROWS/2+tray_sz*l; // le palline non sono centrate
}

void gridcl2xy (int c, int l, int *x, int *y, GtkWidget *widget) {
	gm_debug ("!!! c:%d l:%d\n", c, l);
	*x = grid_xpad+grid_sz*c;
	*y = grid_ypad+grid_sz*l;
}

void trayxy2cl (int x, int y, int *c, int *l, GtkWidget *widget) {
	int x1, y1;
	x1 = x - (widget->allocation.width/2-tray_sz*TRAY_COLS/2);
	y1 = y - (widget->allocation.height - tray_h/2-tray_sz*TRAY_ROWS/2);
	if ((x1 >= 0) &&
	    (x1 <= tray_sz*TRAY_COLS) &&
	    (y1 >= 0) && 
	    (y1 <= tray_sz*TRAY_ROWS) &&
	    ((x1%tray_sz) != 0) &&
	    ((y1%tray_sz) != 0)) {
		*c= (int) x1 / tray_sz;
		*l= (int) y1 / tray_sz;
	}
	else *c = *l = -1;
}

void gridxy2cl (int x, int y, int *c, int *l, GtkWidget *widget) {
	int x1, y1;
	x1 = x - grid_xpad;
	y1 = y - grid_ypad;
// gm_debug ("x1: %d y1:%d\n", x1, y1);
	if ((x1 >= 0) &&
	    (y1 >= 0) &&
	    (x1 <= grid_sz*GRID_COLS) &&
	    (y1 <= grid_sz*grid_rows) &&
	    ((x1%grid_sz) != 0) &&
	    ((y1%grid_sz) != 0)) {
//	 gm_debug ("clicked into the grid\n");
		*c = (int) x1 / grid_sz;
		*l = (int) y1 / grid_sz;
//	 gm_debug ("c:%d l:%d\n", *c, *l);
	} 
}

static gboolean clean_tray (GtkWidget *widget) {
	if(!pixmap || !traymapbk)
		return FALSE;
	gdk_draw_drawable(pixmap,
			  widget->style->white_gc,
			  traymapbk,
			  0, 0,
			  0, widget->allocation.height - tray_h,
			  -1, -1);
	rect.x = 0;
	rect.y = widget->allocation.height - tray_h;
	rect.width = widget->allocation.width;
	rect.height = tray_h;
	gdk_window_invalidate_rect (widget->window, &rect, FALSE);
	return FALSE;
}

void draw_main_grid (GtkWidget *widget) {

	gint i, j;
	gint x, y;
	cairo_t *cr;

	gdouble wah, waw;

	waw = widget->allocation.width;
	wah = widget->allocation.height;
	
	cr = gdk_cairo_create (pixmap);

	cairo_save (cr);
	
	gdk_cairo_set_source_color (cr, &fgcolor);
	cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);
	cairo_set_line_width (cr, 1);
	for (i = 0; i <= GRID_COLS; i++) {
		cairo_move_to (cr, 
			       grid_xpad+grid_sz*i+1,
			       grid_ypad);
		cairo_line_to (cr,
			       grid_xpad+grid_sz*i+1,
			       grid_ypad+grid_sz*grid_rows);
	}
	for (i = 0; i <= 2; i++) {
		cairo_move_to (cr,
			       grid_xpad+grid_sz*(GRID_COLS+1)+grid_sz*i/2+1,
			       grid_ypad);
		cairo_line_to (cr,
			       grid_xpad+grid_sz* (GRID_COLS+1)+grid_sz*i/2+1,
			       grid_ypad+grid_sz*grid_rows);

	}

	cairo_stroke (cr);
	cairo_set_line_width (cr, 2);
	for (j = 0; j <= grid_rows; j++) {
		cairo_move_to (cr, 
			       grid_xpad+1,
			       grid_ypad+grid_sz*j);
		cairo_line_to (cr,
			       grid_xpad+grid_sz*GRID_COLS,
			       grid_ypad+grid_sz*j);

			       }
	cairo_stroke (cr);

	for (j = 0; j <= grid_rows*2; j++) {
		if (j%2) 
			cairo_set_line_width (cr, 1);
		else
			cairo_set_line_width (cr, 2);
		cairo_move_to (cr, 
			       grid_xpad+grid_sz* (GRID_COLS+1)+1,
			       grid_ypad+grid_sz*j/2);
		cairo_line_to (cr,
			       grid_xpad+grid_sz* (GRID_COLS+2),
			       grid_ypad+grid_sz*j/2);
		cairo_stroke (cr);
	}
	/* brighten grid */

	cairo_set_operator (cr, CAIRO_OPERATOR_XOR);
	cairo_set_line_width (cr, 0);
	cairo_rectangle (cr, grid_xpad+1, grid_ypad+1, grid_sz*GRID_COLS-1, grid_sz* (grid_rows)-2);
	cairo_rectangle (cr, grid_xpad+grid_sz* (GRID_COLS+1)+1, grid_ypad+1, grid_sz-1, grid_sz* (grid_rows)-2);
	cairo_set_source_rgba (cr, 1, 1, 1, 0.3);
	cairo_fill (cr);
	cairo_stroke (cr);

	/* darken inactive cells */

	cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
	cairo_set_line_width (cr, 0);
	cairo_rectangle (cr, grid_xpad+1, grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos)))+1, grid_sz*GRID_COLS-1, grid_sz-2);
	cairo_rectangle (cr, grid_xpad+grid_sz* (GRID_COLS+1)+1, grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos)))+1, grid_sz-1, grid_sz-2);
	cairo_set_source_rgba (cr, 1, 1, 1, 0.35);
	cairo_fill (cr);
	cairo_stroke (cr); 

	/* new tray drawings */

	cairo_restore (cr);
	cairo_save (cr);

	cairo_set_line_width (cr, 0);
	gdk_cairo_set_source_color (cr, &bgcolor);
	cairo_rectangle (cr, 
			 grid_xpad-grid_sz, 
			 wah - tray_h,
			 widget->allocation.width-grid_xpad*2+grid_sz*2, tray_h);
	cairo_fill_preserve (cr);

	cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
	cairo_set_source_rgba (cr, 1, 1, 1, 0.25);

	cairo_move_to (cr, 3 + grid_xpad - grid_sz, wah - tray_h + 2);
	cairo_rel_curve_to (cr,
			    0, 0,
			    0, tray_h / 2,
			    30, tray_h / 2);
	cairo_rel_line_to (cr, widget->allocation.width-66-grid_xpad*2+grid_sz*2, 0);
	cairo_rel_curve_to (cr,
			    0, 0,
			    30, 0,
			    30, - tray_h / 2);
	cairo_rel_line_to (cr, 0, tray_h - 5);
	cairo_rel_line_to (cr, -widget->allocation.width+6+grid_xpad*2-grid_sz*2, 0);
	cairo_close_path (cr);

	cairo_fill (cr);
	cairo_stroke (cr);

	/* draw border */
	cairo_restore (cr);
	gdk_cairo_set_source_color (cr, &fgcolor);
	cairo_rectangle (cr, 1 + grid_xpad - grid_sz, 
			 wah - tray_h, 
			 widget->allocation.width - 2 - grid_xpad*2 + grid_sz*2,
			 tray_h - 1);
	cairo_set_line_width (cr, 2);
	cairo_stroke (cr);

	cairo_destroy (cr);

	for (j = 0; j < TRAY_ROWS; j++)
		for (i = 0; i < TRAY_COLS; i++) {
			traycl2xy (i, j, &x, &y, widget);
			gdk_draw_pixbuf (pixmap,
					 NULL,
					 tileset_sm,
					 ballsm * (i+ ((j)*TRAY_COLS)), 0,
					 x, y,
					 ballsm, ballsm, 
					 GDK_RGB_DITHER_MAX, 0, 0);
		}
	if (traymapbk) g_object_unref (traymapbk);
	traymapbk = gdk_pixmap_new (drawing_area->window,
				    drawing_area->allocation.width,
				    tray_h,
				    -1);
	gdk_draw_drawable (traymapbk,
			   drawing_area->style->white_gc,
			   pixmap,
			   0, drawing_area->allocation.height - tray_h,
			   0, 0,
			   -1, tray_h);
	
	gridcl2xy (0, grid_rows-1, &x, &y, drawing_area);

	if (cellbk) g_object_unref (cellbk);
	cellbk = gdk_pixmap_new (widget->window,
				 ballmed-2,
				 ballmed-2,

				 -1);
	
	gdk_draw_drawable (cellbk,
			   widget->style->white_gc,
			   pixmap,
			   x + 1, y + 1,
			   0, 0,
			   ballmed - 1, ballmed -1);
}

gboolean start_new_gameboard (GtkWidget *widget) {

	cairo_t *cr;
	
	if (tileset_sm) g_object_unref (tileset_sm);
	if (tileset_bg) g_object_unref (tileset_bg);
 
	tileset_sm = gdk_pixbuf_new_from_file_at_size (gc_theme,
						       ballsm*8+ballsm/2, ballsm, &error);
	tileset_bg = gdk_pixbuf_new_from_file_at_size (gc_theme,
						       ballmed*8+ballmed/2, ballmed, &error);
	if (error) {
		g_warning ("Failed to load '%s': %s", gc_theme, error->message);
		g_error_free (error);
		error = NULL;
	}

	if (gc_gtkstyle_colors) {
		/* sfondo della finestra di gioco */

		bgcolor = widget->style->base[GTK_STATE_SELECTED];
		bgcolor.red = (bgcolor.red + (widget->style->white).red * 6.2 ) / 8;
		bgcolor.green = (bgcolor.green + (widget->style->white).green * 6.2 ) / 8;
		bgcolor.blue = (bgcolor.blue + (widget->style->white).blue * 6.2 ) / 8;
	 
		/* bordo delle griglie */
	 
		fgcolor = widget->style->base[GTK_STATE_SELECTED];
		fgcolor.red = (fgcolor.red + (widget->style->black).red ) / 2;
		fgcolor.green = (fgcolor.green + (widget->style->black).green ) / 2;
		fgcolor.blue = (fgcolor.blue + (widget->style->black).blue ) / 2;

	} else {
		gdk_color_parse (gc_bgcolor, &bgcolor);
		gdk_color_parse (gc_fgcolor, &fgcolor); 
	}
	if (GTK_IS_WIDGET (pref_dialog)) {
		gtk_color_button_set_color (GTK_COLOR_BUTTON (fg_colorbutton), &fgcolor);
		gtk_color_button_set_color (GTK_COLOR_BUTTON (bg_colorbutton), &bgcolor);
	}

	xpos = 0;
	ypos = grid_rows-1;
	if ((newgame != 0) && pixmap) {
		g_object_unref (pixmap);
		gm_debug ("unreferencing pixmap\n");
		pixmap = NULL;
	}
	if ((!pixmap) || newgame != 0) {
		pixmap = gdk_pixmap_new (widget->window,
					 widget->allocation.width,
					 widget->allocation.height,
					 -1);

		cr = gdk_cairo_create (pixmap);

		gdk_cairo_set_source_color (cr, &bgcolor);
		cairo_rectangle (cr, 0, 0, widget->allocation.width, widget->allocation.height);
		cairo_fill (cr);
		cairo_destroy (cr);

		draw_main_grid (widget);
	}

	gdk_window_invalidate_rect (widget->window, NULL, FALSE); // ogni tanto va bene anche ridisegnare tutto

	return TRUE;
}



void new_game (void) {
	gint i;
	newgame = 1;
	for (i = 0; i < grid_rows; i++) 
		if (movearray[i]) g_free (movearray[i]);
	if (movearray) g_free (movearray);
	movearray = g_try_malloc (gc_max_tries * sizeof ( gint * ));
	if (movearray == FALSE) gm_debug ("alloc 1 failed\n");
	for (i = 0; i < gc_max_tries; i++) {
		movearray[i] = g_try_malloc ((GRID_COLS+2) * sizeof (gint));
		if (movearray[i] == FALSE) gm_debug ("alloc 2 failed\n");
	}
	start_new_gameboard (drawing_area);
	init_game();

	xpos = 0;
	ypos = grid_rows-1; 

	gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
	gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), _("Ready for a new game!"));

	newgame = 0;
}

/* Create callbacks that implement our Actions */
static void quit_action (void) {
	gtk_main_quit();
}

void win_dialog (int tries) {
	GtkWidget *dialog;
	GtkWidget *button;
 
	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_INFO,
						     GTK_BUTTONS_NONE,
						     _("<span size=\"large\" weight=\"bold\">Great!!!</span>\nYou found the solution with <b>%d</b> tries!\n" 
						       "Do you want to play again?"), tries);


	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_QUIT, GTK_RESPONSE_NO);
	button = gtk_dialog_add_button (GTK_DIALOG (dialog), _("Play again"), GTK_RESPONSE_YES);
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_stock (GTK_STOCK_YES, GTK_ICON_SIZE_BUTTON)); 

	
	gint response = gtk_dialog_run (GTK_DIALOG (dialog));
	if ( response == GTK_RESPONSE_YES)
		new_game();
	else quit_action();
	gtk_widget_destroy (dialog);
}

static void 
lose_dialog (int tries) {
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *image[4];
	GtkWidget *shbox;
	GdkPixbuf *sbuf = NULL;
	int i;

	GtkWidget *slabel;

	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_WARNING,
						     GTK_BUTTONS_NONE,
						     _("<span size=\"large\" weight=\"bold\">I'm sorry, you lose!</span>\n"
						       "With just <b>%d</b> tries you didn't find the solution yet?!\n"
						       "Do you want to play again?"), tries);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_QUIT, GTK_RESPONSE_NO);
	button = gtk_dialog_add_button (GTK_DIALOG (dialog), _("Play again"), GTK_RESPONSE_YES);
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_stock (GTK_STOCK_YES, GTK_ICON_SIZE_BUTTON)); 

	shbox = gtk_hbox_new (FALSE, 0);
	slabel = gtk_label_new (_("This was the right solution:"));
	gtk_box_pack_start (GTK_BOX (shbox), slabel, FALSE, FALSE, 20);
	for (i = 0; i < 4; i++) {
		sbuf = gdk_pixbuf_new_subpixbuf (tileset_bg,
						 ballmed*solution[i],
						 0,
						 ballmed, ballmed);
		image[i] = gtk_image_new_from_pixbuf (sbuf);
		g_object_unref (sbuf);
		gtk_box_pack_start (GTK_BOX (shbox), image[i], FALSE, FALSE, 0);
	}
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), shbox, FALSE, FALSE, 0);
	gtk_widget_show (shbox);
	gtk_widget_show_all (dialog);
	gint response = gtk_dialog_run (GTK_DIALOG (dialog));
	if ( response == GTK_RESPONSE_YES)
		new_game();
	else quit_action();
	gtk_widget_destroy (dialog);
}

/* Create a new backing pixmap of the appropriate size */
static gboolean configure_event ( GtkWidget *widget,
				  GdkEventConfigure *event ) {
	/* if (pixmap)
	   g_object_unref (pixmap); */
	// new_game = 0;
	gdouble gr1, gr2;
//	grid_xpad = (widget->allocation.width-GRID_SZ*(GRID_COLS+2))/2;
//	grid_ypad = (widget->allocation.height - tray_h - grid_sz*(grid_rows))/2;
	grid_ypad = GRID_YPAD;
	grid_xpad = GRID_XPAD;
	gr1 = (widget->allocation.height - (GRID_YPAD)*2) / (grid_rows+0.8);
	gr2 = (widget->allocation.width - GRID_XPAD*2) / (GRID_COLS+2);
	if (gr1 < gr2) {
		grid_sz = gr1;
	} else  {
		grid_sz = gr2;
	}

	tray_sz = grid_sz*0.8;
	tray_h = TRAY_ROWS*tray_sz+TRAY_PAD*2;

	grid_xpad = (widget->allocation.width - grid_sz * (GRID_COLS+2)) / 2;	
	grid_ypad = (widget->allocation.height - tray_h - grid_sz*grid_rows) / 2;
	
	ballmed = grid_sz;
	ballsm = ballmed*0.75;

//	grid_xpad = (widget->allocation.width-grid_sz*(GRID_COLS+2))/2;
//	grid_xpad = grid_sz;

	gm_debug ("1: %d 2:%d\n", grid_sz, grid_xpad);

	
	if (confcount == 0) {
		new_game();
	}
	else {
		old_xpos = xpos;
		old_ypos = ypos;
		newgame =1;
		start_new_gameboard (widget);
		newgame = 0;
		xpos = 0;
		ypos = grid_rows - 1; 

		redraw_current_game();
	}
	gm_debug ("\nconfigure event\n\n");
	confcount++;
// start_new_gameboard (widget);
	return TRUE;
}

void draw_score_pegs (int line, int b, int c, GtkWidget *widget) {
	int i;
	int x, y;
	int offset;

	GdkPixbuf *tmp;

	for (i = 0; i< b+c; i++) {
		if (pixbuf) 
			g_object_unref (pixbuf);
		if (i<b){ 
			offset = 0;
		}
		else {
			offset = 1;
		}
		pixbuf = gdk_pixbuf_new_subpixbuf (tileset_bg,
						   ballmed*8,
						   ballmed/2*offset,
						   ballmed/2, ballmed/2);
		tmp = pixbuf;

		pixbuf = gdk_pixbuf_scale_simple (pixbuf,
						  ballmed/2-1,
						  ballmed/2-1,
						  GDK_INTERP_BILINEAR);

		g_object_unref (tmp);

		x = grid_xpad + grid_sz * (GRID_COLS+1) + (grid_sz/2 * (i%2));

		y = grid_ypad + grid_sz * line + ((int)i/2)*grid_sz/2;
	       
		if (i>1) y-=0.5;

		gm_debug ("line: %d, i:%d\n",line,i);

		gdk_draw_pixbuf (pixmap,
				 NULL,
				 pixbuf,
				 0, 0,
				 x+1,
				 y+1,
				 -1, -1, GDK_RGB_DITHER_MAX, 0, 0);
		gtk_widget_queue_draw_area (widget, x, y, grid_sz/2, grid_sz/2);
	}
}

static gboolean clean_next_row (void) {
	cairo_t *cr;
	cr = gdk_cairo_create (pixmap);
	cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
	cairo_set_line_width (cr, 0);
	cairo_save (cr);
	cairo_rectangle (cr, 
			 grid_xpad+1,
			 grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos+1)))+1,
			 grid_sz*GRID_COLS-1,
			 grid_sz-2);

	rect.x = grid_xpad+1;
	rect.y = grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos+1)))+1;
	rect.width =  grid_sz*GRID_COLS-1;
	rect.height = grid_sz-2;

	gdk_window_invalidate_rect (drawing_area->window, &rect, FALSE);

	cairo_rectangle (cr, 
			 grid_xpad+grid_sz* (GRID_COLS+1)+1,
			 grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos+1)))+1, 
			 grid_sz-1, grid_sz-2);
	cairo_set_source_rgba (cr, 1, 1, 1, 0.35);
	cairo_fill_preserve (cr);
	cairo_stroke (cr); 
	cairo_destroy (cr);

	rect.x = grid_xpad+grid_sz* (GRID_COLS+1)+1;
	rect.y = grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos+1)))+1;
	rect.width = grid_sz - 1;
	rect.height = grid_sz - 2;

	gdk_window_invalidate_rect (drawing_area->window, &rect, FALSE);

	return TRUE;
}

static gboolean checkscores() {
	gchar *statusmessage;
	int i, j;
	gm_debug("[checkscores]\n");
	for (i = 0; i < GRID_COLS; i++) tmp[i] = solution[i];
	bulls = cows = 0;
	gm_debug ("solution: ");
	for (i = 0; i < GRID_COLS; i++) gm_debug ("%-d", solution[i]);
	gm_debug ("\n"); 
	gm_debug ("guess: ");
	for (i = 0; i < GRID_COLS; i++) gm_debug ("%-d", guess[i]);
	gm_debug ("\n");
	/* scan for bulls and disable them */
	for (i = 0; i < GRID_COLS; i++) {
		if (guess[i] == tmp[i]) {
			// gm_debug ("guess[%d] = %d tmp[%d] = %d\n", i, guess[i], i, tmp[i]);
			bulls++;
			tmp[i] = -1;
			guess[i] = -2;
		}
	}
	for (i = 0; i < GRID_COLS; i++)
		for (j = 0; j < GRID_COLS; j++) {
			if ((guess[i] == tmp[j])) {
				// gm_debug ("guess[%d] = %d tmp[%d] = %d\n", i, guess[i], j, tmp[j]);
				cows++;
				guess[i] = -2;
				tmp[j] = -1;
			}
		}
	draw_score_pegs (ypos, bulls, cows, drawing_area);
	gdk_window_process_all_updates(); 
       	/* Note for translators: the following tells the number of
	 *  right color and position guesses (bulls) and right color
	 *  only guessed (cows). "Bulls and cows" is a pen and paper
	 *  game similar to mastermind. I've leaved it untranslated in
	 *  italian. Translate it if you want or leave it untranslated
	 *  or feel free to ask my opinion */
	statusmessage = g_strdup_printf (_("%d bulls, %d cows!"), bulls, cows);
	gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
	gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), statusmessage);
	g_free (statusmessage);

	for (i = 0; i < GRID_COLS; i++) filled[i] = 0;
	movearray[ypos][GRID_COLS] = bulls;
	movearray[ypos][GRID_COLS+1] = cows;

	for (i = grid_rows-1; i >= 0; i--){
		for (j = 0; j<GRID_COLS+2; j++)
			gm_debug (" %d ", movearray[i][j]);
		gm_debug ("\n");
	}
	if (bulls == 4) {
		win_dialog (grid_rows-ypos);
		xpos = 0;
		ypos = grid_rows-1;
	} 
	else if (ypos == 0) {
		lose_dialog (grid_rows);
		xpos = 0;
		ypos = grid_rows-1;
	}
	else {
		clean_next_row();
		ypos--;
	}
	
	if (undolist) {
		undolist = g_list_first (undolist);
		g_list_foreach (undolist, (GFunc) g_free, NULL);
		g_list_free (undolist);
	}

	undolist = NULL;

	undolist = g_list_append (undolist, init_lastmove());

	gtk_action_set_sensitive (GTK_ACTION (undo_action), FALSE);
	gtk_action_set_sensitive (GTK_ACTION (redo_action), FALSE);
	
	return TRUE;
}

static gboolean place_grid_color (int c, int l) {
	gint x, y;
	
	gm_debug ("placing c:%d l:%d s:%d\n", c, l, selectedcolor);
	gm_debug ("movecount: %d\n", movecount);
	gridcl2xy (c, l, &x, &y, drawing_area);

	gdk_draw_drawable(pixmap,
			  drawing_area->style->white_gc,
			  cellbk,
			  0, 0,
			  x + 1, y + 1,
			  -1, -1);


	if (selectedcolor >=0 ) {
		gdk_draw_pixbuf (pixmap,
				 NULL,
				 tileset_bg,
				 ballmed*selectedcolor, 0,
				 x, y,
				 ballmed, ballmed, 
				 GDK_RGB_DITHER_MAX, 0, 0);
	}
	
	gtk_action_set_sensitive (GTK_ACTION (undo_action), TRUE);

	rect.x = x;
	rect.y = y;
	rect.width = ballmed;
	rect.height = ballmed;
	gdk_window_invalidate_rect (drawing_area->window, &rect, TRUE);

	return TRUE;
}


static void 
undo_action_cb (void) {

	if ((movecount == 0) || (xpos == 0)) {
		return;
	} 

	undolist = g_list_previous (undolist);
	lastmove = undolist->data;

	gm_debug ("POSITION: %d\n", g_list_position (g_list_first (undolist), undolist));
	
	gm_debug ("[undo] xpos:%d, ypos:%d, movecount:%d, selectedcolor:%d\n",
		  xpos, ypos, movecount, selectedcolor);
	old_xpos = 0;
	movecount--;

	for (xpos = 0; xpos < GRID_COLS; xpos ++) {
		selectedcolor = lastmove [xpos];
		movearray[ypos][xpos] = lastmove[xpos];
		filled[xpos] = 1; // set current position as filled
		guess[xpos] = selectedcolor; // fill guessed solution array with current color
		if (selectedcolor < 0) filled[xpos] = 0;
		else old_xpos++;
		place_grid_color (xpos, ypos);
	}

	xpos = old_xpos;
	
	gtk_action_set_sensitive (GTK_ACTION (redo_action), TRUE);
	
	if ((movecount == 0) || (xpos == 0))
		gtk_action_set_sensitive (GTK_ACTION (undo_action), FALSE);
}

static void 
redo_action_cb (void) {

	current = g_list_next (undolist);
	if (!current) {
		gm_debug ("che succede? qui non dovevi mica arrivarci!\n");
		return;
	}
	
	undolist = current;
	lastmove = undolist->data;

	gm_debug ("POSITION: %d\n", g_list_position (g_list_first (undolist), undolist));

	gm_debug ("[redo] xpos:%d, ypos:%d, movecount:%d, selectedcolor:%d\n",
		  xpos, ypos, movecount, selectedcolor);
	old_xpos = 0;
	movecount++;

	for (xpos = 0; xpos < GRID_COLS; xpos ++) {
		selectedcolor = lastmove [xpos];
		movearray[ypos][xpos] = lastmove[xpos];
		filled[xpos] = 1; // set current position as filled
		guess[xpos] = selectedcolor; // fill guessed solution array with current color
		if (selectedcolor < 0) filled[xpos] = 0;
		else old_xpos++;
		place_grid_color (xpos, ypos);
	}

	xpos = old_xpos;
	current = g_list_next (undolist);
	if (!current) {
		gm_debug ("end of the list\n");
		gtk_action_set_sensitive (GTK_ACTION (redo_action), FALSE);
	}

	gm_debug ("XPOS is %d\n", xpos);

}

static gboolean redraw_current_game() {
	int count = 0;
	int limit = 0;
	int dummy = 0;
 
	gm_debug ("here\n");
	if (movecount == 0) {
		return FALSE;
	}
/*
	for (ypos = grid_rows-1; ypos >= 0; ypos--) {
		for (xpos = 0; xpos<GRID_COLS+2; xpos++)
			gm_debug (" %d ", movearray[ypos][xpos]);
		gm_debug ("\n");
	}

*/
	ypos = grid_rows;

	limit = (movecount % GRID_COLS) ? 
		((int)movecount/GRID_COLS+1)*GRID_COLS : movecount;

	gm_debug ("movecount: %d limit:%d ypos: %d\n", movecount, limit, ypos);

	while (count < limit) {
		xpos = count % GRID_COLS;
		if (xpos == 0)
			ypos--;
		selectedcolor = movearray[ypos][xpos];
		if (selectedcolor < 0) dummy = 1;
	 
		if ((xpos == GRID_COLS-1) && (ypos>0) && !dummy) clean_next_row();

		if (selectedcolor >= 0)
			place_grid_color (xpos, ypos);

		if (xpos == GRID_COLS-1){
			gm_debug ("line is %d\n", ypos);
			bulls = movearray[ypos][GRID_COLS];
			cows = movearray[ypos][GRID_COLS+1];
			bulls = (bulls < 0) ? 0 : bulls;
			cows = (cows < 0) ? 0 : cows;
			draw_score_pegs (ypos, bulls, cows, drawing_area);
		}

		gm_debug ("xpos:%d ypos:%d\n", xpos, ypos);
		count++;
	}

	xpos = old_xpos;
	ypos = old_ypos;

	return TRUE;
}

static gboolean tray_mid_click();

static gboolean parse_tray_event (GdkEventButton *event, GtkWidget *widget)
{
	int x, y;
	int c, l;

	trayxy2cl (event->x, event->y, &c, &l, widget); //rescaling operations
	traycl2xy (c, l, &x, &y, widget);
	
	gm_debug("c: %d, l: %d\n", c, l);
	
	if ((c >= 0) && (l >= 0)) {
		pressed = 1;
		selectedcolor = c+ (l*TRAY_COLS);
		if (selectedcolor >= 0) 
			clean_tray(widget);
		gdk_draw_pixbuf (pixmap,
				 NULL,
				 tileset_bg,
				 ballmed * selectedcolor, 0,
				 x+ballsm/2-ballmed/2, y+ballsm/2-ballmed/2,
				 ballmed, ballmed, 
				 GDK_RGB_DITHER_MAX, 0, 0);
//		gdk_window_invalidate_rect (widget->window, NULL, FALSE); 

		gm_debug ("type: %d\n", event->type);

		if ((event->button == 2 && event->type == GDK_BUTTON_PRESS) || 
		    (event->button == 1 && event->type == GDK_2BUTTON_PRESS)) {
			tray_mid_click();
		}
	}
	
	return TRUE;
}

static gboolean motion_event (GtkWidget *widget,
			      GdkEventButton *event) {

	if (pressed && !mstarted) {
		gint x,y;
		traycl2xy(selectedcolor, 0, &x, &y, widget);
		motion_x_shift =  (x - event->x) - (ballmed - ballsm)/2;
		motion_y_shift = (y - event->y) - (ballmed - ballsm)/2;
		gm_debug("motion start\n");
/*
  gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
  gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), _("Drag the ball to an empty cell!"));

*/
		clean_tray(widget);
	}
	if (pressed) {
		if (motionbk) {
			gdk_draw_drawable(pixmap,
					  widget->style->white_gc,
					  motionbk,
					  0, 0,
					  xbk, ybk,
					  ballmed, ballmed);
			rect.x = xbk;
			rect.y = ybk;
			rect.width = ballmed;
			rect.height = ballmed;
			gdk_window_invalidate_rect (widget->window, &rect, FALSE);
			g_object_unref (motionbk);
			motionbk = NULL;
		}
		motionbk = gdk_pixmap_new (widget->window,
					   ballmed,
					   ballmed,
					   -1);
		gdk_draw_drawable(motionbk,
				  widget->style->white_gc,
				  pixmap,
				  event->x+motion_x_shift, event->y+motion_y_shift,
				  0, 0,
				  ballmed, ballmed);
		xbk = event->x+motion_x_shift;
		ybk = event->y+motion_y_shift;
		
		mstarted = 1;
		gdk_draw_pixbuf (pixmap,
				 NULL,
				 tileset_bg,
				 ballmed * selectedcolor, 0,
				 event->x+motion_x_shift, event->y+motion_y_shift,
				 ballmed, ballmed, 
				 GDK_RGB_DITHER_MAX, 0, 0);
		rect.x = xbk;
		rect.y = ybk;
		rect.width = ballmed;
		rect.height = ballmed;
		gdk_window_invalidate_rect (widget->window, &rect, FALSE);
//		gdk_window_invalidate_rect (widget->window, NULL, FALSE); 
	}

	return FALSE;
}

static gboolean button_press_event ( GtkWidget *widget,
				     GdkEventButton *event )
{
	gint c, l;
	gint i;

	if (event->type == GDK_BUTTON_RELEASE) {
		if (stx == event->x && sty == event->y) {
		}
		pressed = 0;
		if (mstarted) {
			if (motionbk) {
				gdk_draw_drawable(pixmap,
						  widget->style->white_gc,
						  motionbk,
						  0, 0, xbk, ybk, ballmed, ballmed);
				rect.x = xbk;
				rect.y = ybk;
				rect.width = ballmed;
				rect.height = ballmed;
				gdk_window_invalidate_rect (widget->window, &rect, FALSE);
				g_object_unref (motionbk);
				motionbk = NULL;
			}
			
			// FIXME  (duplicated code - move to function) //
			
			gridxy2cl (event->x, event->y, &c, &l, widget);
		      
			if (l == ypos) {
				clean_tray(widget);
				
				if(!filled[c]) {
					xpos++;
					xpos = xpos%GRID_COLS;
				}
				
				
				place_grid_color (c, l);
				
				gm_debug ("POSITION: %d\n", g_list_position (g_list_first (undolist), undolist));

				while ((current = g_list_next(undolist))) {
					gm_debug ("ooo\n");
					g_free (current->data);
					undolist = g_list_remove_link (undolist, current);
				}

				gtk_action_set_sensitive (GTK_ACTION (redo_action), FALSE);
				
				undolist = g_list_append (undolist, init_lastmove ());
				undolist = g_list_last (undolist);
				lastmove = undolist->data; 
				for (i = 0; i < GRID_COLS; i++) {
					lastmove[i] = movearray[ypos][i];
					gm_debug(":: %d ", lastmove[i]);
				}
				gm_debug ("\n");
				lastmove[c] = selectedcolor;

				movearray[l][c] = selectedcolor; 				
				movecount++; 				
				filled[c] = 1; // set current position as filled
				guess[c] = selectedcolor; // fill guessed solution array with current color
				
				gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
				gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), _("Select a color!"));
				
				gm_debug ("[button_press_event]xpos:%d ypos:%d\n", xpos, ypos);
				if (xpos == 0 && movecount > 1) {
					undolist = g_list_append (undolist, init_lastmove ());
					undolist = g_list_last (undolist);
					checkscores();
				}
				selectedcolor = -1;
			}
			else {
				gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
				gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), _("Select a valid place!"));
			}
			
			gm_debug("motion stop\n");
			selectedcolor = -1;
			mstarted = 0;
		}
	}

	if ((event->type != GDK_BUTTON_PRESS && event->type != GDK_2BUTTON_PRESS) || pixmap == NULL) 
		return TRUE;
	
	if (event->y > (widget->allocation.height - tray_h)) {
		stx = event->x;
		sty = event->y;
		parse_tray_event (event, widget);
	} 
       
	else 
		gm_debug ("parse_grid_event\n");

/* consider moving the rest in a separate function */
	if (event->button == 1 && pixmap != NULL && selectedcolor >= 0) {

		gridxy2cl (event->x, event->y, &c, &l, widget);

		if (l == ypos) {
			clean_tray (widget);
			if(!filled[c]) {
				xpos++;
				xpos = xpos%GRID_COLS;
			}

			place_grid_color (c, l);

			gm_debug ("POSITION: %d\n", g_list_position (g_list_first (undolist), undolist));
			while ((current = g_list_next(undolist))) {
				gm_debug ("ooo\n");
				g_free (current->data);
				undolist = g_list_remove_link (undolist, current);
			}

			gtk_action_set_sensitive (GTK_ACTION (redo_action), FALSE);
						
			undolist = g_list_append (undolist, init_lastmove ());
			undolist = g_list_last (undolist);
			lastmove = undolist->data; 
			for (i = 0; i < GRID_COLS; i++) {
				lastmove[i] = movearray[ypos][i];
				gm_debug(":: %d ", lastmove[i]);
			}
			gm_debug ("\n");
			lastmove[c] = selectedcolor;
			
			movearray[l][c] = selectedcolor;
			movecount++;
			filled[c] = 1; // set current position as filled
			guess[c] = selectedcolor; // fill guessed solution array with current color

			gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
			gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), _("Select a color!"));

			gm_debug ("[button_press_event]xpos:%d ypos:%d\n", xpos, ypos);
			if (xpos == 0 && movecount > 1) {
				checkscores();
			}
			selectedcolor = -1;
			
		}

		else {
			gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
			gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), _("Select a valid place!"));
		}

	}
	return TRUE;
}



static gboolean tray_mid_click(){
	gint c;
	gint i;
	gint found = 0;
	for (c = 0; c < GRID_COLS; c++) {
		if (!filled[c] && !found) { 
//	 gm_debug ("found %d\n", i);
			found = 1;

			xpos++;
			xpos = xpos%GRID_COLS;

//	 gm_debug ("i:%d ypos:%d c: %d l:%d\n", i, ypos, c, l);

			if (timeout_id > 0) {
				g_source_remove (timeout_id);
			}
			timeout_id = g_timeout_add (200, 
						    (GSourceFunc) clean_tray, 
						    drawing_area);

			place_grid_color (c, ypos);

			gm_debug ("POSITION: %d\n", g_list_position (g_list_first (undolist), undolist));
			while ((current = g_list_next(undolist))) {
				gm_debug ("ooo\n");
				g_free (current->data);
				undolist = g_list_remove_link (undolist, current);
			}

			gtk_action_set_sensitive (GTK_ACTION (redo_action), FALSE);

			undolist = g_list_append (undolist, init_lastmove ());
			undolist = g_list_last (undolist);
			lastmove = undolist->data;
			for (i = 0; i < GRID_COLS; i++) 
				lastmove[i] = movearray[ypos][i];
			lastmove[c] = selectedcolor;
			movearray[ypos][c] = selectedcolor;

			movecount++;

			filled[c] = 1; // set current position as filled

			guess[c] = selectedcolor; // fill guessed solution array with current color
			gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
			gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), _("Select a color!"));

			gm_debug ("[tray_mid_click]xpos:%d ypos:%d\n", xpos, ypos);
			if (xpos == 0 && movecount > 1) {
				undolist = g_list_append (undolist, init_lastmove ());
				undolist = g_list_last (undolist);
				checkscores();
			}
			selectedcolor = -1;
		}
	}
	return TRUE;
}

/* Redraw the screen from the backing pixmap */
static gboolean expose_event ( GtkWidget *widget,
			       GdkEventExpose *event )
{
	gdk_draw_drawable (widget->window,
			   widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			   pixmap,
			   event->area.x, event->area.y,
			   event->area.x, event->area.y,
			   event->area.width, event->area.height);



	return FALSE;
}

static gboolean delete_event (GtkWidget *widget, GdkEvent *event, gpointer data) {
	if (pixmap)
		g_object_unref (pixmap);
	if (traymapbk)
		g_object_unref (traymapbk);
	if (cellbk)
		g_object_unref (cellbk);
	if (motionbk)
		g_object_unref (motionbk);

	gtk_main_quit();
	return FALSE;
}

static void destroy (GtkWidget *widget, gpointer data) {
	gtk_main_quit();
}

static void help_action (void) {
#ifndef G_OS_WIN32
	gchar   *argv[] = { "yelp",
			    "ghelp:gnome-mastermind",
			    NULL };
	GError *error = NULL;

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
		       NULL, NULL, NULL, &error);
	if (error) 
	{
		g_message ("Error while launching yelp %s", error->message);
		g_error_free (error);
		error = NULL;
	}
#endif
}

void about_url_show (GtkAboutDialog *about,
		     const gchar *link,
		     gpointer data) {
	GError *error = NULL;
	gchar *launcher = NULL;

	if ((launcher = g_find_program_in_path("xdg-open"))) {} 
	else if ((launcher = g_find_program_in_path("gnome-open"))) {} 
	else if ((launcher = g_find_program_in_path("epiphany"))) {} 
	else if ((launcher = g_find_program_in_path("firefox"))) {} 
	else if ((launcher = g_find_program_in_path("konqueror"))) {} 
	else return;
		
	gchar   *argv[] = { launcher,
			    g_strdup(link),
			    NULL };

	gm_debug("%s: %s\n", launcher, link);

	g_spawn_async (NULL, argv, NULL, 
		       G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
		       NULL, NULL, NULL, &error);
	if (error) 
	{
		g_message ("Error while launching gnome-open %s", error->message);
		g_error_free (error);
		error = NULL;
	}
}

void about_email_show (GtkAboutDialog *about,
		       const gchar *link,
		       gpointer data) {
	GError *error = NULL;
	gchar *launcher = NULL;

	if ((launcher = g_find_program_in_path("xdg-open"))) {} 
	else if ((launcher = g_find_program_in_path("gnome-open"))) {} 
	else if ((launcher = g_find_program_in_path("evolution"))) {} 
	else if ((launcher = g_find_program_in_path("kmail"))) {} 
	else return;
		
	gchar   *argv[] = { launcher,
			    g_strdup_printf("mailto:%s",link),
			    NULL };

	gm_debug("%s: mailto:%s\n", launcher, link);

	g_spawn_async (NULL, argv, NULL, 
		       G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
		       NULL, NULL, NULL, &error);
	if (error) 
	{
		g_message ("Error while launching gnome-open %s", error->message);
		g_error_free (error);
		error = NULL;
	}
}

static void about_action (void) {
	gchar *authors[] = { "Filippo Argiolas <filippo.argiolas@gmail.com>", NULL };
	gchar *artists[] = { 
		"Filippo Argiolas <filippo.argiolas@gmail.com>, me ;)",
		"Ulisse Perusin <uli.peru@gmail.com>, for that beautiful icon!", "and..",
		"..some other people for their hints and suggestions", 
		"Isabella Piredda, grazie amore mio!", 
		"Enrica Argiolas, my lil sister and beta tester", NULL };
	

	gtk_show_about_dialog (GTK_WINDOW (window),
			       "name", "GNOME Mastermind",
			       "authors", authors,
			       "artists", artists,
			       /* Note for translators: Replace this
				* string with your name and email
				* address */
			       "translator-credits", _("Filippo Argiolas <filippo.argiolas@gmail.com>"),
			       "comments", _("A Mastermind clone for gnome"),
			       "copyright", "gnome-mastermind, copyright (c) 2007 Filippo Argiolas\n"
			       "mastermind, copyright (c) 1971 Invicta Plastics, Ltd. UK",
			       "version", VERSION,
			       "logo-icon-name", "gnome-mastermind",
			       "website", "http://www.autistici.org/gnome-mastermind/",
			       NULL);
}

static void theme_changed (GtkWidget *widget, void *data) {

	GList *theme_item;

	theme_item = g_list_nth (theme_list,
				 gtk_combo_box_get_active (GTK_COMBO_BOX (widget)));

	gconf_client_set_string (settings, "/apps/gnome-mastermind/theme", theme_item->data, NULL);
}

static void populate_theme_combo (GtkWidget *combo) {
	GDir *dir;
	gchar *dir_name;
	gchar *cur_theme;
	const gchar *item;

	gint index = 0;
	gint active = 0;

	if (theme_list) {
		g_list_foreach (theme_list, (GFunc) g_free, NULL);
		g_list_free (theme_list);
	}

	theme_list = NULL;

	cur_theme = g_strrstr (gc_theme, "/");
	cur_theme++;

	dir_name = g_strdup (PKGDATA_DIR "/themes");
	dir = g_dir_open(dir_name, 0, NULL);

	if (!dir) {
		g_free (dir_name);
		return;
	}
	while ((item = g_dir_read_name(dir)) != NULL) {

		if (! (g_strrstr (item, ".png") || 
		       g_strrstr (item, ".svg"))) {
			continue;
		}
	 
		theme_list = g_list_append (theme_list, g_strdup(item));

		if (!g_ascii_strcasecmp (item, cur_theme)) {
			active = index;
		}

		gm_debug ("%s\n", item);

		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), item);
	 
		index++;
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), active);

	g_dir_close (dir);

	g_free (dir_name);
}

static void use_style_toggled (GtkWidget *toggle, GtkWidget *table) {
	gboolean state;
	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
	gtk_widget_set_sensitive (table, !state);
	gconf_client_set_bool (settings, "/apps/gnome-mastermind/gtkstyle_colors", state, NULL);
}

static void fgcolorbutton_set (GtkWidget *widget, gpointer data) {
	GdkColor color;
	gchar *color_string;
 
	gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &color);

	color_string = g_strdup_printf ("#%04x%04x%04x", color.red,
					color.green, color.blue);
	gconf_client_set_string (settings, "/apps/gnome-mastermind/fgcolor", color_string, NULL);
}

static void bgcolorbutton_set (GtkWidget *widget, gpointer data) {
	GdkColor color;
	gchar *color_string;
 
	gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &color);
	color_string = g_strdup_printf ("#%04x%04x%04x", color.red, color.green, color.blue);

	gconf_client_set_string (settings, "/apps/gnome-mastermind/bgcolor", color_string, NULL);
}

static void preferences_action (void){
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *theme_combo;
	GtkWidget *color_table;
	GtkWidget *color_vbox;
	GtkWidget *frame;
	GtkWidget *align;
	GtkWidget *pango_label;

	GtkWidget *use_style_check;
	GtkWidget *max_tries_spin;
// GtkWidget *hbox;
 
	pref_dialog = gtk_dialog_new_with_buttons (_("Preferences"),
						   GTK_WINDOW (window),
						   GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
						   NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (pref_dialog), FALSE);

	gtk_dialog_add_button (GTK_DIALOG (pref_dialog),
			       GTK_STOCK_CLOSE,
			       GTK_RESPONSE_CLOSE);
	gtk_container_set_border_width (GTK_CONTAINER (pref_dialog), 4);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (pref_dialog)->vbox), 2);

	frame = gtk_frame_new (NULL);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	pango_label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (pango_label), _("<b>Tileset theme</b>"));
 
	gtk_frame_set_label_widget (GTK_FRAME (frame), pango_label);
 
	align = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 10, 0);

	table = gtk_table_new (1, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 10);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);

	label = gtk_label_new (_("Theme:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
			  (GtkAttachOptions) GTK_FILL, (GtkAttachOptions) 0, 0, 0);
	theme_combo = gtk_combo_box_new_text();
	populate_theme_combo (theme_combo);

	g_signal_connect (G_OBJECT (theme_combo), "changed",
			  G_CALLBACK (theme_changed), NULL);

	gtk_table_attach_defaults (GTK_TABLE (table), theme_combo, 1, 2, 0, 1);

	gtk_container_add (GTK_CONTAINER (align), table);
	gtk_container_add (GTK_CONTAINER (frame), align);

	gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (pref_dialog)->vbox),
				     frame);

	frame = gtk_frame_new (NULL);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	pango_label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (pango_label), _("<b>Color settings</b>"));
 
	gtk_frame_set_label_widget (GTK_FRAME (frame), pango_label);

	align = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 10, 0);

	color_table = gtk_table_new (2, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (color_table), 10);
	gtk_table_set_row_spacings (GTK_TABLE (color_table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (color_table), 12);

	label = gtk_label_new (_("Foreground Color:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	gtk_table_attach (GTK_TABLE (color_table), label, 0, 1, 0, 1,
			  (GtkAttachOptions) GTK_FILL, (GtkAttachOptions) 0, 0, 0);
	fg_colorbutton = gtk_color_button_new_with_color (&fgcolor);

	g_signal_connect (G_OBJECT (fg_colorbutton), "color-set",
			  G_CALLBACK (fgcolorbutton_set), &fgcolor);

	gtk_table_attach_defaults (GTK_TABLE (color_table), fg_colorbutton, 1, 2, 0, 1);
 
	label = gtk_label_new (_("Background Color:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	gtk_table_attach (GTK_TABLE (color_table), label, 0, 1, 1, 2,
			  (GtkAttachOptions) GTK_FILL, (GtkAttachOptions) 0, 0, 0);
	bg_colorbutton = gtk_color_button_new_with_color (&bgcolor);

	g_signal_connect (G_OBJECT (bg_colorbutton), "color-set",
			  G_CALLBACK (bgcolorbutton_set), &bgcolor);

	gtk_table_attach_defaults (GTK_TABLE (color_table), bg_colorbutton, 1, 2, 1, 2);

	use_style_check = gtk_check_button_new_with_label (_("Try to get colors from system theme"));

	gtk_container_set_border_width (GTK_CONTAINER (use_style_check), 10);

	g_signal_connect (G_OBJECT (use_style_check), "toggled",
			  G_CALLBACK (use_style_toggled), color_table);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_style_check),
				      gc_gtkstyle_colors);
 

	color_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start_defaults (GTK_BOX (color_vbox),
				     color_table);
	gtk_box_pack_start_defaults (GTK_BOX (color_vbox),
				     use_style_check);

	gtk_container_add (GTK_CONTAINER (align), color_vbox);
	gtk_container_add (GTK_CONTAINER (frame), align);

	gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (pref_dialog)->vbox),
				     frame);

	frame = gtk_frame_new (NULL);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	pango_label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (pango_label), _("<b>Game settings</b>"));
 
	gtk_frame_set_label_widget (GTK_FRAME (frame), pango_label);

	align = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 10, 0);

	table = gtk_table_new (1, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 10);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);

	label = gtk_label_new (_("Maximum number of tries allowed:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
			  (GtkAttachOptions) GTK_FILL, (GtkAttachOptions) 0, 0, 0);

	max_tries_spin = gtk_spin_button_new_with_range (2, 14, 1);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (max_tries_spin), gc_max_tries);

	gtk_table_attach_defaults (GTK_TABLE (table), max_tries_spin, 1, 2, 0, 1);

	gtk_container_add (GTK_CONTAINER (align), table);
	gtk_container_add (GTK_CONTAINER (frame), align);

	gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (pref_dialog)->vbox),
				     frame);

	gtk_widget_show_all (pref_dialog);

	gtk_dialog_run (GTK_DIALOG (pref_dialog));

	gconf_client_set_int (settings, "/apps/gnome-mastermind/maximum_tries",
			      gtk_spin_button_get_value (
				      GTK_SPIN_BUTTON (max_tries_spin)),
			      NULL);

/*
  else if (response == GTK_RESPONSE_YES) {
  reset_default_settings();
  }
*/ 

	gtk_widget_destroy (pref_dialog);
}

/* Create a list of entries which are passed to the Action constructor. 
 * This is a huge convenience over building Actions by hand. */
static GtkActionEntry entries[] = 
{
	{ "GameMenuAction", NULL, N_("_Game") }, /* name, stock id, label */

 
	{ "NewAction", GTK_STOCK_NEW,
	  N_("_New game"), "<control>N", 
	  N_("Start a new game"),
	  G_CALLBACK (new_game) },

	{ "UndoAction", GTK_STOCK_UNDO,
	  N_("_Undo Move"), "<control>Z", 
	  N_("Undo last move"),
	  G_CALLBACK (undo_action_cb) },
	
	{ "RedoAction", GTK_STOCK_REDO,
	  N_("_Redo Move"), "<shift><control>Z", 
	  N_("Redo last move"),
	  G_CALLBACK (redo_action_cb) },
 
	{ "QuitAction", GTK_STOCK_QUIT,
	  N_("_Quit"), "<control>Q", 
	  N_("Quit the game"),
	  G_CALLBACK (quit_action) },

	{ "SettingsMenuAction", NULL, N_("_Settings") },

	{ "PrefAction", GTK_STOCK_PREFERENCES,
	  N_("_Preferences"), "<control>P", 
	  N_("Change game settings"),
	  G_CALLBACK (preferences_action) },

	{ "HelpMenuAction", NULL, N_("_Help") },
 
	{ "AboutAction", GTK_STOCK_ABOUT,
	  N_("_About"), 
	  NULL,
	  N_("About"),
	  G_CALLBACK (about_action) },

	{ "HelpAction", GTK_STOCK_HELP, 
	  N_("_Contents"),
	  "F1",
	  N_("_Contents"),
	  G_CALLBACK (help_action) }
};

static void show_tb_callback (void) {
	gboolean state;

	state =
		gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (show_toolbar_action));
	
	gm_debug ("gc_show_toolbar: %d state: %d\n", gc_show_toolbar, state);
 
	gconf_client_set_bool (settings, "/apps/gnome-mastermind/show_toolbar", state , NULL);
}


static const GtkToggleActionEntry toggle_actions[] = {
	{"ShowToolbarAction", NULL, N_("_Show Toolbar"), NULL, N_("Show or hide the toolbar"),
	 G_CALLBACK (show_tb_callback)}
};

static guint n_entries = G_N_ELEMENTS (entries);

/* Implement a handler for GtkUIManager's "add_widget" signal. The UI manager
 * will emit this signal whenever it needs you to place a new widget it has. */
static void
menu_add_widget (GtkUIManager *ui, GtkWidget *widget, GtkContainer *container)
{
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
}

int main ( int argc, char *argv[] )
{
	int i;

	GtkWidget *gridframe;
	GtkActionGroup *action_group; 
	GtkUIManager *menu_manager; 
	GtkAccelGroup *accel_group;
	const gchar *debug_env = NULL;

	bindtextdomain (GETTEXT_PACKAGE, GMLOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE); 

	debug_env = g_getenv ("GM_DEBUG");
	if (debug_env && !g_ascii_strcasecmp(debug_env, "yes")) {
		gm_debug_on = TRUE; 
		gm_debug("*** DEBUG MODE ON ***\n");
	} else {
		gm_debug_on = FALSE;
	}

	gtk_init (&argc, &argv);
 
	gtk_window_set_default_icon_name("gnome-mastermind");
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "GNOME Mastermind");
 
	g_signal_connect (G_OBJECT (window), "delete_event",
			  G_CALLBACK (delete_event), NULL);
	g_signal_connect (G_OBJECT (window), "destroy",
			  G_CALLBACK (destroy), NULL); 

	vbox = gtk_vbox_new (FALSE, 0); 
// gtk_box_set_homogeneous (GTK_BOX (vbox), FALSE);
	action_group = gtk_action_group_new ("TestActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	menu_manager = gtk_ui_manager_new();

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_action_group_add_actions (action_group, entries, n_entries, NULL);
	gtk_action_group_add_toggle_actions (action_group, toggle_actions,
					     G_N_ELEMENTS (toggle_actions), NULL);

	gtk_ui_manager_insert_action_group (menu_manager, action_group, 0);

	gtk_ui_manager_add_ui_from_file (menu_manager, PKGDATA_DIR "/ui/ui.xml", &error);

	if (error)
	{
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
		error = NULL;
	}
	g_signal_connect 
		( 
			menu_manager, 
			"add_widget", 
			G_CALLBACK (menu_add_widget), 
			vbox
			);

	init_gconf();

	toolbar = gtk_ui_manager_get_widget (menu_manager, "/MainMenuBar");

	show_toolbar_action = gtk_action_group_get_action (action_group, "ShowToolbarAction");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (show_toolbar_action), gc_show_toolbar);

	undo_action = gtk_action_group_get_action (action_group, "UndoAction");
	gtk_action_set_sensitive (GTK_ACTION (undo_action), FALSE);
	redo_action = gtk_action_group_get_action (action_group, "RedoAction");
	gtk_action_set_sensitive (GTK_ACTION (redo_action), FALSE);
	

	accel_group = gtk_ui_manager_get_accel_group (menu_manager);
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

	drawing_area = gtk_drawing_area_new();

	g_signal_connect (G_OBJECT (drawing_area), "expose_event",
			  G_CALLBACK (expose_event), NULL);
	g_signal_connect (G_OBJECT (drawing_area), "configure_event",
			  G_CALLBACK (configure_event), NULL);

	gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK
			       | GDK_LEAVE_NOTIFY_MASK
			       | GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_BUTTON1_MOTION_MASK);

	g_signal_connect (G_OBJECT (drawing_area), "button_press_event",
			  G_CALLBACK (button_press_event), NULL);
	g_signal_connect (G_OBJECT (drawing_area), "button_release_event",
			  G_CALLBACK (button_press_event), NULL);
	g_signal_connect (G_OBJECT (drawing_area), "motion-notify-event",
			  G_CALLBACK (motion_event), NULL);



	gridframe = gtk_frame_new (NULL);

	grid_rows = gc_max_tries;

	movearray = g_try_malloc (grid_rows * sizeof ( gint * ));
	if (movearray == FALSE) gm_debug ("alloc 1 failed\n");
	for (i = 0; i < grid_rows; i++) {
		movearray[i] = g_try_malloc ((GRID_COLS+2) * sizeof (gint));
		if (movearray[i] == FALSE) gm_debug ("alloc 2 failed\n");
	}

	frame_min_w = GRID_SZ* (GRID_COLS+2) + GRID_XPAD*2;
	frame_min_h = GRID_SZ*grid_rows+2*GRID_YPAD+TRAY_ROWS*TRAY_SZ+TRAY_PAD*2;
	tray_h = TRAY_ROWS*TRAY_SZ+TRAY_PAD*2;

	init_game();

	gtk_container_add (GTK_CONTAINER (gridframe), drawing_area);

	status = gtk_statusbar_new();
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (status), FALSE);

	gtk_box_pack_end (GTK_BOX (vbox), status, FALSE, FALSE, 0);

	gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
	gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), _("Select a color!"));

	gtk_box_pack_end (GTK_BOX (vbox), gridframe, TRUE, TRUE, 0);

//	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

	gtk_widget_show_all (window);

	if (!gconf_client_get_bool
	    (settings, "/apps/gnome-mastermind/show_toolbar", NULL)) {
		gtk_widget_hide (toolbar);
		gtk_window_resize (GTK_WINDOW(window), 1, 1);
	}

	gtk_about_dialog_set_url_hook (about_url_show, NULL, NULL);
	gtk_about_dialog_set_email_hook (about_email_show, NULL, NULL);
 
	gtk_main();

	return 0;
}

