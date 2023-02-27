/*
 * GNOME Mastermind
 *
 * Authors:
 *  Filippo Argiolas <filippo.argiolas@gmail.com>  (2008)
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
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo/cairo.h>

#include "gnome-mastermind.h"

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
GAction *new_action;
GAction *quit;
GAction *show_toolbar_action;
GAction *undo_action;
GAction *redo_action;

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
static cairo_surface_t *pixmap = NULL;
static cairo_surface_t *traymapbk = NULL; // quick way to store clean tray
static cairo_surface_t *motionbk = NULL; // save pixmap to redrawing it after motion
static cairo_surface_t *cellbk = NULL;
static GdkPixbuf *pixbuf = NULL;
static GdkRectangle rect;
static GError *error = 0;

static GdkPixbuf *tileset_bg = NULL; /* main tileset */
static GdkPixbuf *tileset_sm = NULL; /* small tileset */

GList *theme_list = NULL;

GdkRGBA fgcolor;
GdkRGBA bgcolor;

GSettings *settings;

GtkWidget *pref_dialog = NULL;

GtkWidget *fg_colorbutton = NULL;
GtkWidget *bg_colorbutton = NULL;

static gboolean redraw_current_game();
gboolean start_new_gameboard (GtkWidget *widget);
void new_game (GSimpleAction *action, GVariant *param, gpointer data);

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

static void
max_tries_notify_func (GSettings *settings,
                       gchar *key,
                       gpointer user_data)
{
  GtkWidget *dialog;
  gc_max_tries = g_settings_get_int (settings, key);
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
      g_action_activate (new_action, NULL);
      frame_min_w = GRID_SZ* (GRID_COLS+2) + GRID_XPAD*2;
      frame_min_h = GRID_SZ*grid_rows+2*GRID_YPAD+TRAY_ROWS*TRAY_SZ+TRAY_PAD*2;
      gtk_widget_set_size_request (drawing_area, frame_min_w, frame_min_h);
      gtk_window_resize (GTK_WINDOW(window), 1, 1);

    }

    gtk_widget_destroy (dialog);
  }
}

static void
theme_notify_func (GSettings *settings,
                   gchar *key,
                   gpointer user_data)
{
  GtkWidget *dialog;
  gchar *value;
  value = g_settings_get_string (settings, key);
  gc_theme = g_strdup_printf ("%s%s%s", PKGDATA_DIR, "/themes/", value);
  g_free (value);
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
    g_settings_set_string (settings, "theme", "simple.svg");
    g_free (gc_theme);
    gc_theme = g_strdup_printf ("%s%s", PKGDATA_DIR, "/themes/simple.svg");
  }

  old_xpos = xpos;
  old_ypos = ypos;
  selectedcolor = -1;
  gtk_widget_queue_resize (drawing_area);
}

static void
color_notify_func (GSettings *settings,
                   gchar *key,
                   gpointer user_data)
{
  if (!g_ascii_strcasecmp ("bgcolor", key)) {
    g_free (gc_bgcolor);
    gc_bgcolor = g_settings_get_string (settings, key);
  }
  else if (!g_ascii_strcasecmp ("fgcolor", key)) {
    g_free (gc_fgcolor);
    gc_fgcolor = g_settings_get_string (settings, key);
  }
  else if (!g_ascii_strcasecmp ("gtkstyle-colors", key))
    gc_gtkstyle_colors = g_settings_get_boolean (settings, key);

  old_xpos = xpos;
  old_ypos = ypos;
  gtk_widget_queue_resize (drawing_area);
}

static void
toolbar_notify_func (GSettings *settings,
                     gchar *key,
                     gpointer user_data)
{
  gint cw, ch;
  gboolean vis = FALSE;

  gc_show_toolbar = g_settings_get_boolean (settings, key);
  g_simple_action_set_state (G_SIMPLE_ACTION (show_toolbar_action),
                             g_variant_new_boolean (gc_show_toolbar));

  gtk_window_get_size (GTK_WINDOW (window), &cw, &ch);

  vis = gtk_widget_get_visible (toolbar);

  if (gc_show_toolbar && !vis) {
    gtk_widget_show (toolbar);
    ch += gtk_widget_get_allocated_height (toolbar);
  }
  else if (!gc_show_toolbar && vis) {
    gtk_widget_hide (toolbar);
    ch -= gtk_widget_get_allocated_height (toolbar);
  }

  gtk_window_resize (GTK_WINDOW (window), cw, ch);
}

void init_gconf (void) {
  gchar *tmp;
  gchar *tmp2;
  GtkWidget *dialog;

  settings = g_settings_new ("org.fargiolas.gnome-mastermind");

  tmp = g_settings_get_string (settings, "theme");
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
    g_settings_set_string (settings, "theme", "simple.svg");
    gc_theme = g_strdup_printf ("%s%s", PKGDATA_DIR, "/themes/simple.svg");
  }
  g_free (tmp);
  g_free (tmp2);
