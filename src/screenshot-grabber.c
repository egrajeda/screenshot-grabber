/*
Copyright (C) 2010, Eduardo Grajeda <tatofoo@gmail.com>

This file is part of Gyazolinux.

Gyazolinux is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Gyazolinux is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Gyazolinux.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "config.h"

static void
select_area_button_press (XKeyEvent    *event,
                          GdkRectangle *rect,
                          GdkRectangle *draw_rect)
{
  rect->x = event->x_root;
  rect->y = event->y_root;

  draw_rect->x = rect->x;
  draw_rect->y = rect->y;
  draw_rect->width  = 0;
  draw_rect->height = 0;
}

static void
select_area_button_release (XKeyEvent    *event,
                            GdkRectangle *rect,
                            GdkRectangle *draw_rect,
                            GdkWindow    *root,
                            GdkGC        *gc)
{
  /* remove the old rectangle */
  if (draw_rect->width > 0 && draw_rect->height > 0)
    gdk_draw_rectangle (root, gc, FALSE, 
                        draw_rect->x, draw_rect->y,
                        draw_rect->width, draw_rect->height);

  rect->width  = ABS (rect->x - event->x_root);
  rect->height = ABS (rect->y - event->y_root);

  rect->x = MIN (rect->x, event->x_root);
  rect->y = MIN (rect->y, event->y_root);
}

static void
select_area_motion_notify (XKeyEvent    *event,
                           GdkRectangle *rect,
                           GdkRectangle *draw_rect,
                           GdkWindow    *root,
                           GdkGC        *gc)
{
  /* FIXME: draw some nice rubberband with cairo if composited */

  /* remove the old rectangle */
  if (draw_rect->width > 0 && draw_rect->height > 0)
    gdk_draw_rectangle (root, gc, FALSE, 
                        draw_rect->x, draw_rect->y,
                        draw_rect->width, draw_rect->height);

  draw_rect->width  = ABS (rect->x - event->x_root);
  draw_rect->height = ABS (rect->y - event->y_root);

  draw_rect->x = MIN (rect->x, event->x_root);
  draw_rect->y = MIN (rect->y, event->y_root);

  /* draw the new rectangle */
  if (draw_rect->width > 0 && draw_rect->height > 0)
    gdk_draw_rectangle (root, gc, FALSE, 
                        draw_rect->x, draw_rect->y,
                        draw_rect->width, draw_rect->height);
}

typedef struct {
  GdkRectangle  rect;
  GdkRectangle  draw_rect;
  gboolean      button_pressed;
  /* only needed because we're not using cairo to draw the rectangle */
  GdkWindow    *root;
  GdkGC        *gc;
} select_area_filter_data;

static GdkFilterReturn
select_area_filter (GdkXEvent *gdk_xevent,
                    GdkEvent  *event,
                    gpointer   user_data)
{
  select_area_filter_data *data = user_data;
  XEvent *xevent = (XEvent *) gdk_xevent;

  switch (xevent->type)
    {
    case ButtonPress:
      if (!data->button_pressed)
        {
          select_area_button_press (&xevent->xkey,
                                    &data->rect, &data->draw_rect);
          data->button_pressed = TRUE;
        }
      return GDK_FILTER_REMOVE;
    case ButtonRelease:
      if (data->button_pressed)
      {
        select_area_button_release (&xevent->xkey,
                                    &data->rect, &data->draw_rect,
                                    data->root, data->gc);
        gtk_main_quit ();
      }
      return GDK_FILTER_REMOVE;
    case MotionNotify:
      if (data->button_pressed)
        select_area_motion_notify (&xevent->xkey,
                                   &data->rect, &data->draw_rect,
                                   data->root, data->gc);
      return GDK_FILTER_REMOVE;
    case KeyPress:
      if (xevent->xkey.keycode == XKeysymToKeycode (gdk_display, XK_Escape))
        {
          data->rect.x = 0;
          data->rect.y = 0;
          data->rect.width  = 0;
          data->rect.height = 0;
          gtk_main_quit ();
          return GDK_FILTER_REMOVE;
        }
      break;
    default:
      break;
    }
 
  return GDK_FILTER_CONTINUE;
}

static GdkPixbuf *
get_screenshot_rectangle (GdkRectangle *rectangle)
{
  GdkWindow *root;
  GdkPixbuf *screenshot;
  gint x_orig, y_orig;
  gint width, height;

  root = gdk_get_default_root_window ();

  x_orig = rectangle->x;
  y_orig = rectangle->y;
  width  = rectangle->width;
  height = rectangle->height;
  
  screenshot = gdk_pixbuf_get_from_drawable (NULL, root, NULL,
                                             x_orig, y_orig, 0, 0,
                                             width, height);
  return screenshot;
}