/*	 gc_small_size = gconf_client_get_int (settings, "/apps/gnome-mastermind/small_ball_size", NULL);
         gc_big_size = gconf_client_get_int (settings, "/apps/gnome-mastermind/big_ball_size", NULL); */
  gc_max_tries = g_settings_get_int (settings, "maximum-tries");
  gc_gtkstyle_colors = g_settings_get_boolean (settings, "gtkstyle-colors");
  gc_bgcolor = g_settings_get_string (settings, "bgcolor");
  gc_fgcolor = g_settings_get_string (settings, "fgcolor");
  gc_show_toolbar = g_settings_get_boolean (settings, "show-toolbar");
  gm_debug ("settings: \n");
  gm_debug ("theme: %s\n", gc_theme);
  gm_debug ("gc_max_tries: %d\n", gc_max_tries);
  gm_debug ("gc_gtkstyle_colors: %d\n", gc_gtkstyle_colors);
  gm_debug ("gc_fgcolor: %s\n", gc_fgcolor);
  gm_debug ("gc_bgcolor: %s\n", gc_bgcolor);
  gm_debug ("gc_show_toolbar: %d\n", gc_show_toolbar);
  g_signal_connect (settings, "changed::maximum-tries",
                    G_CALLBACK (max_tries_notify_func), NULL);
  g_signal_connect (settings, "changed::theme",
                    G_CALLBACK (theme_notify_func), NULL);
  g_signal_connect (settings, "changed::gtkstyle-colors",
                    G_CALLBACK (color_notify_func), NULL);
  g_signal_connect (settings, "changed::bgcolor",
                    G_CALLBACK (color_notify_func), NULL);
  g_signal_connect (settings, "changed::fgcolor",
                    G_CALLBACK (color_notify_func), NULL);
  g_signal_connect (settings, "changed::show-toolbar",
                    G_CALLBACK (toolbar_notify_func), NULL);
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

  g_simple_action_set_enabled (G_SIMPLE_ACTION (undo_action), FALSE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (redo_action), FALSE);

  selectedcolor = -1;
// gc_theme = g_strdup_printf ("%s%s", PKGDATA_DIR, "/themes/simple");
// gm_debug ("%s\n", gc_theme);

  rand = g_rand_new();
  for (i = 0; i < 4; i++) {
    solution[i] = g_rand_int_range (rand, 0, TRAY_ROWS*TRAY_COLS);
    filled[i] = 0;
  }
#ifdef SSHOT
  solution [0] = 5;
  solution [1] = 1;
  solution [2] = 7;
  solution [3] = 6;
#endif /* SSHOT */
  g_free (rand);
}


void traycl2xy (int c, int l, int *x, int *y, GtkWidget *widget) {
  *x = gtk_widget_get_allocated_width (widget) / 2
    - tray_sz*TRAY_COLS/2+tray_sz*c+1; // questo +1 mi sa che serve perchè
  *y = gtk_widget_get_allocated_height (widget)
    - tray_h/2-tray_sz*TRAY_ROWS/2+tray_sz*l; // le palline non sono centrate
}

void gridcl2xy (int c, int l, int *x, int *y, GtkWidget *widget) {
  gm_debug ("!!! c:%d l:%d\n", c, l);
  *x = grid_xpad+grid_sz*c;
  *y = grid_ypad+grid_sz*l;
}

void trayxy2cl (int x, int y, int *c, int *l, GtkWidget *widget) {
  int x1, y1;
  x1 = x - (gtk_widget_get_allocated_width (widget)/2
            - tray_sz*TRAY_COLS/2);
  y1 = y - (gtk_widget_get_allocated_height (widget)
            - tray_h/2-tray_sz*TRAY_ROWS/2);
  if ((x1 >= 0) &&
      (x1 <= tray_sz*TRAY_COLS) &&
      (y1 >= 0) &&
      (y1 <= tray_sz*TRAY_ROWS) &&
      ((x1%tray_sz) != 0) &&
      ((y1%tray_sz) != 0)) {
    *c= (int) x1 / tray_sz;
    *l= (int) y1 / tray_sz;
  } else *c = *l = -1;
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
  } else *c = *l = -1;
}

static gboolean clean_tray (GtkWidget *widget) {
  GdkWindow *window;
  cairo_t *cr;
  gint width, height;

  timeout_id = 0;

  if(!pixmap || !traymapbk)
    return FALSE;
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  cr = cairo_create (pixmap);
  cairo_set_source_surface (cr, traymapbk, 0, height - tray_h);
  cairo_rectangle (cr, 0, height - tray_h, width, tray_h);
  cairo_fill (cr);
  cairo_destroy (cr);
  rect.x = 0;
  rect.y = height - tray_h;
  rect.width = width;
  rect.height = tray_h;
  window = gtk_widget_get_window (widget);
  gdk_window_invalidate_rect (window, &rect, FALSE);
  return FALSE;
}

void draw_main_grid (GtkWidget *widget) {

  gint i, j;
  gint x, y;
  cairo_t *cr;
  GdkWindow *window;

  gdouble wah, waw;

  waw = gtk_widget_get_allocated_width (widget);
  wah = gtk_widget_get_allocated_height (widget);

  cr = cairo_create (pixmap);
  window = gtk_widget_get_window (widget);

  cairo_save (cr);

  gdk_cairo_set_source_rgba (cr, &fgcolor);
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
//	cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_width (cr, 1);
  for (i = 0; i <= GRID_COLS; i++) {
    cairo_move_to (cr,
                   grid_xpad+grid_sz*i,
                   grid_ypad);
    cairo_line_to (cr,
                   grid_xpad+grid_sz*i,
                   grid_ypad+grid_sz*grid_rows);
  }
  for (i = 0; i <= 2; i++) {
    cairo_move_to (cr,
                   grid_xpad+grid_sz*(GRID_COLS+1)+grid_sz*i/2,
                   grid_ypad);
    cairo_line_to (cr,
                   grid_xpad+grid_sz* (GRID_COLS+1)+grid_sz*i/2,
                   grid_ypad+grid_sz*grid_rows);

  }

/* ci siamo la chiave sta tutta in quel 0.5!!!! */

  cairo_stroke (cr);
  cairo_set_line_width (cr, 2);
  for (j = 0; j <= grid_rows; j++) {
    cairo_move_to (cr,
                   grid_xpad-0.5,
                   grid_ypad+grid_sz*j);
    cairo_line_to (cr,
                   grid_xpad+grid_sz*GRID_COLS+0.5,
                   grid_ypad+grid_sz*j);

  }
  cairo_stroke (cr);

  for (j = 0; j <= grid_rows*2; j++) {
    if (j%2)
      cairo_set_line_width (cr, 1);
    else
      cairo_set_line_width (cr, 2);
    cairo_move_to (cr,
                   grid_xpad+grid_sz* (GRID_COLS+1)-0.5,
                   grid_ypad+grid_sz*j/2);
    cairo_line_to (cr,
                   grid_xpad+grid_sz* (GRID_COLS+2)+0.5,
                   grid_ypad+grid_sz*j/2);
    cairo_stroke (cr);
  }
  /* brighten grid */

  cairo_set_operator (cr, CAIRO_OPERATOR_DIFFERENCE);
  cairo_set_line_width (cr, 0);
  cairo_rectangle (cr, grid_xpad+0.5, grid_ypad+1, grid_sz*GRID_COLS-1, grid_sz* (grid_rows)-2);
  cairo_rectangle (cr, grid_xpad+grid_sz* (GRID_COLS+1)+0.5, grid_ypad+1, grid_sz-1, grid_sz* (grid_rows)-2);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.3);
  cairo_fill (cr);
  cairo_stroke (cr);

  /* darken inactive cells */

  cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
  cairo_set_line_width (cr, 0);
  cairo_rectangle (cr, grid_xpad+0.5, grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos)))+1, grid_sz*GRID_COLS-1, grid_sz-2);
  cairo_rectangle (cr, grid_xpad+grid_sz* (GRID_COLS+1)+0.5, grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos)))+1, grid_sz-1, grid_sz-2);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.35);
  cairo_fill (cr);
  cairo_stroke (cr);

  /* new tray drawings */

  cairo_restore (cr);
  cairo_save (cr);

  cairo_set_line_width (cr, 0);
  gdk_cairo_set_source_rgba (cr, &bgcolor);
  cairo_rectangle (cr,
                   grid_xpad-grid_sz,
                   wah - tray_h,
                   waw-grid_xpad*2+grid_sz*2, tray_h);
  cairo_fill_preserve (cr);

  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
  cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.25);

  cairo_move_to (cr, 3 + grid_xpad - grid_sz, wah - tray_h + 2);
  cairo_rel_curve_to (cr,
                      0, 0,
                      0, tray_h / 2,
                      30, tray_h / 2);
  cairo_rel_line_to (cr, waw-66-grid_xpad*2+grid_sz*2, 0);
  cairo_rel_curve_to (cr,
                      0, 0,
                      30, 0,
                      30, - tray_h / 2);
  cairo_rel_line_to (cr, 0, tray_h - 5);
  cairo_rel_line_to (cr, -waw+6+grid_xpad*2-grid_sz*2, 0);
  cairo_close_path (cr);

  cairo_fill (cr);
  cairo_stroke (cr);

  /* draw border */
  cairo_restore (cr);
  gdk_cairo_set_source_rgba (cr, &fgcolor);
  cairo_rectangle (cr, 1 + grid_xpad - grid_sz,
                   wah - tray_h,
                   waw - 2 - grid_xpad*2 + grid_sz*2,
                   tray_h - 1);
  cairo_set_line_width (cr, 2);
  cairo_stroke (cr);

  for (j = 0; j < TRAY_ROWS; j++)
    for (i = 0; i < TRAY_COLS; i++) {
      traycl2xy (i, j, &x, &y, widget);
      gdk_cairo_set_source_pixbuf (cr, tileset_sm,
                                   x - ballsm
                                   * (i + (j * TRAY_COLS)),
                                   y);
      cairo_rectangle (cr, x, y, ballsm, ballsm);
      cairo_fill (cr);
    }
  cairo_destroy (cr);
  if (traymapbk) cairo_surface_destroy (traymapbk);

  traymapbk
    = gdk_window_create_similar_surface (window,
                                         CAIRO_CONTENT_COLOR_ALPHA,
                                         waw, tray_h);
  cr = cairo_create (traymapbk);
  cairo_set_source_surface (cr, pixmap, 0, tray_h - wah);
  cairo_paint (cr);
  cairo_destroy (cr);

  gridcl2xy (0, grid_rows-1, &x, &y, drawing_area);

  if (cellbk) cairo_surface_destroy (cellbk);
  cellbk = gdk_window_create_similar_surface (window,
                                              CAIRO_CONTENT_COLOR_ALPHA,
                                              ballmed - 2, ballmed - 2);

  cr = cairo_create (cellbk);
  cairo_set_source_surface (cr, pixmap, -x - 1, -y - 1);
  cairo_rectangle (cr, 0, 0, ballmed - 1, ballmed - 1);
  cairo_fill (cr);
  cairo_destroy (cr);
}