static GdkRectangle *
select_area ()
{
  GdkWindow               *root;
  GdkCursor               *cursor;
  select_area_filter_data  data;
  GdkGCValues              values;
  GdkColor                 color;
  GdkRectangle            *rectangle;

  root   = gdk_get_default_root_window ();
  cursor = gdk_cursor_new (GDK_CROSSHAIR);

  if (gdk_pointer_grab (root, FALSE,
                        GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK,
                        NULL, cursor,
                        GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
    {
      gdk_cursor_unref (cursor);
      return FALSE;
    }

  gdk_keyboard_grab (root, FALSE, GDK_CURRENT_TIME);

  gdk_window_add_filter (root, (GdkFilterFunc) select_area_filter, &data);

  gdk_flush ();

  data.rect.x = 0;
  data.rect.y = 0;
  data.rect.width  = 0;
  data.rect.height = 0;
  data.button_pressed = FALSE;
  data.root = root;

  values.function = GDK_XOR;
  values.fill = GDK_SOLID;
  values.clip_mask = NULL;
  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  values.clip_x_origin = 0;
  values.clip_y_origin = 0;
  values.graphics_exposures = 0;
  values.line_width = 0;
  values.line_style = GDK_LINE_SOLID;
  values.cap_style = GDK_CAP_BUTT;
  values.join_style = GDK_JOIN_MITER;

  data.gc = gdk_gc_new_with_values (root, &values,
                                    GDK_GC_FUNCTION | GDK_GC_FILL |
                                    GDK_GC_CLIP_MASK | GDK_GC_SUBWINDOW |
                                    GDK_GC_CLIP_X_ORIGIN |
                                    GDK_GC_CLIP_Y_ORIGIN | GDK_GC_EXPOSURES |
                                    GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE |
                                    GDK_GC_CAP_STYLE | GDK_GC_JOIN_STYLE);
  gdk_color_parse ("white", &color);
  gdk_gc_set_rgb_fg_color (data.gc, &color);
  gdk_color_parse ("black", &color);
  gdk_gc_set_rgb_bg_color (data.gc, &color);

  gtk_main ();

  g_object_unref (data.gc);

  gdk_window_remove_filter (root, (GdkFilterFunc) select_area_filter, &data);

  gdk_keyboard_ungrab (GDK_CURRENT_TIME);
  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gdk_cursor_unref (cursor);

  if (data.rect.width == 0 && data.rect.height == 0)
    return NULL;

  rectangle = g_new0 (GdkRectangle, 1);
  rectangle->x = data.rect.x;
  rectangle->y = data.rect.y;
  rectangle->width  = data.rect.width + 1;
  rectangle->height = data.rect.height + 1;

  return rectangle;
}

static void
take_screenshot ()
{
  GdkRectangle *rectangle;
  GdkPixbuf    *screenshot;
  GtkClipboard *clipboard;

  rectangle  = select_area ();
  if (rectangle == NULL)
    return;

  screenshot = get_screenshot_rectangle (rectangle);

  clipboard = gtk_clipboard_get (gdk_atom_intern ("CLIPBOARD", FALSE));
  gtk_clipboard_set_image (clipboard, screenshot);
}

/* -- Main -- */

DBusHandlerResult dbus_handler (DBusConnection *bus,
                                DBusMessage    *message,
                                void           *user_data)
{
  if (dbus_message_is_signal (message, "org.gnome.ScreenshotGrabber",
                              "take_screenshot"))
    {
      take_screenshot ();
      DBUS_HANDLER_RESULT_HANDLED; 
    }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int
main (int    argc,
      char **argv)
{
  int             ret;
  DBusConnection *bus;
  DBusMessage    *msg;
  DBusError       err = {0};

  gtk_init (&argc, &argv);

  bus = dbus_bus_get (DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set (&err))
    goto exit;

  ret = dbus_bus_request_name (bus, "org.gnome.ScreenshotGrabber",
                               DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
  if (dbus_error_is_set (&err))
    goto exit;

  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      msg = dbus_message_new_signal ("/org/gnome/ScreenshotGrabber",
                                     "org.gnome.ScreenshotGrabber",
                                     "take_screenshot");
      dbus_connection_send (bus, msg, NULL);
      dbus_message_unref (msg);
      return 0;
    }

  dbus_connection_setup_with_g_main (bus, NULL);

  dbus_bus_add_match (bus, "type='signal',"
      "interface='org.gnome.ScreenshotGrabber',"
      "path='/org/gnome/ScreenshotGrabber'", &err);
  if (dbus_error_is_set (&err))
    goto exit;
  dbus_connection_add_filter (bus, dbus_handler, NULL, NULL);

  take_screenshot ();

  gtk_main ();

exit:
  if (dbus_error_is_set (&err))
    {
      fprintf (stderr, "[E] DBus: %s\n", err.message);
      dbus_error_free (&err);
    }

  return 0;
}