gboolean start_new_gameboard (GtkWidget *widget) {

  cairo_t *cr;
  GdkWindow *window;
  GtkStyleContext *ctxt;
  gint w, h;

  window = gtk_widget_get_window (widget);

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

    ctxt = gtk_widget_get_style_context (widget);
    gtk_style_context_get_color (ctxt, GTK_STATE_FLAG_SELECTED,
                                 &bgcolor);

    fgcolor = bgcolor;
    bgcolor.red =  (bgcolor.red + 6.2) / 8;
    bgcolor.green = (bgcolor.green + 6.2) / 8;
    bgcolor.blue = (bgcolor.blue +  6.2) / 8;

    /* bordo delle griglie */

    fgcolor.red = fgcolor.red / 2 ;
    fgcolor.green = fgcolor.green / 2;
    fgcolor.blue = fgcolor.blue / 2;
  } else {
    gdk_rgba_parse (&bgcolor, gc_bgcolor);
    gdk_rgba_parse (&fgcolor, gc_fgcolor);
  }

  if (pref_dialog) {
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (fg_colorbutton), &fgcolor);
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (bg_colorbutton), &bgcolor);
  }

  xpos = 0;
  ypos = grid_rows-1;
  if ((newgame != 0) && pixmap) {
    cairo_surface_destroy (pixmap);
    gm_debug ("unreferencing pixmap\n");
    pixmap = NULL;
  }
  if ((!pixmap) || newgame != 0) {
    w = gtk_widget_get_allocated_width (widget);
    h = gtk_widget_get_allocated_height (widget);
    pixmap = gdk_window_create_similar_surface (window,
                                                CAIRO_CONTENT_COLOR_ALPHA,
                                                w, h);

    cr = cairo_create (pixmap);
    cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);

    gdk_cairo_set_source_rgba (cr, &bgcolor);
    cairo_rectangle (cr, 0, 0, w, h);
    cairo_fill (cr);
    cairo_destroy (cr);

    draw_main_grid (widget);
  }

  gdk_window_invalidate_rect (window, NULL, FALSE); // ogni tanto va bene anche ridisegnare tutto

  return TRUE;
}



void new_game (GSimpleAction *action, GVariant *param, gpointer data) {
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
#ifdef SSHOT
  GdkEventKey k;
  gboolean dummy;

  k.type = GDK_KEY_PRESS;
  k.keyval = GDK_1;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_1;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_3;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_5;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_7;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_8;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_2;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_2;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_2;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_2;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_8;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_7;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_2;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_2;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_6;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_7;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_6;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_2;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_8;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);
  k.keyval = GDK_7;
  g_signal_emit_by_name (drawing_area, "key-press-event", &k, &dummy);

#endif /* SSHOT */
  newgame = 0;
}

/* Create callbacks that implement our Actions */
static void quit_action (GSimpleAction *action, GVariant *par, gpointer data) {
  GApplication *app = data;

  g_object_unref (settings);
  g_application_quit (app);
}

void win_dialog (int tries) {
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_NONE,
                                               ngettext(
                                                 "<span size=\"large\" weight=\"bold\">Great!!!</span>\nYou found the solution with <b>%d</b> try!\n"
                                                 "Do you want to play again?",
                                                 "<span size=\"large\" weight=\"bold\">Great!!!</span>\nYou found the solution with <b>%d</b> tries!\n"
                                                 "Do you want to play again?", tries
                                                 ), tries);


  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Quit"), GTK_RESPONSE_NO);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("Play Again"), GTK_RESPONSE_YES);


  gint response = gtk_dialog_run (GTK_DIALOG (dialog));
  if ( response == GTK_RESPONSE_YES)
    g_action_activate (new_action, NULL);
  else g_action_activate (quit, NULL);
  gtk_widget_destroy (dialog);
}

static void
lose_dialog (int tries) {
  GtkWidget *dialog;
  GtkWidget *image[4];
  GtkWidget *shbox;
  GdkPixbuf *sbuf = NULL;
  int i;

  GtkWidget *slabel;

  dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_WARNING,
                                               GTK_BUTTONS_NONE,
                                               ngettext(
                                                 "<span size=\"large\" weight=\"bold\">I'm sorry, you lose!</span>\n"
                                                 "With just <b>%d</b> try you didn't find the solution yet?!\n"
                                                 "Do you want to play again?",
                                                 "<span size=\"large\" weight=\"bold\">I'm sorry, you lose!</span>\n"
                                                 "With just <b>%d</b> tries you didn't find the solution yet?!\n"
                                                 "Do you want to play again?", tries
                                                 ), tries);

  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Quit"), GTK_RESPONSE_NO);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("Play Again"), GTK_RESPONSE_YES);

  shbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
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
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), shbox, FALSE, FALSE, 0);
  gtk_widget_show (shbox);
  gtk_widget_show_all (dialog);
  gint response = gtk_dialog_run (GTK_DIALOG (dialog));
  if ( response == GTK_RESPONSE_YES)
    g_action_activate (new_action, NULL);
  else g_action_activate (quit, NULL);
  gtk_widget_destroy (dialog);
}

/* Create a new backing pixmap of the appropriate size */
static gboolean configure_event ( GtkWidget *widget,
                                  GdkEventConfigure *event ) {
  /* if (pixmap)
     g_object_unref (pixmap); */
  // new_game = 0;
  gdouble gr1, gr2;
  gint width, height;
//	grid_xpad = (widget->allocation.width-GRID_SZ*(GRID_COLS+2))/2;
//	grid_ypad = (widget->allocation.height - tray_h - grid_sz*(grid_rows))/2;
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  grid_ypad = GRID_YPAD;
  grid_xpad = GRID_XPAD;
  gr1 = (height - (GRID_YPAD)*2) / (grid_rows+0.8);
  gr2 = (width - GRID_XPAD*2) / (GRID_COLS+2);
  if (gr1 < gr2) {
    grid_sz = gr1;
  } else  {
    grid_sz = gr2;
  }

  tray_sz = grid_sz*0.8;
  tray_h = TRAY_ROWS*tray_sz+TRAY_PAD*2;

  grid_xpad = (width - grid_sz * (GRID_COLS+2)) / 2;
  grid_ypad = (height - tray_h - grid_sz*grid_rows) / 2;

  ballmed = grid_sz;
  ballsm = ballmed*0.75;

//	grid_xpad = (widget->allocation.width-grid_sz*(GRID_COLS+2))/2;
//	grid_xpad = grid_sz;

  gm_debug ("1: %d 2:%d\n", grid_sz, grid_xpad);


  if (confcount == 0) {
    g_action_activate (new_action, NULL);
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
  cairo_t *cr;

  cr = cairo_create (pixmap);

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

// anche qui?
    x = grid_xpad + grid_sz * (GRID_COLS+1) + (grid_sz/2 * (i%2));

    y = grid_ypad + grid_sz * line + ((int)i/2)*grid_sz/2;

    if (i>1) y-=0.5;

    gm_debug ("line: %d, i:%d\n",line,i);

    gdk_cairo_set_source_pixbuf (cr, pixbuf, x + 1, y + 1);
    cairo_paint (cr);
    gtk_widget_queue_draw_area (widget, x, y, grid_sz/2, grid_sz/2);
  }
  cairo_destroy (cr);
}

static gboolean clean_next_row (void) {
  cairo_t *cr;
  GdkWindow *window;
  cr = cairo_create (pixmap);
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
  cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
  cairo_set_line_width (cr, 0);
  cairo_save (cr);
  cairo_rectangle (cr,
                   grid_xpad+0.5,
                   grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos+1)))+1,
                   grid_sz*GRID_COLS-1,
                   grid_sz-2);

  rect.x = grid_xpad;
  rect.y = grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos+1)))+1;
  rect.width =  grid_sz*GRID_COLS;
  rect.height = grid_sz-2;

  window = gtk_widget_get_window (drawing_area);
  gdk_window_invalidate_rect (window, FALSE, FALSE);

  cairo_rectangle (cr,
                   grid_xpad+grid_sz* (GRID_COLS+1)+0.5,
                   grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos+1)))+1,
                   grid_sz-1,
                   grid_sz-2);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.35);
  cairo_fill_preserve (cr);
  cairo_stroke (cr);
  cairo_destroy (cr);

  rect.x = grid_xpad+grid_sz* (GRID_COLS+1);
  rect.y = grid_ypad+ (grid_sz* (grid_rows- (grid_rows-ypos+1)))+1;
  rect.width = grid_sz;
  rect.height = grid_sz - 2;

  gdk_window_invalidate_rect (window, &rect, FALSE);

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
  /* Note for translators: the following tells the number of
   *  right color and position guesses (bulls) and right color
   *  only guessed (cows). "Bulls and cows" is a pen and paper
   *  game similar to mastermind. I've leaved it untranslated in
   *  italian. Translate it if you want or leave it untranslated
   *  or feel free to ask my opinion */
  gchar * bmsg = g_strdup_printf(ngettext("%d bull, ", "%d bulls, ", bulls), bulls);
  gchar * cmsg = g_strdup_printf(ngettext("%d cow!", "%d cows!", cows), cows);

  statusmessage = g_strconcat (bmsg, cmsg, NULL);
  g_free (bmsg);
  g_free (cmsg);
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
#ifndef SSHOT
    win_dialog (grid_rows-ypos);
#endif /* SSHOT */
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

  g_simple_action_set_enabled (G_SIMPLE_ACTION (undo_action), FALSE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (redo_action), FALSE);

  return TRUE;
}

static gboolean place_grid_color (int c, int l) {
  cairo_t *cr;
  gint x, y;

  gm_debug ("placing c:%d l:%d s:%d\n", c, l, selectedcolor);
  gm_debug ("movecount: %d\n", movecount);
  gridcl2xy (c, l, &x, &y, drawing_area);

  cr = cairo_create (pixmap);
  cairo_set_source_surface (cr, cellbk, x + 1, y + 1);
  cairo_paint (cr);

  if (selectedcolor >=0 ) {
    gdk_cairo_set_source_pixbuf (cr, tileset_bg,
                                 x - ballmed * selectedcolor, y);
    cairo_rectangle (cr, x, y, ballmed, ballmed);
    cairo_fill (cr);
  }

  g_simple_action_set_enabled (G_SIMPLE_ACTION (undo_action), TRUE);

  rect.x = x;
  rect.y = y;
  rect.width = ballmed;
  rect.height = ballmed;
  gdk_window_invalidate_rect (gtk_widget_get_window (drawing_area),
                              &rect, TRUE);
  cairo_destroy (cr);

  return TRUE;
}


static void
undo_action_cb (GSimpleAction *action, GVariant *param, gpointer data) {

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

  g_simple_action_set_enabled (G_SIMPLE_ACTION (redo_action), TRUE);

  if ((movecount == 0) || (xpos == 0))
    g_simple_action_set_enabled (action, FALSE);
}

static void
redo_action_cb (GSimpleAction *action, GVariant *param, gpointer data) {

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
    g_simple_action_set_enabled (action, FALSE);
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
  cairo_t *cr;

  trayxy2cl (event->x, event->y, &c, &l, widget); //rescaling operations
  traycl2xy (c, l, &x, &y, widget);

  gm_debug("c: %d, l: %d\n", c, l);

  if ((c >= 0) && (l >= 0)) {
    pressed = 1;
    selectedcolor = c+ (l*TRAY_COLS);
    if (selectedcolor >= 0)
      clean_tray(widget);
    cr = cairo_create (pixmap);
    gdk_cairo_set_source_pixbuf (cr, tileset_bg,
                                 x + ballsm / 2 - ballmed / 2
                                 - ballmed * selectedcolor,
                                 y + ballsm / 2 - ballmed / 2);
    cairo_rectangle (cr, x + ballsm / 2 - ballmed / 2,
                     y + ballsm / 2 - ballmed / 2,
                     ballmed, ballmed);
    cairo_fill (cr);
    cairo_destroy (cr);
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
                              GdkEventMotion *event) {
  GdkWindow *window;
  cairo_t *cr;

  if (selectedcolor < 0)
    return TRUE;
  if (pressed && !mstarted) {
    gint x,y;
    traycl2xy(selectedcolor, 0, &x, &y, widget);
    motion_x_shift = (x - (int) event->x) - (ballmed - ballsm)/2;
    motion_y_shift = (y - (int) event->y) - (ballmed - ballsm)/2;
    gm_debug("motion start\n");
/*
  gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
  gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), _("Drag the ball to an empty cell!"));

*/
    clean_tray(widget);
  }
  if (pressed) {
    window = gtk_widget_get_window (widget);
    if (motionbk) {
      cr = cairo_create (pixmap);
      cairo_set_source_surface (cr, motionbk, xbk, ybk);
      cairo_paint (cr);
      cairo_destroy (cr);
      rect.x = xbk;
      rect.y = ybk;
      rect.width = ballmed;
      rect.height = ballmed;
      gdk_window_invalidate_rect (window, &rect, FALSE);
      cairo_surface_destroy (motionbk);
      motionbk = NULL;
    }
    motionbk = gdk_window_create_similar_surface (window,
                                                  CAIRO_CONTENT_COLOR_ALPHA,
                                                  ballmed,
                                                  ballmed);
    cr = cairo_create (motionbk);
    cairo_set_source_surface (cr, pixmap,
                              (int) -event->x - motion_x_shift,
                              (int) -event->y - motion_y_shift);
    cairo_paint (cr);
    cairo_destroy (cr);

    xbk = (int) event->x + motion_x_shift;
    ybk = (int) event->y + motion_y_shift;

    mstarted = 1;
    cr = cairo_create (pixmap);
    gdk_cairo_set_source_pixbuf (cr, tileset_bg,
                                 xbk - ballmed * selectedcolor,
                                 ybk);
    cairo_rectangle (cr, xbk, ybk, ballmed, ballmed);
    cairo_fill (cr);
    cairo_destroy (cr);
    rect.x = xbk;
    rect.y = ybk;
    rect.width = ballmed;
    rect.height = ballmed;
    gdk_window_invalidate_rect (window, &rect, FALSE);
  }

  return FALSE;
}

static gboolean button_press_event ( GtkWidget *widget,
                                     GdkEventButton *event )
{
  cairo_t *cr;
  gint c, l;
  gint i;

  if (event->type == GDK_BUTTON_RELEASE) {
    if (stx == event->x && sty == event->y) {
    }
    pressed = 0;
    if (mstarted) {
      if (motionbk) {
        cr = cairo_create (pixmap);
        cairo_set_source_surface (cr, motionbk,
                                  xbk, ybk);
        cairo_paint (cr);
        cairo_destroy (cr);
        rect.x = xbk;
        rect.y = ybk;
        rect.width = ballmed;
        rect.height = ballmed;
        gdk_window_invalidate_rect (gtk_widget_get_window (widget), &rect, FALSE);
        cairo_surface_destroy (motionbk);
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

        g_simple_action_set_enabled (G_SIMPLE_ACTION (redo_action), FALSE);

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

  if (event->y > (gtk_widget_get_allocated_height (widget) - tray_h)) {
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

      g_simple_action_set_enabled (G_SIMPLE_ACTION (redo_action), FALSE);

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

static gboolean key_press_event ( GtkWidget *widget,
                                  GdkEventKey *event ) {
  if (pressed)
    return TRUE;
  switch (event->keyval) {
    case GDK_KEY_1:
      selectedcolor = 0;
      tray_mid_click ();
      break;
    case GDK_KEY_2:
      selectedcolor = 1;
      tray_mid_click ();
      break;
    case GDK_KEY_3:
      selectedcolor = 2;
      tray_mid_click ();
      break;
    case GDK_KEY_4:
      selectedcolor = 3;
      tray_mid_click ();
      break;
    case GDK_KEY_5:
      selectedcolor = 4;
      tray_mid_click ();
      break;
    case GDK_KEY_6:
      selectedcolor = 5;
      tray_mid_click ();
      break;
    case GDK_KEY_7:
      selectedcolor = 6;
      tray_mid_click ();
      break;
    case GDK_KEY_8:
      selectedcolor = 7;
      tray_mid_click ();
      break;
    default:
      break;
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
        timeout_id = 0;
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

      g_simple_action_set_enabled (G_SIMPLE_ACTION (redo_action), FALSE);

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
                               cairo_t *cr )
{
  cairo_set_source_surface (cr, pixmap, 0, 0);
  cairo_paint (cr);

  return FALSE;
}

static gboolean delete_event (GtkWidget *widget, GdkEvent *event, gpointer data) {
  if (pixmap)
    cairo_surface_destroy (pixmap);
  if (traymapbk)
    cairo_surface_destroy (traymapbk);
  if (cellbk)
    cairo_surface_destroy (cellbk);
  if (motionbk)
    cairo_surface_destroy (motionbk);

  return FALSE;
}

static void destroy (GtkWidget *widget, gpointer data) {
  g_action_activate (quit, NULL);
}

static void help_action (GSimpleAction *action, GVariant *par, gpointer data) {
  GError *error = NULL;

  gtk_show_uri_on_window (GTK_WINDOW (window), "help:gnome-mastermind",
                          gtk_get_current_event_time (), &error);
  if (error)
  {
    g_message ("Error while launching yelp %s", error->message);
    g_error_free (error);
    error = NULL;
  }
}

static void about_action (GSimpleAction *action,
                          GVariant *param, gpointer data) {
  gchar *authors[] = { "Filippo Argiolas <filippo.argiolas@gmail.com>", NULL };
  gchar *artists[] = {
    "Filippo Argiolas <filippo.argiolas@gmail.com>, me ;)",
    "Ulisse Perusin <uli.peru@gmail.com>, for that beautiful icon and tango sets!", "Sean Wilson <suseux@gmail.com>, for the new fruity tileset","and..",
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
                         "copyright", "gnome-mastermind, copyright (c) 2008 Filippo Argiolas\n"
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

  g_settings_set_string (settings, "theme", theme_item->data);
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

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                    item);

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
  g_settings_set_boolean (settings, "gtkstyle-colors", state);
}

static void fgcolorbutton_set (GtkColorButton *widget, gpointer data) {
  GdkRGBA color;
  gchar *color_string;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (widget), &color);

  color_string = gdk_rgba_to_string (&color);
  g_settings_set_string (settings, "fgcolor", color_string);
  g_free (color_string);
}

static void bgcolorbutton_set (GtkColorButton *widget, gpointer data) {
  GdkRGBA color;
  gchar *color_string;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (widget), &color);
  color_string = gdk_rgba_to_string (&color);

  g_settings_set_string (settings, "bgcolor", color_string);
  g_free (color_string);
}

static void preferences_action (GSimpleAction *action,
                                GVariant *param, gpointer data) {
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *theme_combo;
  GtkWidget *color_table;
  GtkWidget *color_vbox;
  GtkWidget *frame;
  GtkWidget *carea;
  GtkWidget *pango_label;

  GtkWidget *use_style_check;
  GtkWidget *max_tries_spin;
// GtkWidget *hbox;

  pref_dialog = gtk_dialog_new_with_buttons (_("Preferences"),
                                             GTK_WINDOW (window),
                                             GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                             _("_Close"),
                                             GTK_RESPONSE_CLOSE,
                                             NULL);

  carea = gtk_dialog_get_content_area (GTK_DIALOG (pref_dialog));
  gtk_container_set_border_width (GTK_CONTAINER (pref_dialog), 4);
  gtk_box_set_spacing (GTK_BOX (carea), 2);

  frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  pango_label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (pango_label), _("<b>Tileset theme</b>"));

  gtk_frame_set_label_widget (GTK_FRAME (frame), pango_label);

  table = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (table), 10);
  gtk_grid_set_row_spacing (GTK_GRID (table), 6);
  gtk_grid_set_column_spacing (GTK_GRID (table), 12);
  gtk_widget_set_halign (table, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (table, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (table, 10);

  label = gtk_label_new (_("Theme:"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);

  theme_combo = gtk_combo_box_text_new ();
  populate_theme_combo (theme_combo);

  g_signal_connect (G_OBJECT (theme_combo), "changed",
                    G_CALLBACK (theme_changed), NULL);

  gtk_grid_attach (GTK_GRID (table), theme_combo, 1, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (frame), table);

  gtk_box_pack_start (GTK_BOX (carea), frame, TRUE, TRUE, 0);

  frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  pango_label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (pango_label), _("<b>Color settings</b>"));

  gtk_frame_set_label_widget (GTK_FRAME (frame), pango_label);

  color_table = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (color_table), 10);
  gtk_grid_set_row_spacing (GTK_GRID (color_table), 2);
  gtk_grid_set_column_spacing (GTK_GRID (color_table), 12);

  label = gtk_label_new (_("Foreground Color:"));
  gtk_grid_attach (GTK_GRID (color_table), label, 0, 0, 1, 1);

  fg_colorbutton = gtk_color_button_new_with_rgba (&fgcolor);
  gtk_widget_set_size_request (fg_colorbutton, 150, -1);

  g_signal_connect (G_OBJECT (fg_colorbutton), "color-set",
                    G_CALLBACK (fgcolorbutton_set), &fgcolor);

  gtk_grid_attach (GTK_GRID (color_table), fg_colorbutton, 1, 0, 1, 1);

  label = gtk_label_new (_("Background Color:"));
  gtk_grid_attach (GTK_GRID (color_table), label, 0, 1, 1, 1);

  bg_colorbutton = gtk_color_button_new_with_rgba (&bgcolor);
  gtk_widget_set_size_request (bg_colorbutton, 150, -1);

  g_signal_connect (G_OBJECT (bg_colorbutton), "color-set",
                    G_CALLBACK (bgcolorbutton_set), &bgcolor);

  gtk_grid_attach (GTK_GRID (color_table), bg_colorbutton, 1, 1, 1, 1);

  use_style_check = gtk_check_button_new_with_label (_("Try to get colors from system theme"));

  gtk_container_set_border_width (GTK_CONTAINER (use_style_check), 10);

  g_signal_connect (G_OBJECT (use_style_check), "toggled",
                    G_CALLBACK (use_style_toggled), color_table);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (use_style_check),
                                gc_gtkstyle_colors);


  color_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (color_vbox, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (color_vbox, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (color_vbox, 10);
  gtk_box_pack_start (GTK_BOX (color_vbox),
                      color_table, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (color_vbox),
                      use_style_check, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (frame), color_vbox);

  gtk_box_pack_start (GTK_BOX (carea), frame, TRUE, TRUE, 0);

  frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  pango_label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (pango_label), _("<b>Game settings</b>"));

  gtk_frame_set_label_widget (GTK_FRAME (frame), pango_label);

  table = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (table), 10);
  gtk_grid_set_row_spacing (GTK_GRID (table), 6);
  gtk_grid_set_column_spacing (GTK_GRID (table), 12);
  gtk_widget_set_halign (table, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (table, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (table, 10);

  label = gtk_label_new (_("Maximum number of tries allowed:"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);

  max_tries_spin = gtk_spin_button_new_with_range (2, 14, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (max_tries_spin), gc_max_tries);

  gtk_grid_attach (GTK_GRID (table), max_tries_spin, 1, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (frame), table);

  gtk_box_pack_start (GTK_BOX (carea), frame, TRUE, TRUE, 0);

  gtk_widget_show_all (pref_dialog);

  gtk_dialog_run (GTK_DIALOG (pref_dialog));

  g_settings_set_int (settings, "maximum-tries",
                      gtk_spin_button_get_value (
                        GTK_SPIN_BUTTON (max_tries_spin)));

/*
  else if (response == GTK_RESPONSE_YES) {
  reset_default_settings();
  }
*/

  gtk_widget_destroy (pref_dialog);
  pref_dialog = NULL;
}

static void show_tb_callback (GSimpleAction *action, GVariant *param,
                              gpointer data) {
  gboolean state;

  state = g_variant_get_boolean (param);

  gm_debug ("gc_show_toolbar: %d state: %d\n", gc_show_toolbar, state);

  g_settings_set_boolean (settings, "show-toolbar", state);
  g_simple_action_set_state (action, param);
}


/* Create a list of entries which are passed to the Action constructor.
 * This is a huge convenience over building Actions by hand. */
static GActionEntry entries[] =
{
  { "new", new_game, NULL, NULL, NULL },
  { "undo", undo_action_cb, NULL, NULL, NULL },
  { "redo", redo_action_cb, NULL, NULL, NULL },
  { "quit", quit_action, NULL, NULL, NULL },
  { "show-toolbar", NULL, NULL, "true", show_tb_callback },
  { "prefs", preferences_action, NULL, NULL, NULL },
  { "help", help_action, NULL, NULL, NULL },
  { "about", about_action, NULL, NULL, NULL }
};

static guint n_entries = G_N_ELEMENTS (entries);

static void
activate (GApplication *app)
{
  int i;

  GtkWidget *gridframe;
  GtkBuilder *builder;

  gtk_window_set_default_icon_name("gnome-mastermind");

  window = gtk_application_window_new (GTK_APPLICATION (app));
  gtk_window_set_title(GTK_WINDOW(window), "GNOME Mastermind");

  g_signal_connect (G_OBJECT (window), "delete_event",
                    G_CALLBACK (delete_event), NULL);
  g_signal_connect (G_OBJECT (window), "destroy",
                    G_CALLBACK (destroy), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  init_gconf();

  builder = gtk_builder_new_from_resource ("/org/fargiolas/gnome-mastermind/ui.ui");
  toolbar = GTK_WIDGET (gtk_builder_get_object (builder, "toolbar"));
  gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
  g_object_unref (builder);

  show_toolbar_action = g_action_map_lookup_action (G_ACTION_MAP (app),
                                                    "show-toolbar");
  g_simple_action_set_state (G_SIMPLE_ACTION (show_toolbar_action),
                             g_variant_new_boolean (gc_show_toolbar));

  new_action = g_action_map_lookup_action (G_ACTION_MAP (app), "new");
  quit = g_action_map_lookup_action (G_ACTION_MAP (app), "quit");
  undo_action = g_action_map_lookup_action (G_ACTION_MAP (app), "undo");
  redo_action = g_action_map_lookup_action (G_ACTION_MAP (app), "redo");

  drawing_area = gtk_drawing_area_new();

  g_signal_connect (G_OBJECT (drawing_area), "draw",
                    G_CALLBACK (expose_event), NULL);
  g_signal_connect (G_OBJECT (drawing_area), "configure_event",
                    G_CALLBACK (configure_event), NULL);

  gtk_widget_add_events (drawing_area, GDK_EXPOSURE_MASK
                         | GDK_LEAVE_NOTIFY_MASK
                         | GDK_BUTTON_PRESS_MASK
                         | GDK_KEY_PRESS_MASK
                         | GDK_BUTTON_RELEASE_MASK
                         | GDK_BUTTON1_MOTION_MASK);

  g_signal_connect (G_OBJECT (drawing_area), "key_press_event",
                    G_CALLBACK (key_press_event), NULL);
  g_signal_connect (G_OBJECT (drawing_area), "button_press_event",
                    G_CALLBACK (button_press_event), NULL);
  g_signal_connect (G_OBJECT (drawing_area), "button_release_event",
                    G_CALLBACK (button_press_event), NULL);
  g_signal_connect (G_OBJECT (drawing_area), "motion-notify-event",
                    G_CALLBACK (motion_event), NULL);

  gtk_widget_set_can_focus (drawing_area, TRUE);

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

  gtk_box_pack_end (GTK_BOX (vbox), status, FALSE, FALSE, 0);

  gtk_statusbar_pop (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"));
  gtk_statusbar_push (GTK_STATUSBAR (status), gtk_statusbar_get_context_id (GTK_STATUSBAR (status), "mmind"), _("Select a color!"));

  gtk_box_pack_end (GTK_BOX (vbox), gridframe, TRUE, TRUE, 0);

//	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  gtk_widget_show_all (window);

  if (!g_settings_get_boolean (settings, "show-toolbar"))
  {
    gtk_widget_hide (toolbar);
    gtk_window_resize (GTK_WINDOW(window), 1, 1);
  }

  gtk_widget_grab_focus (drawing_area);
}

static void
startup (GApplication *app)
{
  gint i;
  const gchar *debug_env = NULL;
  struct {
    const gchar *action;
    const gchar *accel[2];
  } accels[] = {
    { "app.new", { "<Primary>n", NULL } },
    { "app.undo", { "<Primary>z", NULL } },
    { "app.redo", { "<Shift><Control>Z", NULL } },
    { "app.quit", { "<Primary>q", NULL } },
    { "app.prefs", { "<Primary>p", NULL } },
    { "app.help", { "F1", NULL } }
  };

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

  g_action_map_add_action_entries (G_ACTION_MAP (app), entries,
                                   n_entries, app);

  for (i = 0; i < G_N_ELEMENTS (accels); i++)
    gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                           accels[i].action,
                                           accels[i].accel);
}

int main ( int argc, char *argv[] )
{
  GtkApplication *app;
  int status;

  app = gtk_application_new ("org.fargiolas.gnome-mastermind",
                             G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "startup", G_CALLBACK (startup), NULL);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
