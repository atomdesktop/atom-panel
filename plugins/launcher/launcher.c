/*
 * Copyright (C) 2008-2009 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>
#include <exo/exo.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <garcon/garcon.h>
#include <xfconf/xfconf.h>

#include <libxfce4panel/libxfce4panel.h>
#include <common/panel-private.h>
#include <common/panel-xfconf.h>

#include "launcher.h"
#include "launcher-dialog.h"

#define ARROW_BUTTON_SIZE              (12)
#define TOOLTIP_ICON_SIZE              (32)
#define MENU_POPUP_DELAY               (225)
#define NO_ARROW_INSIDE_BUTTON(plugin) ((plugin)->arrow_position != LAUNCHER_ARROW_INTERNAL \
                                        || LIST_HAS_ONE_OR_NO_ENTRIES ((plugin)->items))
#define ARROW_INSIDE_BUTTON(plugin)    (!NO_ARROW_INSIDE_BUTTON (plugin))
#define RELATIVE_CONFIG_PATH           PANEL_PLUGIN_RELATIVE_PATH G_DIR_SEPARATOR_S "%s-%d"



static void       launcher_plugin_get_property                  (GObject              *object,
                                                                 guint                 prop_id,
                                                                 GValue               *value,
                                                                 GParamSpec           *pspec);
static void       launcher_plugin_set_property                  (GObject              *object,
                                                                 guint                 prop_id,
                                                                 const GValue         *value,
                                                                 GParamSpec           *pspec);
static void       launcher_plugin_construct                     (XfcePanelPlugin      *panel_plugin);
static void       launcher_plugin_free_data                     (XfcePanelPlugin      *panel_plugin);
static void       launcher_plugin_removed                       (XfcePanelPlugin      *panel_plugin);
static gboolean   launcher_plugin_remote_event                  (XfcePanelPlugin      *panel_plugin,
                                                                 const gchar          *name,
                                                                 const GValue         *value);
static void       launcher_plugin_save                          (XfcePanelPlugin      *panel_plugin);
static void       launcher_plugin_orientation_changed           (XfcePanelPlugin      *panel_plugin,
                                                                 GtkOrientation        orientation);
static gboolean   launcher_plugin_size_changed                  (XfcePanelPlugin      *panel_plugin,
                                                                 gint                  size);
static void       launcher_plugin_configure_plugin              (XfcePanelPlugin      *panel_plugin);
static void       launcher_plugin_screen_position_changed       (XfcePanelPlugin      *panel_plugin,
                                                                 XfceScreenPosition    position);
static void       launcher_plugin_icon_theme_changed            (GtkIconTheme         *icon_theme,
                                                                 LauncherPlugin       *plugin);
static void       launcher_plugin_pack_widgets                  (LauncherPlugin       *plugin);
static GdkPixbuf *launcher_plugin_tooltip_pixbuf                (GdkScreen            *screen,
                                                                 const gchar          *icon_name);
static void       launcher_plugin_menu_deactivate               (GtkWidget            *menu,
                                                                 LauncherPlugin       *plugin);
static void       launcher_plugin_menu_item_activate            (GtkMenuItem          *widget,
                                                                 GarconMenuItem       *item);
static void       launcher_plugin_menu_item_drag_data_received  (GtkWidget            *widget,
                                                                 GdkDragContext       *context,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 GtkSelectionData     *data,
                                                                 guint                 info,
                                                                 guint                 drag_time,
                                                                 GarconMenuItem       *item);
static void       launcher_plugin_menu_construct                (LauncherPlugin       *plugin);
static void       launcher_plugin_menu_popup_destroyed          (gpointer              user_data);
static gboolean   launcher_plugin_menu_popup                    (gpointer              user_data);
static void       launcher_plugin_menu_destroy                  (LauncherPlugin       *plugin);
static void       launcher_plugin_button_update                 (LauncherPlugin       *plugin);
static void       launcher_plugin_button_state_changed          (GtkWidget            *button_a,
                                                                 GtkStateType         state,
                                                                 GtkWidget            *button_b);
static gboolean   launcher_plugin_button_press_event            (GtkWidget            *button,
                                                                 GdkEventButton       *event,
                                                                 LauncherPlugin       *plugin);
static gboolean   launcher_plugin_button_release_event          (GtkWidget            *button,
                                                                 GdkEventButton       *event,
                                                                 LauncherPlugin       *plugin);
static gboolean   launcher_plugin_button_query_tooltip          (GtkWidget            *widget,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 gboolean              keyboard_mode,
                                                                 GtkTooltip           *tooltip,
                                                                 LauncherPlugin       *plugin);
static void       launcher_plugin_button_drag_data_received     (GtkWidget            *widget,
                                                                 GdkDragContext       *context,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 GtkSelectionData     *selection_data,
                                                                 guint                 info,
                                                                 guint                 drag_time,
                                                                 LauncherPlugin       *plugin);
static gboolean   launcher_plugin_button_drag_motion            (GtkWidget            *widget,
                                                                 GdkDragContext       *context,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 guint                 drag_time,
                                                                 LauncherPlugin       *plugin);
static void       launcher_plugin_button_drag_leave             (GtkWidget            *widget,
                                                                 GdkDragContext       *context,
                                                                 guint                 drag_time,
                                                                 LauncherPlugin       *plugin);
static gboolean   launcher_plugin_button_expose_event           (GtkWidget            *widget,
                                                                 GdkEventExpose       *event,
                                                                 LauncherPlugin       *launcher);
static void       launcher_plugin_arrow_visibility              (LauncherPlugin       *plugin);
static gboolean   launcher_plugin_arrow_press_event             (GtkWidget            *button,
                                                                 GdkEventButton       *event,
                                                                 LauncherPlugin       *plugin);
static gboolean   launcher_plugin_arrow_drag_motion             (GtkWidget            *widget,
                                                                 GdkDragContext       *context,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 guint                 drag_time,
                                                                 LauncherPlugin       *plugin);
static void       launcher_plugin_arrow_drag_leave              (GtkWidget            *widget,
                                                                 GdkDragContext       *context,
                                                                 guint                 drag_time,
                                                                 LauncherPlugin       *plugin);
static gboolean   launcher_plugin_item_query_tooltip            (GtkWidget            *widget,
                                                                 gint                  x,
                                                                 gint                  y,
                                                                 gboolean              keyboard_mode,
                                                                 GtkTooltip           *tooltip,
                                                                 GarconMenuItem       *item);
static gboolean   launcher_plugin_item_exec_on_screen           (GarconMenuItem       *item,
                                                                 guint32               event_time,
                                                                 GdkScreen            *screen,
                                                                 GSList               *uri_list);
static void       launcher_plugin_item_exec                     (GarconMenuItem       *item,
                                                                 guint32               event_time,
                                                                 GdkScreen            *screen,
                                                                 GSList               *uri_list);
static void       launcher_plugin_item_exec_from_clipboard      (GarconMenuItem       *item,
                                                                 guint32               event_time,
                                                                 GdkScreen            *screen);
static void       launcher_plugin_exec_append_quoted            (GString              *string,
                                                                 const gchar          *unquoted);
static gboolean   launcher_plugin_exec_parse                    (GarconMenuItem       *item,
                                                                 GSList               *uri_list,
                                                                 gchar              ***argv,
                                                                 GError              **error);
static GSList    *launcher_plugin_uri_list_extract              (GtkSelectionData     *data);
static void       launcher_plugin_uri_list_free                 (GSList               *uri_list);



struct _LauncherPluginClass
{
  XfcePanelPluginClass __parent__;
};

struct _LauncherPlugin
{
  XfcePanelPlugin __parent__;

  GtkWidget         *box;
  GtkWidget         *button;
  GtkWidget         *arrow;
  GtkWidget         *child;
  GtkWidget         *menu;

  GSList            *items;

  GdkPixbuf         *tooltip_cache;

  gulong             theme_change_id;

  guint              menu_timeout_id;

  guint              disable_tooltips : 1;
  guint              move_first : 1;
  guint              show_label : 1;
  LauncherArrowType  arrow_position;

  GFile             *config_directory;
  GFileMonitor      *config_monitor;
};

enum
{
  PROP_0,
  PROP_ITEMS,
  PROP_DISABLE_TOOLTIPS,
  PROP_MOVE_FIRST,
  PROP_SHOW_LABEL,
  PROP_ARROW_POSITION
};

enum
{
  ITEMS_CHANGED,
  LAST_SIGNAL
};


/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN_RESIDENT (LauncherPlugin, launcher_plugin)



/* quark to attach the plugin to menu items */
static GQuark      launcher_plugin_quark = 0;
static guint       launcher_signals[LAST_SIGNAL];
static GtkIconSize launcher_menu_icon_size = GTK_ICON_SIZE_INVALID;
static GtkIconSize launcher_tooltip_icon_size = GTK_ICON_SIZE_INVALID;



/* target types for dropping in the launcher plugin */
static const GtkTargetEntry drop_targets[] =
{
  { "text/uri-list", 0, 0, },
  { "STRING", 0, 0 },
  { "UTF8_STRING", 0, 0 },
  { "text/plain", 0, 0 },
};



static void
launcher_plugin_class_init (LauncherPluginClass *klass)
{
  GObjectClass         *gobject_class;
  XfcePanelPluginClass *plugin_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = launcher_plugin_get_property;
  gobject_class->set_property = launcher_plugin_set_property;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = launcher_plugin_construct;
  plugin_class->free_data = launcher_plugin_free_data;
  plugin_class->save = launcher_plugin_save;
  plugin_class->orientation_changed = launcher_plugin_orientation_changed;
  plugin_class->size_changed = launcher_plugin_size_changed;
  plugin_class->configure_plugin = launcher_plugin_configure_plugin;
  plugin_class->screen_position_changed = launcher_plugin_screen_position_changed;
  plugin_class->removed = launcher_plugin_removed;
  plugin_class->remote_event = launcher_plugin_remote_event;

  g_object_class_install_property (gobject_class,
                                   PROP_ITEMS,
                                   g_param_spec_boxed ("items",
                                                       NULL, NULL,
                                                       PANEL_PROPERTIES_TYPE_VALUE_ARRAY,
                                                       EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DISABLE_TOOLTIPS,
                                   g_param_spec_boolean ("disable-tooltips",
                                                         NULL, NULL,
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MOVE_FIRST,
                                   g_param_spec_boolean ("move-first",
                                                         NULL, NULL,
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_LABEL,
                                   g_param_spec_boolean ("show-label",
                                                         NULL, NULL,
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ARROW_POSITION,
                                   g_param_spec_uint ("arrow-position",
                                                      NULL, NULL,
                                                      LAUNCHER_ARROW_DEFAULT,
                                                      LAUNCHER_ARROW_INTERNAL,
                                                      LAUNCHER_ARROW_DEFAULT,
                                                      EXO_PARAM_READWRITE));

  launcher_signals[ITEMS_CHANGED] =
    g_signal_new (g_intern_static_string ("items-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /* initialize the quark */
  launcher_plugin_quark = g_quark_from_static_string ("xfce-launcher-plugin");

  launcher_menu_icon_size = gtk_icon_size_from_name ("panel-launcher-menu");
  if (launcher_menu_icon_size == GTK_ICON_SIZE_INVALID)
    launcher_menu_icon_size = gtk_icon_size_register ("panel-launcher-menu", 32, 32);

  launcher_tooltip_icon_size = gtk_icon_size_from_name ("panel-launcher-tooltip");
  if (launcher_tooltip_icon_size == GTK_ICON_SIZE_INVALID)
    launcher_tooltip_icon_size = gtk_icon_size_register ("panel-launcher-tooltip",
                                                         TOOLTIP_ICON_SIZE,
                                                         TOOLTIP_ICON_SIZE);
}



static void
launcher_plugin_init (LauncherPlugin *plugin)
{
  GtkIconTheme *icon_theme;

  plugin->disable_tooltips = FALSE;
  plugin->move_first = FALSE;
  plugin->show_label = FALSE;
  plugin->arrow_position = LAUNCHER_ARROW_DEFAULT;
  plugin->menu = NULL;
  plugin->items = NULL;
  plugin->child = NULL;
  plugin->tooltip_cache = NULL;
  plugin->menu_timeout_id = 0;

  /* monitor the default icon theme for changes */
  icon_theme = gtk_icon_theme_get_default ();
  plugin->theme_change_id = g_signal_connect (G_OBJECT (icon_theme), "changed",
      G_CALLBACK (launcher_plugin_icon_theme_changed), plugin);

  /* create the panel widgets */
  plugin->box = xfce_hvbox_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (plugin), plugin->box);

  plugin->button = xfce_panel_create_button ();
  gtk_box_pack_start (GTK_BOX (plugin->box), plugin->button, TRUE, TRUE, 0);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->button);
  gtk_widget_set_has_tooltip (plugin->button, TRUE);
  gtk_widget_set_name (plugin->button, "launcher-button");
  gtk_drag_dest_set (plugin->button, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                     drop_targets, G_N_ELEMENTS (drop_targets), GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (plugin->button), "button-press-event",
      G_CALLBACK (launcher_plugin_button_press_event), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "button-release-event",
      G_CALLBACK (launcher_plugin_button_release_event), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "query-tooltip",
      G_CALLBACK (launcher_plugin_button_query_tooltip), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "drag-data-received",
      G_CALLBACK (launcher_plugin_button_drag_data_received), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "drag-motion",
      G_CALLBACK (launcher_plugin_button_drag_motion), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "drag-leave",
      G_CALLBACK (launcher_plugin_button_drag_leave), plugin);
  g_signal_connect_after (G_OBJECT (plugin->button), "expose-event",
      G_CALLBACK (launcher_plugin_button_expose_event), plugin);

  plugin->child = xfce_panel_image_new ();
  gtk_container_add (GTK_CONTAINER (plugin->button), plugin->child);

  plugin->arrow = xfce_arrow_button_new (GTK_ARROW_UP);
  gtk_box_pack_start (GTK_BOX (plugin->box), plugin->arrow, FALSE, FALSE, 0);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->arrow);
  gtk_button_set_relief (GTK_BUTTON (plugin->arrow), GTK_RELIEF_NONE);
  gtk_widget_set_name (plugin->button, "launcher-arrow");
  gtk_drag_dest_set (plugin->arrow, GTK_DEST_DEFAULT_MOTION,
                     drop_targets, G_N_ELEMENTS (drop_targets),
                     GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (plugin->arrow), "button-press-event",
      G_CALLBACK (launcher_plugin_arrow_press_event), plugin);
  g_signal_connect (G_OBJECT (plugin->arrow), "drag-motion",
      G_CALLBACK (launcher_plugin_arrow_drag_motion), plugin);
  g_signal_connect (G_OBJECT (plugin->arrow), "drag-leave",
      G_CALLBACK (launcher_plugin_arrow_drag_leave), plugin);

  /* sync button states */
  g_signal_connect (G_OBJECT (plugin->button), "state-changed",
      G_CALLBACK (launcher_plugin_button_state_changed), plugin->arrow);
  g_signal_connect (G_OBJECT (plugin->arrow), "state-changed",
      G_CALLBACK (launcher_plugin_button_state_changed), plugin->button);
}



static void
launcher_plugin_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (object);
  GPtrArray      *array;
  GValue         *tmp;
  GSList         *li;
  GFile          *item_file;

  switch (prop_id)
    {
    case PROP_ITEMS:
      array = g_ptr_array_new ();
      for (li = plugin->items; li != NULL; li = li->next)
        {
          tmp = g_new0 (GValue, 1);
          g_value_init (tmp, G_TYPE_STRING);
          panel_return_if_fail (GARCON_IS_MENU_ITEM (li->data));
          item_file = garcon_menu_item_get_file (li->data);
          if (g_file_has_prefix (item_file, plugin->config_directory))
            g_value_take_string (tmp, g_file_get_basename (item_file));
          else
            g_value_take_string (tmp, g_file_get_uri (item_file));
          g_object_unref (G_OBJECT (item_file));
          g_ptr_array_add (array, tmp);
        }
      g_value_set_boxed (value, array);
      xfconf_array_free (array);
      break;

    case PROP_DISABLE_TOOLTIPS:
      g_value_set_boolean (value, plugin->disable_tooltips);
      break;

    case PROP_MOVE_FIRST:
      g_value_set_boolean (value, plugin->move_first);
      break;

    case PROP_SHOW_LABEL:
      g_value_set_boolean (value, plugin->show_label);
      break;

    case PROP_ARROW_POSITION:
      g_value_set_uint (value, plugin->arrow_position);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
launcher_plugin_item_changed (GarconMenuItem *item,
                              LauncherPlugin *plugin)
{
  GSList *li;

  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* find the item */
  li = g_slist_find (plugin->items, item);
  if (G_LIKELY (li != NULL))
    {
      /* update the button or destroy the menu */
      if (plugin->items == li)
        launcher_plugin_button_update (plugin);
      else
        launcher_plugin_menu_destroy (plugin);
    }
  else
    {
      panel_assert_not_reached ();
    }
}



static gboolean
launcher_plugin_item_dupplicate (GFile   *src_file,
                                 GFile   *dst_file,
                                 GError **error)
{
  GKeyFile *key_file;
  gchar    *contents = NULL;
  gsize     length;
  gboolean  result = FALSE;
  gchar    *uri;

  panel_return_val_if_fail (G_IS_FILE (src_file), FALSE);
  panel_return_val_if_fail (G_IS_FILE (dst_file), FALSE);
  panel_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!g_file_load_contents (src_file, NULL, &contents, &length, NULL, error))
    return FALSE;

  /* note that we don't load the key file with preserving the translations
   * and comments, this way we save a small desktop file in the user's language */
  key_file = g_key_file_new ();
  if (!g_key_file_load_from_data (key_file, contents, length, 0, error))
    goto err1;

  /* store the source uri in the desktop file for resore purposes */
  uri = g_file_get_uri (src_file);
  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, "X-XFCE-Source", uri);
  g_free (uri);

  contents = g_key_file_to_data (key_file, &length, error);
  if (contents == NULL)
    goto err1;

  result = g_file_replace_contents (dst_file, contents, length, NULL, FALSE,
#if GLIB_CHECK_VERSION (2, 20, 0)
                                    G_FILE_CREATE_REPLACE_DESTINATION,
#else
                                    G_FILE_CREATE_NONE,
#endif
                                    NULL, NULL, error);

err1:
  g_free (contents);
  g_key_file_free (key_file);

  return result;
}



static GarconMenuItem *
launcher_plugin_item_load (LauncherPlugin *plugin,
                           const gchar    *str,
                           gboolean       *desktop_id_return)
{
  GFile          *src_file, *dst_file;
  gchar          *src_path, *dst_path;
  GSList         *li, *lnext;
  GarconMenuItem *item = NULL;
  GError         *error = NULL;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), NULL);
  panel_return_val_if_fail (str != NULL, NULL);
  panel_return_val_if_fail (G_IS_FILE (plugin->config_directory), NULL);

  if (G_UNLIKELY (g_path_is_absolute (str) || exo_str_looks_like_an_uri (str)))
    {
      src_file = g_file_new_for_commandline_arg (str);
      if (g_file_has_prefix (src_file, plugin->config_directory))
        {
          /* nothing, we use the file below */
        }
      else if (g_file_query_exists (src_file, NULL))
        {
          /* create a unique file in the config directory */
          dst_path = launcher_plugin_unique_filename (plugin);
          dst_file = g_file_new_for_path (dst_path);

          /* create a dupplicate in the config directory */
          if (launcher_plugin_item_dupplicate (src_file, dst_file, &error))
            {
              /* use the new file */
              g_object_unref (G_OBJECT (src_file));
              src_file = dst_file;
            }
          else
            {
              src_path = g_file_get_parse_name (src_file);
              g_warning ("Failed to create dupplicate of desktop file \"%s\" "
                          "to \"%s\": %s", src_path, dst_path, error->message);
              g_error_free (error);
              g_free (src_path);

              /* continue using the source file, the user won't be able to
               * edit the item, but atleast we have something that works in
               * the panel */
              g_object_unref (G_OBJECT (dst_path));
            }

          g_free (dst_path);
        }
      else
        {
          /* nothing we can do with this file */
          src_path = g_file_get_parse_name (src_file);
          g_warning ("Failed to load desktop file \"%s\". It will be removed "
                     "from the configuration", src_path);
          g_free (src_path);
          g_object_unref (G_OBJECT (src_file));

          return NULL;
        }
    }
  else
    {
      /* assume the file is a child in the config directory */
      src_file = g_file_get_child (plugin->config_directory, str);

      /* str might also be a global desktop id */
      if (G_LIKELY (desktop_id_return != NULL))
        *desktop_id_return = TRUE;
    }

  panel_assert (G_IS_FILE (src_file));

  /* maybe we have this file in the launcher configuration, then we don't
   * have to load it again from the harddisk */
  for (li = plugin->items; item == NULL && li != NULL; li = lnext)
    {
      lnext = li->next;
      dst_file = garcon_menu_item_get_file (GARCON_MENU_ITEM (li->data));
      if (g_file_equal (src_file, dst_file))
        {
          item = GARCON_MENU_ITEM (li->data);
          plugin->items = g_slist_delete_link (plugin->items, li);
        }
      g_object_unref (G_OBJECT (dst_file));
    }

  /* load the file from the disk */
  if (item == NULL)
    item = garcon_menu_item_new (src_file);

  g_object_unref (G_OBJECT (src_file));

  return item;
}



static void
launcher_plugin_items_free (GSList *items)
{
  if (items != NULL)
    {
      g_slist_foreach (items, (GFunc) g_object_unref, NULL);
      g_slist_free (items);
      items = NULL;
    }
}



static void
launcher_plugin_items_load (LauncherPlugin *plugin,
                            GPtrArray      *array)
{
  guint           i;
  const GValue   *value;
  const gchar    *str;
  GarconMenuItem *item;
  GarconMenuItem *pool_item;
  GSList         *items = NULL;
  GHashTable     *pool = NULL;
  gboolean        desktop_id;
  gchar          *uri;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));
  panel_return_if_fail (array != NULL);

  for (i = 0; i < array->len; i++)
    {
      value = g_ptr_array_index (array, i);
      panel_assert (G_VALUE_HOLDS_STRING (value));
      str = g_value_get_string (value);

      /* only accept desktop files */
      if (str == NULL || !g_str_has_suffix (str, ".desktop"))
        continue;

      /* try to load the item */
      desktop_id = FALSE;
      item = launcher_plugin_item_load (plugin, str, &desktop_id);
      if (G_LIKELY (item == NULL))
        {
          /* str did not look like a desktop-id, so no need to look
           * for it in the application pool */
          if (!desktop_id)
            continue;

          /* load the pool with desktop items */
          if (pool == NULL)
            pool = launcher_plugin_garcon_menu_pool ();

          /* lookup the item in the item pool */
          pool_item = g_hash_table_lookup (pool, str);
          if (pool_item != NULL)
            {
              /* we want an editable file, so try to make a copy */
              uri = garcon_menu_item_get_uri (pool_item);
              item = launcher_plugin_item_load (plugin, uri, NULL);
              g_free (uri);

              /* if something failed, use the pool item, but this one
               * won't be editable in the dialog */
              if (G_UNLIKELY (item == NULL))
                item = g_object_ref (G_OBJECT (pool_item));
            }

          /* skip this item if still not found */
          if (item == NULL)
            continue;
        }

      /* add the item to the list */
      panel_assert (GARCON_IS_MENU_ITEM (item));
      items = g_slist_append (items, item);
      g_signal_connect (G_OBJECT (item), "changed",
          G_CALLBACK (launcher_plugin_item_changed), plugin);
    }

  if (G_UNLIKELY (pool != NULL))
    g_hash_table_destroy (pool);

  /* release the old menu items and set new one */
  launcher_plugin_items_free (plugin->items);
  plugin->items = items;
}



static void
launcher_plugin_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (object);
  GPtrArray      *array;

  panel_return_if_fail (G_IS_FILE (plugin->config_directory));

  /* destroy the menu, all the setting changes need this */
  launcher_plugin_menu_destroy (plugin);

  switch (prop_id)
    {
    case PROP_ITEMS:
      /* load new items from the array */
      array = g_value_get_boxed (value);
      if (G_LIKELY (array != NULL))
        launcher_plugin_items_load (plugin, array);
      else
        launcher_plugin_items_free (plugin->items);

      /* emit signal */
      g_signal_emit (G_OBJECT (plugin), launcher_signals[ITEMS_CHANGED], 0);

      /* update the button */
      launcher_plugin_button_update (plugin);

      /* update the widget packing */
      goto update_arrow;
      break;

    case PROP_DISABLE_TOOLTIPS:
      plugin->disable_tooltips = g_value_get_boolean (value);
      gtk_widget_set_has_tooltip (plugin->button, !plugin->disable_tooltips);
      break;

    case PROP_MOVE_FIRST:
      plugin->move_first = g_value_get_boolean (value);
      break;

    case PROP_SHOW_LABEL:
      plugin->show_label = g_value_get_boolean (value);

      /* destroy the old child */
      if (plugin->child != NULL)
        gtk_widget_destroy (plugin->child);

      /* create child */
      if (G_UNLIKELY (plugin->show_label))
        plugin->child = gtk_label_new (NULL);
      else
        plugin->child = xfce_panel_image_new ();
      gtk_container_add (GTK_CONTAINER (plugin->button), plugin->child);
      gtk_widget_show (plugin->child);

      /* update size */
      launcher_plugin_size_changed (XFCE_PANEL_PLUGIN (plugin),
          xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin)));

      /* update the button */
      launcher_plugin_button_update (plugin);
      break;

    case PROP_ARROW_POSITION:
      plugin->arrow_position = g_value_get_uint (value);

update_arrow:
      /* update the arrow button visibility */
      launcher_plugin_arrow_visibility (plugin);

      /* repack the widgets */
      launcher_plugin_pack_widgets (plugin);

      /* update the plugin size */
      launcher_plugin_size_changed (XFCE_PANEL_PLUGIN (plugin),
          xfce_panel_plugin_get_size (XFCE_PANEL_PLUGIN (plugin)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
launcher_plugin_file_changed (GFileMonitor      *monitor,
                              GFile             *changed_file,
                              GFile             *other_file,
                              GFileMonitorEvent  event_type,
                              LauncherPlugin    *plugin)
{
  GSList         *li, *lnext;
  GarconMenuItem *item;
  GFile          *item_file;
  gboolean        found;
  GError         *error = NULL;
  gchar          *base_name;
  gboolean        result;
  gboolean        exists;
  gboolean        update_plugin = FALSE;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));
  panel_return_if_fail (plugin->config_monitor == monitor);

  /* waited until all events are proccessed */
  if (event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT
      && event_type != G_FILE_MONITOR_EVENT_DELETED)
    return;

  /* we only act on desktop files */
  base_name = g_file_get_basename (changed_file);
  result = g_str_has_suffix (base_name, ".desktop");
  g_free (base_name);
  if (!result)
    return;

  exists = g_file_query_exists (changed_file, NULL);

  /* lookup the file in the menu items */
  for (li = plugin->items, found = FALSE; !found && li != NULL; li = lnext)
    {
      lnext = li->next;
      item = GARCON_MENU_ITEM (li->data);
      item_file = garcon_menu_item_get_file (item);
      found = g_file_equal (changed_file, item_file);
      if (found)
        {
          if (exists)
            {
              /* reload the file */
              if (!garcon_menu_item_reload (item, &error))
                {
                  g_critical ("Failed to reload menu item: %s", error->message);
                  g_error_free (error);
                }
            }
          else
            {
              /* remove from the list */
              plugin->items = g_slist_delete_link (plugin->items, li);
              g_object_unref (G_OBJECT (item));
              update_plugin = TRUE;
            }
        }
      g_object_unref (G_OBJECT (item_file));
    }

  if (!found && exists)
    {
      /* add the new file to the config */
      item = garcon_menu_item_new (changed_file);
      if (G_LIKELY (item != NULL))
        {
          plugin->items = g_slist_append (plugin->items, item);
          g_signal_connect (G_OBJECT (item), "changed",
              G_CALLBACK (launcher_plugin_item_changed), plugin);
          update_plugin = TRUE;
        }
    }

  if (update_plugin)
    {
      launcher_plugin_button_update (plugin);
      launcher_plugin_menu_destroy (plugin);

      /* save the new config */
      g_object_notify (G_OBJECT (plugin), "items");

      /* update the dialog */
      g_signal_emit (G_OBJECT (plugin), launcher_signals[ITEMS_CHANGED], 0);
    }
}



static void
launcher_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  LauncherPlugin      *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);
  const gchar * const *uris;
  guint                i;
  GPtrArray           *array;
  GValue              *value;
  gchar               *file, *path;
  GError              *error = NULL;
  const PanelProperty  properties[] =
  {
    { "show-label", G_TYPE_BOOLEAN },
    { "items", PANEL_PROPERTIES_TYPE_VALUE_ARRAY },
    { "disable-tooltips", G_TYPE_BOOLEAN },
    { "move-first", G_TYPE_BOOLEAN },
    { "arrow-position", G_TYPE_UINT },
    { NULL }
  };

  /* show the configure menu item */
  xfce_panel_plugin_menu_show_configure (XFCE_PANEL_PLUGIN (plugin));

  /* lookup the config directory where this launcher stores it's desktop files */
  file = g_strdup_printf (RELATIVE_CONFIG_PATH,
                          xfce_panel_plugin_get_name (XFCE_PANEL_PLUGIN (plugin)),
                          xfce_panel_plugin_get_unique_id (XFCE_PANEL_PLUGIN (plugin)));
  path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, file, FALSE);
  plugin->config_directory = g_file_new_for_path (path);
  g_free (file);
  g_free (path);

  /* bind all properties */
  panel_properties_bind (NULL, G_OBJECT (plugin),
                         xfce_panel_plugin_get_property_base (panel_plugin),
                         properties, FALSE);

  /* handle and empty plugin */
  if (G_UNLIKELY (plugin->items == NULL))
    {
      /* get the plugin arguments list */
      uris = xfce_panel_plugin_get_arguments (panel_plugin);
      if (G_LIKELY (uris != NULL))
        {
          /* create array with all the uris */
          array = g_ptr_array_new ();
          for (i = 0; uris[i] != NULL; i++)
            {
              value = g_new0 (GValue, 1);
              g_value_init (value, G_TYPE_STRING);
              g_value_set_static_string (value, uris[i]);
              g_ptr_array_add (array, value);
            }

          /* set new file list */
          if (G_LIKELY (array->len > 0))
            g_object_set (G_OBJECT (plugin), "items", array, NULL);
          xfconf_array_free (array);
        }
      else
        {
          /* update the icon */
          launcher_plugin_button_update (plugin);
        }
    }

  /* start file monitor in our config directory */
  plugin->config_monitor = g_file_monitor_directory (plugin->config_directory,
                                                     G_FILE_MONITOR_NONE, NULL, &error);
  if (G_LIKELY (plugin->config_monitor != NULL))
    {
      g_signal_connect (G_OBJECT (plugin->config_monitor), "changed",
                        G_CALLBACK (launcher_plugin_file_changed), plugin);
    }
  else
    {
      g_critical ("Failed to start file monitor: %s", error->message);
      g_error_free (error);
    }

  /* show the beast */
  gtk_widget_show (plugin->box);
  gtk_widget_show (plugin->button);
  gtk_widget_show (plugin->child);
}



static void
launcher_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);
  GtkIconTheme   *icon_theme;

  /* stop monitoring */
  if (plugin->config_monitor != NULL)
    {
      g_file_monitor_cancel (plugin->config_monitor);
      g_object_unref (G_OBJECT (plugin->config_monitor));
    }

  if (plugin->config_directory != NULL)
    g_object_unref (G_OBJECT (plugin->config_directory));

  /* destroy the menu and timeout */
  launcher_plugin_menu_destroy (plugin);

  /* free items */
  launcher_plugin_items_free (plugin->items);

  /* stop watching the icon theme */
  if (plugin->theme_change_id != 0)
    {
      icon_theme = gtk_icon_theme_get_default ();
      g_signal_handler_disconnect (G_OBJECT (icon_theme), plugin->theme_change_id);
    }

  /* release the cached tooltip */
  if (plugin->tooltip_cache != NULL)
    g_object_unref (G_OBJECT (plugin->tooltip_cache));
}



static void
launcher_plugin_removed (XfcePanelPlugin *panel_plugin)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);
  GError         *error = NULL;
  GSList         *li;
  GFile          *item_file;
  gboolean        result = TRUE;

  panel_return_if_fail (G_IS_FILE (plugin->config_directory));

  /* leave if there is not config */
  if (!g_file_query_exists (plugin->config_directory, NULL))
    return;

  /* stop monitoring */
  if (plugin->config_monitor != NULL)
    {
      g_file_monitor_cancel (plugin->config_monitor);
      g_object_unref (G_OBJECT (plugin->config_monitor));
      plugin->config_monitor = NULL;
    }

  /* cleanup desktop files in the config dir */
  for (li = plugin->items; result && li != NULL; li = li->next)
    {
      item_file = garcon_menu_item_get_file (li->data);
      if (g_file_has_prefix (item_file, plugin->config_directory))
        result = g_file_delete (item_file, NULL, &error);
      g_object_unref (G_OBJECT (item_file));
    }

  /* remove config dir if everything went fine */
  if (result)
    result = g_file_delete (plugin->config_directory, NULL, &error);

  if (!result)
    {
      g_message ("launcher-%d: Failed to cleanup the configuration: %s",
                 xfce_panel_plugin_get_unique_id (panel_plugin),
                 error->message);
      g_error_free (error);
    }
}



static gboolean
launcher_plugin_remote_event (XfcePanelPlugin *panel_plugin,
                              const gchar     *name,
                              const GValue    *value)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);

  panel_return_val_if_fail (value == NULL || G_IS_VALUE (value), FALSE);

  if (exo_str_is_equal (name, "popup")
      && LIST_HAS_TWO_OR_MORE_ENTRIES (plugin->items)
      && (plugin->menu == NULL || !GTK_WIDGET_VISIBLE (plugin->menu)))
    {
      launcher_plugin_menu_popup (plugin);

      return TRUE;
    }

  if (exo_str_is_equal (name, "disable-tooltips")
      && value != NULL
      && G_VALUE_HOLDS_BOOLEAN (value))
    {
      g_object_set_property (G_OBJECT (plugin), "disable-tooltips", value);

      return FALSE;
    }

  return FALSE;
}



static void
launcher_plugin_save (XfcePanelPlugin *panel_plugin)
{
  /* make sure the items are stored */
  g_object_notify (G_OBJECT (panel_plugin), "items");
}



static void
launcher_plugin_orientation_changed (XfcePanelPlugin *panel_plugin,
                                     GtkOrientation   orientation)
{
  /* update the widget order */
  launcher_plugin_pack_widgets (XFCE_LAUNCHER_PLUGIN (panel_plugin));

  /* update the arrow button */
  launcher_plugin_screen_position_changed (panel_plugin,
      xfce_panel_plugin_get_screen_position (panel_plugin));

  /* update the plugin size */
  launcher_plugin_size_changed (panel_plugin,
      xfce_panel_plugin_get_size (panel_plugin));
}



static gboolean
launcher_plugin_size_changed (XfcePanelPlugin *panel_plugin,
                              gint             size)
{
  LauncherPlugin    *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);
  gint               p_width, p_height;
  gint               a_width, a_height;
  gboolean           horizontal;
  LauncherArrowType  arrow_position;

  /* initialize the plugin size */
  p_width = p_height = size;
  a_width = a_height = -1;

  /* add the arrow size */
  if (GTK_WIDGET_VISIBLE (plugin->arrow))
    {
      /* if the panel is horizontal */
      horizontal = !!(xfce_panel_plugin_get_orientation (panel_plugin) ==
          GTK_ORIENTATION_HORIZONTAL);

      /* set tmp arrow position */
      arrow_position = plugin->arrow_position;
      if (arrow_position == LAUNCHER_ARROW_DEFAULT)
        arrow_position = horizontal ? LAUNCHER_ARROW_WEST : LAUNCHER_ARROW_NORTH;

      switch (plugin->arrow_position)
        {
        case LAUNCHER_ARROW_NORTH:
        case LAUNCHER_ARROW_SOUTH:
          a_height = ARROW_BUTTON_SIZE;

          if (horizontal)
            p_width -= ARROW_BUTTON_SIZE;
          else
            p_height += ARROW_BUTTON_SIZE;
          break;

        default:
          a_width = ARROW_BUTTON_SIZE;

          if (horizontal)
            p_width += ARROW_BUTTON_SIZE;
          else
            p_height += ARROW_BUTTON_SIZE;
          break;
        }

      /* set the arrow size */
      gtk_widget_set_size_request (plugin->arrow, a_width, a_height);
    }

  /* set the panel plugin size */
  if (plugin->show_label)
    gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), -1, -1);
  else
    gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), p_width, p_height);

  return TRUE;
}



static void
launcher_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
  /* run the configure dialog */
  launcher_dialog_show (XFCE_LAUNCHER_PLUGIN (panel_plugin));
}



static void
launcher_plugin_screen_position_changed (XfcePanelPlugin    *panel_plugin,
                                         XfceScreenPosition  position)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (panel_plugin);

  /* set the new arrow direction */
  xfce_arrow_button_set_arrow_type (XFCE_ARROW_BUTTON (plugin->arrow),
      xfce_panel_plugin_arrow_type (panel_plugin));
}



static void
launcher_plugin_icon_theme_changed (GtkIconTheme   *icon_theme,
                                    LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));
  panel_return_if_fail (GTK_IS_ICON_THEME (icon_theme));

  /* invalid the icon cache */
  if (plugin->tooltip_cache != NULL)
    {
      g_object_unref (G_OBJECT (plugin->tooltip_cache));
      plugin->tooltip_cache = NULL;
    }
}



static void
launcher_plugin_pack_widgets (LauncherPlugin *plugin)
{
  LauncherArrowType pos = plugin->arrow_position;
  gboolean          rtl;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* leave when the arrow button is not visible */
  if (!GTK_WIDGET_VISIBLE (plugin->arrow)
      || pos == LAUNCHER_ARROW_INTERNAL)
    return;

  if (pos == LAUNCHER_ARROW_DEFAULT)
    {
      /* get the plugin direction */
      rtl = !!(gtk_widget_get_direction (GTK_WIDGET (plugin)) == GTK_TEXT_DIR_RTL);

      if (xfce_panel_plugin_get_orientation (XFCE_PANEL_PLUGIN (plugin)) ==
              GTK_ORIENTATION_HORIZONTAL)
        pos = rtl ? LAUNCHER_ARROW_WEST : LAUNCHER_ARROW_EAST;
      else
        pos = rtl ? LAUNCHER_ARROW_NORTH : LAUNCHER_ARROW_SOUTH;
    }

  /* set the position of the arrow button in the box */
  gtk_box_reorder_child (GTK_BOX (plugin->box), plugin->arrow,
      (pos == LAUNCHER_ARROW_WEST || pos == LAUNCHER_ARROW_NORTH) ? 0 : -1);

  /* set the orientation of the hvbox */
  xfce_hvbox_set_orientation (XFCE_HVBOX (plugin->box),
      !!(pos == LAUNCHER_ARROW_WEST || pos == LAUNCHER_ARROW_EAST) ?
          GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
}



static GdkPixbuf *
launcher_plugin_tooltip_pixbuf (GdkScreen   *screen,
                                const gchar *icon_name)
{
  GtkIconTheme *theme;
  gint          w, h, size;

  panel_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), NULL);

  if (exo_str_is_empty (icon_name))
    return NULL;

  if (gtk_icon_size_lookup (launcher_tooltip_icon_size, &w, &h))
    size = MIN (w, h);
  else
    size = TOOLTIP_ICON_SIZE;

  /* load directly from a file */
  if (G_UNLIKELY (g_path_is_absolute (icon_name)))
    return exo_gdk_pixbuf_new_from_file_at_max_size (icon_name, size, size, TRUE, NULL);

  if (G_LIKELY (screen != NULL))
    theme = gtk_icon_theme_get_for_screen (screen);
  else
    theme = gtk_icon_theme_get_default ();

  return gtk_icon_theme_load_icon (theme, icon_name, size, 0, NULL);
}



static void
launcher_plugin_menu_deactivate (GtkWidget      *menu,
                                 LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));
  panel_return_if_fail (plugin->menu == menu);

  /* deactivate the arrow button */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);
}



static void
launcher_plugin_menu_item_activate (GtkMenuItem      *widget,
                                    GarconMenuItem   *item)
{
  LauncherPlugin *plugin;
  GdkScreen      *screen;
  GdkEvent       *event;
  guint32         event_time;

  panel_return_if_fail (GTK_IS_MENU_ITEM (widget));
  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* get a copy of the event causing the menu item to activate */
  event = gtk_get_current_event ();
  event_time = gdk_event_get_time (event);

  /* get the widget screen */
  screen = gtk_widget_get_screen (GTK_WIDGET (widget));

  /* launch the command */
  if (event != NULL
      && event->type == GDK_BUTTON_RELEASE
      && event->button.button == 2)
    launcher_plugin_item_exec_from_clipboard (item, event_time, screen);
  else
    launcher_plugin_item_exec (item, event_time, screen, NULL);

  if (event != NULL)
    gdk_event_free (event);

  /* get the plugin */
  plugin = g_object_get_qdata (G_OBJECT (widget), launcher_plugin_quark);
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* move the item to the first position if enabled */
  if (G_UNLIKELY (plugin->move_first))
    {
      /* prepend the item in the list */
      plugin->items = g_slist_remove (plugin->items, item);
      plugin->items = g_slist_prepend (plugin->items, item);

      /* destroy the menu and update the icon */
      launcher_plugin_menu_destroy (plugin);
      launcher_plugin_button_update (plugin);
    }
}



static void
launcher_plugin_menu_item_drag_data_received (GtkWidget          *widget,
                                              GdkDragContext     *context,
                                              gint                x,
                                              gint                y,
                                              GtkSelectionData   *data,
                                              guint               info,
                                              guint               drag_time,
                                              GarconMenuItem     *item)
{
  LauncherPlugin *plugin;
  GSList         *uri_list;

  panel_return_if_fail (GTK_IS_MENU_ITEM (widget));
  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* get the plugin */
  plugin = g_object_get_qdata (G_OBJECT (widget), launcher_plugin_quark);
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* extract the uris from the selection data */
  uri_list = launcher_plugin_uri_list_extract (data);
  if (G_LIKELY (uri_list != NULL))
    {
      /* execute the menu item */
      launcher_plugin_item_exec (item, drag_time,
                                 gtk_widget_get_screen (widget),
                                 uri_list);

      launcher_plugin_uri_list_free (uri_list);
    }

  /* hide the menu */
  gtk_widget_hide (GTK_MENU (plugin->menu)->toplevel);
  gtk_widget_hide (plugin->menu);

  /* inactivate the toggle button */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);

  /* finish the drag */
  gtk_drag_finish (context, TRUE, FALSE, drag_time);
}



static void
launcher_plugin_menu_construct (LauncherPlugin *plugin)
{
  GtkArrowType    arrow_type;
  guint           n;
  GarconMenuItem *item;
  GtkWidget      *mi, *image;
  const gchar    *name, *icon_name;
  GSList         *li;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));
  panel_return_if_fail (plugin->menu == NULL);

  /* create a new menu */
  plugin->menu = gtk_menu_new ();
  gtk_menu_attach_to_widget (GTK_MENU (plugin->menu), GTK_WIDGET (plugin), NULL);
  g_signal_connect (G_OBJECT (plugin->menu), "deactivate",
      G_CALLBACK (launcher_plugin_menu_deactivate), plugin);

  /* get the arrow type of the plugin */
  arrow_type = xfce_arrow_button_get_arrow_type (XFCE_ARROW_BUTTON (plugin->arrow));

  /* walk through the menu entries */
  for (li = plugin->items, n = 0; li != NULL; li = li->next, n++)
    {
      /* skip the first entry when the arrow is visible */
      if (n == 0 && plugin->arrow_position != LAUNCHER_ARROW_INTERNAL)
        continue;

      /* get the item data */
      item = GARCON_MENU_ITEM (li->data);

      /* create the menu item */
      name = garcon_menu_item_get_name (item);
      mi = gtk_image_menu_item_new_with_label (
          exo_str_is_empty (name) ? _("Unnamed Item") : name);
      g_object_set_qdata (G_OBJECT (mi), launcher_plugin_quark, plugin);
      gtk_widget_show (mi);
      gtk_drag_dest_set (mi, GTK_DEST_DEFAULT_ALL, drop_targets,
                         G_N_ELEMENTS (drop_targets), GDK_ACTION_COPY);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (launcher_plugin_menu_item_activate), item);
      g_signal_connect (G_OBJECT (mi), "drag-data-received",
          G_CALLBACK (launcher_plugin_menu_item_drag_data_received), item);
      g_signal_connect (G_OBJECT (mi), "drag-leave",
          G_CALLBACK (launcher_plugin_arrow_drag_leave), plugin);

      /* only connect the tooltip signal if tips are enabled */
      if (!plugin->disable_tooltips)
        {
          gtk_widget_set_has_tooltip (mi, TRUE);
          g_signal_connect (G_OBJECT (mi), "query-tooltip",
              G_CALLBACK (launcher_plugin_item_query_tooltip), item);
        }

      /* depending on the menu position we prepend or append */
      if (G_UNLIKELY (arrow_type == GTK_ARROW_DOWN))
        gtk_menu_shell_append (GTK_MENU_SHELL (plugin->menu), mi);
      else
        gtk_menu_shell_prepend (GTK_MENU_SHELL (plugin->menu), mi);

      /* set the icon if one is set */
      icon_name = garcon_menu_item_get_icon_name (item);
      if (!exo_str_is_empty (icon_name))
        {
          image = gtk_image_new_from_icon_name (icon_name, launcher_menu_icon_size);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
          gtk_widget_show (image);
        }
    }
}



static void
launcher_plugin_menu_popup_destroyed (gpointer user_data)
{
   XFCE_LAUNCHER_PLUGIN (user_data)->menu_timeout_id = 0;
}



static gboolean
launcher_plugin_menu_popup (gpointer user_data)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (user_data);
  gint            x, y;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  GDK_THREADS_ENTER ();

  /* construct the menu if needed */
  if (plugin->menu == NULL)
    launcher_plugin_menu_construct (plugin);

  /* toggle the arrow button */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), TRUE);

  /* popup the menu */
  gtk_menu_popup (GTK_MENU (plugin->menu), NULL, NULL,
                  xfce_panel_plugin_position_menu,
                  XFCE_PANEL_PLUGIN (plugin), 1,
                  gtk_get_current_event_time ());

  /* fallback to manual positioning, this is used with
   * drag motion over the arrow button */
  if (!GTK_WIDGET_VISIBLE (plugin->menu))
    {
      /* make sure the size is allocated */
      if (!GTK_WIDGET_REALIZED (plugin->menu))
        gtk_widget_realize (plugin->menu);

      /* use the widget position function to get the coordinates */
      xfce_panel_plugin_position_widget (XFCE_PANEL_PLUGIN (plugin),
                                         plugin->menu, NULL, &x, &y);

      /* bit ugly... but show the menu */
      gtk_widget_show (plugin->menu);
      gtk_window_move (GTK_WINDOW (GTK_MENU (plugin->menu)->toplevel), x, y);
      gtk_widget_show (GTK_MENU (plugin->menu)->toplevel);
    }

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
launcher_plugin_menu_destroy (LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* stop pending timeout */
  if (plugin->menu_timeout_id != 0)
    g_source_remove (plugin->menu_timeout_id);

  if (plugin->menu != NULL)
    {
      /* destroy the menu */
      gtk_widget_destroy (plugin->menu);
      plugin->menu = NULL;

      /* deactivate the toggle button */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);
    }
}



static void
launcher_plugin_button_update (LauncherPlugin *plugin)
{
  GarconMenuItem *item = NULL;
  const gchar    *icon_name;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* invalate the tooltip icon cache */
  if (plugin->tooltip_cache != NULL)
    {
      g_object_unref (G_OBJECT (plugin->tooltip_cache));
      plugin->tooltip_cache = NULL;
    }

  /* get first item */
  if (G_LIKELY (plugin->items != NULL))
    item = GARCON_MENU_ITEM (plugin->items->data);

  if (G_UNLIKELY (plugin->show_label))
    {
      panel_return_if_fail (GTK_IS_LABEL (plugin->child));
      gtk_label_set_text (GTK_LABEL (plugin->child),
          item != NULL ? garcon_menu_item_get_name (item) : _("No items"));
    }
  else if (G_LIKELY (item != NULL))
    {
      panel_return_if_fail (XFCE_IS_PANEL_IMAGE (plugin->child));

      icon_name = garcon_menu_item_get_icon_name (item);
      xfce_panel_image_set_from_source (XFCE_PANEL_IMAGE (plugin->child),
          exo_str_is_empty (icon_name) ? GTK_STOCK_MISSING_IMAGE : icon_name);
    }
  else
    {
      /* set missing image icon */
      panel_return_if_fail (XFCE_IS_PANEL_IMAGE (plugin->child));
      xfce_panel_image_set_from_source (XFCE_PANEL_IMAGE (plugin->child),
                                        GTK_STOCK_MISSING_IMAGE);
    }
}



static void
launcher_plugin_button_state_changed (GtkWidget    *button_a,
                                      GtkStateType  state,
                                      GtkWidget    *button_b)
{
  if (GTK_WIDGET_STATE (button_a) != GTK_WIDGET_STATE (button_b)
      && GTK_WIDGET_STATE (button_a) != GTK_STATE_INSENSITIVE)
    gtk_widget_set_state (button_b, GTK_WIDGET_STATE (button_a));
}



static gboolean
launcher_plugin_button_press_event (GtkWidget      *button,
                                    GdkEventButton *event,
                                    LauncherPlugin *plugin)
{
  guint modifiers;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* do nothing on anything else then a single click */
  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  /* get the default accelerator modifier mask */
  modifiers = event->state & gtk_accelerator_get_default_mod_mask ();

  /* leave when button 1 is not pressed or shift is pressed */
  if (event->button != 1 || modifiers == GDK_CONTROL_MASK)
    return FALSE;

  if (ARROW_INSIDE_BUTTON (plugin))
    {
      /* directly popup the menu */
      launcher_plugin_menu_popup (plugin);
    }
  else if (plugin->menu_timeout_id == 0
           && LIST_HAS_TWO_OR_MORE_ENTRIES (plugin->items))
    {
      /* start the popup timeout */
      plugin->menu_timeout_id =
          g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
                              MENU_POPUP_DELAY,
                              launcher_plugin_menu_popup, plugin,
                              launcher_plugin_menu_popup_destroyed);
    }

  return FALSE;
}



static gboolean
launcher_plugin_button_release_event (GtkWidget      *button,
                                      GdkEventButton *event,
                                      LauncherPlugin *plugin)
{
  GarconMenuItem *item;
  GdkScreen      *screen;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* remove a delayed popup timeout */
  if (plugin->menu_timeout_id != 0)
    g_source_remove (plugin->menu_timeout_id);

  /* leave when there are no menu items or there is an internal arrow */
  if (plugin->items == NULL
      || !GTK_BUTTON (button)->in_button
      || ARROW_INSIDE_BUTTON (plugin))
    return FALSE;

  /* get the menu item and the screen */
  item = GARCON_MENU_ITEM (plugin->items->data);
  screen = gtk_widget_get_screen (button);

  /* launcher the entry */
  if (event->button == 1)
    launcher_plugin_item_exec (item, event->time, screen, NULL);
  else if (event->button == 2)
    launcher_plugin_item_exec_from_clipboard (item, event->time, screen);
  else
    return TRUE;

  return FALSE;
}



static gboolean
launcher_plugin_button_query_tooltip (GtkWidget      *widget,
                                      gint            x,
                                      gint            y,
                                      gboolean        keyboard_mode,
                                      GtkTooltip     *tooltip,
                                      LauncherPlugin *plugin)
{
  gboolean        result;
  GarconMenuItem *item;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);
  panel_return_val_if_fail (!plugin->disable_tooltips, FALSE);

  /* check if we show tooltips */
  if (plugin->arrow_position == LAUNCHER_ARROW_INTERNAL
      || plugin->items == NULL
      || plugin->items->data == NULL)
    return FALSE;

  /* get the first item */
  item = GARCON_MENU_ITEM (plugin->items->data);

  /* handle the basic tooltip data */
  result = launcher_plugin_item_query_tooltip (widget, x, y, keyboard_mode, tooltip, item);
  if (G_LIKELY (result))
    {
      /* set the cached icon if not already set */
      if (G_UNLIKELY (plugin->tooltip_cache == NULL))
        plugin->tooltip_cache =
            launcher_plugin_tooltip_pixbuf (gtk_widget_get_screen (widget),
                                            garcon_menu_item_get_icon_name (item));

      if (G_LIKELY (plugin->tooltip_cache != NULL))
        gtk_tooltip_set_icon (tooltip, plugin->tooltip_cache);
    }

  return result;
}



static void
launcher_plugin_button_drag_data_received (GtkWidget        *widget,
                                           GdkDragContext   *context,
                                           gint              x,
                                           gint              y,
                                           GtkSelectionData *selection_data,
                                           guint             info,
                                           guint             drag_time,
                                           LauncherPlugin   *plugin)
{
  GSList *uri_list;

  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* leave when there are not items or the arrow is internal */
  if (ARROW_INSIDE_BUTTON (plugin) || plugin->items == NULL)
    return;

  /* get the list of uris from the selection data */
  uri_list = launcher_plugin_uri_list_extract (selection_data);
  if (G_LIKELY (uri_list != NULL))
    {
      /* execute */
      launcher_plugin_item_exec (GARCON_MENU_ITEM (plugin->items->data),
                                 gtk_get_current_event_time (),
                                 gtk_widget_get_screen (widget),
                                 uri_list);

      launcher_plugin_uri_list_free (uri_list);
    }

  /* finish the drag */
  gtk_drag_finish (context, TRUE, FALSE, drag_time);
}



static gboolean
launcher_plugin_button_drag_motion (GtkWidget      *widget,
                                    GdkDragContext *context,
                                    gint            x,
                                    gint            y,
                                    guint           drag_time,
                                    LauncherPlugin *plugin)
{
  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* do nothing if the plugin is empty */
  if (plugin->items == NULL)
    {
      /* not a drop zone */
      gdk_drag_status (context, 0, drag_time);
      return FALSE;
    }

  /* highlight the button if this is a launcher button */
  if (NO_ARROW_INSIDE_BUTTON (plugin))
    {
      gtk_drag_highlight (widget);
      return TRUE;
    }

  /* handle the popup menu */
  return launcher_plugin_arrow_drag_motion (widget, context, x, y,
                                            drag_time, plugin);
}



static void
launcher_plugin_button_drag_leave (GtkWidget      *widget,
                                   GdkDragContext *context,
                                   guint           drag_time,
                                   LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  /* unhighlight the widget or make sure the menu is deactivated */
  if (NO_ARROW_INSIDE_BUTTON (plugin))
    gtk_drag_unhighlight (widget);
  else
    launcher_plugin_arrow_drag_leave (widget, context, drag_time, plugin);
}



static gboolean
launcher_plugin_button_expose_event (GtkWidget      *widget,
                                     GdkEventExpose *event,
                                     LauncherPlugin *plugin)
{
  GtkArrowType arrow_type;
  gint         size, x, y, thickness, offset;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* leave when the arrow is not shown inside the button */
  if (NO_ARROW_INSIDE_BUTTON (plugin))
    return FALSE;

  /* get the arrow type */
  arrow_type = xfce_arrow_button_get_arrow_type (XFCE_ARROW_BUTTON (plugin->arrow));

  /* style thickness */
  thickness = MAX (widget->style->xthickness, widget->style->ythickness);

  /* size of the arrow and the start coordinates */
  size = widget->allocation.width / 3;
  x = widget->allocation.x + thickness;
  y = widget->allocation.y + thickness;
  offset = size + 2 * thickness;

  /* calculate the position based on the arrow type */
  switch (arrow_type)
    {
    case GTK_ARROW_UP:
      /* north east */
      x += widget->allocation.width - offset;
      break;

    case GTK_ARROW_DOWN:
      /* south west */
      y += widget->allocation.height - offset;
      break;

    case GTK_ARROW_RIGHT:
      /* south east */
      x += widget->allocation.width - offset;
      y += widget->allocation.height - offset;
      break;

    default:
      /* north west */
      break;
    }

  /* paint the arrow */
  gtk_paint_arrow (widget->style, widget->window,
                   GTK_WIDGET_STATE (widget), GTK_SHADOW_IN,
                   &(event->area), widget, "launcher_button",
                   arrow_type, TRUE, x, y, size, size);

  return FALSE;
}



static void
launcher_plugin_arrow_visibility (LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  if (plugin->arrow_position != LAUNCHER_ARROW_INTERNAL
       && LIST_HAS_TWO_OR_MORE_ENTRIES (plugin->items))
    gtk_widget_show (plugin->arrow);
  else
    gtk_widget_hide (plugin->arrow);
}



static gboolean
launcher_plugin_arrow_press_event (GtkWidget      *button,
                                   GdkEventButton *event,
                                   LauncherPlugin *plugin)
{
  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* only popup when button 1 is pressed */
  if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
    {
      launcher_plugin_menu_popup (plugin);
      return FALSE;
    }

  return TRUE;
}




static gboolean
launcher_plugin_arrow_drag_motion (GtkWidget      *widget,
                                   GdkDragContext *context,
                                   gint            x,
                                   gint            y,
                                   guint           drag_time,
                                   LauncherPlugin *plugin)
{
  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);

  /* the arrow is not a drop zone */
  gdk_drag_status (context, 0, drag_time);

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (plugin->arrow)))
    {
      /* make the toggle button active */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), TRUE);

      /* start the popup timeout */
      plugin->menu_timeout_id =
          g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, MENU_POPUP_DELAY,
                              launcher_plugin_menu_popup, plugin,
                              launcher_plugin_menu_popup_destroyed);

      return TRUE;
    }

  return FALSE;
}



static gboolean
launcher_plugin_arrow_drag_leave_timeout (gpointer user_data)
{
  LauncherPlugin *plugin = XFCE_LAUNCHER_PLUGIN (user_data);
  gint            pointer_x, pointer_y;
  GtkWidget      *menu = plugin->menu;
  gint            menu_x, menu_y, menu_w, menu_h;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);
  panel_return_val_if_fail (menu == NULL || GDK_IS_WINDOW (menu->window), FALSE);

  /* leave when the menu is destroyed */
  if (G_UNLIKELY (plugin->menu == NULL))
    return FALSE;

  /* get the pointer position */
  gdk_display_get_pointer (gtk_widget_get_display (menu),
                           NULL, &pointer_x, &pointer_y, NULL);

  /* get the menu position */
  gdk_window_get_root_origin (menu->window, &menu_x, &menu_y);
  gdk_drawable_get_size (GDK_DRAWABLE (menu->window), &menu_w, &menu_h);

  /* check if we should hide the menu */
  if (pointer_x < menu_x || pointer_x > menu_x + menu_w
      || pointer_y < menu_y || pointer_y > menu_y + menu_h)
    {
      /* hide the menu */
      gtk_widget_hide (GTK_MENU (menu)->toplevel);
      gtk_widget_hide (menu);

      /* inactive the toggle button */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);
    }

  return FALSE;
}



static void
launcher_plugin_arrow_drag_leave (GtkWidget      *widget,
                                  GdkDragContext *context,
                                  guint           drag_time,
                                  LauncherPlugin *plugin)
{
  panel_return_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin));

  if (plugin->menu_timeout_id != 0)
    {
      /* stop the popup timeout */
      g_source_remove (plugin->menu_timeout_id);

      /* inactive the toggle button */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->arrow), FALSE);
    }
  else
    {
      /* start a timeout to give the user some time to drag to the menu */
      g_timeout_add (MENU_POPUP_DELAY, launcher_plugin_arrow_drag_leave_timeout, plugin);
    }
}



static gboolean
launcher_plugin_item_query_tooltip (GtkWidget      *widget,
                                    gint            x,
                                    gint            y,
                                    gboolean        keyboard_mode,
                                    GtkTooltip     *tooltip,
                                    GarconMenuItem *item)
{
  gchar       *markup;
  const gchar *name, *comment;
  GdkPixbuf   *pixbuf;

  panel_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  /* require atleast an item name */
  name = garcon_menu_item_get_name (item);
  if (exo_str_is_empty (name))
    return FALSE;

  comment = garcon_menu_item_get_comment (item);
  if (!exo_str_is_empty (comment))
    {
      markup = g_markup_printf_escaped ("<b>%s</b>\n%s", name, comment);
      gtk_tooltip_set_markup (tooltip, markup);
      g_free (markup);
    }
  else
    {
      gtk_tooltip_set_text (tooltip, name);
    }

  /* we use the cached pixbuf for the button, because they are more
   * likely to occur and we don't want to poke the hard drive multiple
   * times for a simple pixbuf. for menu items this is not a big real,
   * so here we use the pixbuf directly */
  if (GTK_IS_MENU_ITEM (widget))
    {
      pixbuf = launcher_plugin_tooltip_pixbuf (gtk_widget_get_screen (widget),
                                               garcon_menu_item_get_icon_name (item));
      if (G_LIKELY (pixbuf != NULL))
        {
          gtk_tooltip_set_icon (tooltip, pixbuf);
          g_object_unref (G_OBJECT (pixbuf));
        }
     }

  return TRUE;
}



static gboolean
launcher_plugin_item_exec_on_screen (GarconMenuItem *item,
                                     guint32         event_time,
                                     GdkScreen      *screen,
                                     GSList         *uri_list)
{
  GError    *error = NULL;
  gchar    **argv;
  gboolean   succeed = FALSE;

  panel_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  panel_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  /* parse the execute command */
  if (launcher_plugin_exec_parse (item, uri_list, &argv, &error))
    {
      /* launch the command on the screen */
      succeed = xfce_spawn_on_screen (screen,
                                      garcon_menu_item_get_path (item),
                                      argv, NULL, G_SPAWN_SEARCH_PATH,
                                      garcon_menu_item_supports_startup_notification (item),
                                      event_time,
                                      garcon_menu_item_get_icon_name (item),
                                      &error);

      g_strfreev (argv);
    }

  if (G_UNLIKELY (!succeed))
    {
      /* show an error dialog */
      xfce_dialog_show_error (NULL, error,
                              _("Failed to execute command \"%s\"."),
                              garcon_menu_item_get_command (item));
      g_error_free (error);
    }

  return succeed;
}



static void
launcher_plugin_item_exec (GarconMenuItem *item,
                           guint32         event_time,
                           GdkScreen      *screen,
                           GSList         *uri_list)
{
  GSList      *li, fake;
  gboolean     proceed = TRUE;
  const gchar *command;

  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));
  panel_return_if_fail (GDK_IS_SCREEN (screen));

  /* leave when there is nothing to execute */
  command = garcon_menu_item_get_command (item);
  if (exo_str_is_empty (command))
    return;

  if (G_UNLIKELY (uri_list != NULL
      && strstr (command, "%F") == NULL
      && strstr (command, "%U") == NULL))
    {
      fake.next = NULL;

      /* run an instance for each file, break on the first error */
      for (li = uri_list; li != NULL && proceed; li = li->next)
        {
          fake.data = li->data;
          proceed = launcher_plugin_item_exec_on_screen (item, event_time, screen, &fake);
        }
    }
  else
    {
      launcher_plugin_item_exec_on_screen (item, event_time, screen, uri_list);
    }
}



static void
launcher_plugin_item_exec_from_clipboard (GarconMenuItem *item,
                                          guint32         event_time,
                                          GdkScreen      *screen)
{
  GtkClipboard     *clipboard;
  gchar            *text = NULL;
  GSList           *uri_list;
  GtkSelectionData  data;

  panel_return_if_fail (GARCON_IS_MENU_ITEM (item));
  panel_return_if_fail (GDK_IS_SCREEN (screen));

  /* get the primary clipboard text */
  clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
  if (G_LIKELY (clipboard))
    text = gtk_clipboard_wait_for_text (clipboard);

  /* try the secondary keayboard if the text is empty */
  if (!exo_str_is_empty (text))
    {
      /* get the secondary clipboard text */
      clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
      if (G_LIKELY (clipboard))
        text = gtk_clipboard_wait_for_text (clipboard);
    }

  if (!exo_str_is_empty (text))
    {
      /* create fake selection data */
      data.data = (guchar *) text;
      data.length = strlen (text);
      data.target = GDK_NONE;

      /* extract the uris from the selection data */
      uri_list = launcher_plugin_uri_list_extract (&data);

      /* launch with the uri list */
      launcher_plugin_item_exec (item, event_time,
                                 screen, uri_list);

      launcher_plugin_uri_list_free (uri_list);
    }

  g_free (text);
}



static void
launcher_plugin_exec_append_quoted (GString     *string,
                                    const gchar *unquoted)
{
  gchar *quoted;

  quoted = g_shell_quote (unquoted);
  g_string_append (string, quoted);
  g_free (quoted);
}



static gboolean
launcher_plugin_exec_parse (GarconMenuItem   *item,
                            GSList           *uri_list,
                            gchar          ***argv,
                            GError          **error)
{
  GString     *string;
  const gchar *p;
  gboolean     result;
  GSList      *li;
  gchar       *filename, *uri;
  const gchar *command, *tmp;

  panel_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  /* get the command */
  command = garcon_menu_item_get_command (item);
  panel_return_val_if_fail (!exo_str_is_empty (command), FALSE);

  /* allocate an empty string */
  string = g_string_sized_new (100);

  /* prepend terminal command if required */
  if (garcon_menu_item_requires_terminal (item))
    g_string_append (string, "exo-open --launch TerminalEmulator ");

  for (p = command; *p != '\0'; ++p)
    {
      if (G_UNLIKELY (p[0] == '%' && p[1] != '\0'))
        {
          switch (*++p)
            {
            case 'f':
            case 'F':
              for (li = uri_list; li != NULL; li = li->next)
                {
                  filename = g_filename_from_uri ((const gchar *) li->data,
                                                  NULL, NULL);
                  if (G_LIKELY (filename != NULL))
                    launcher_plugin_exec_append_quoted (string, filename);
                  g_free (filename);

                  if (*p == 'f')
                    break;
                  if (li->next != NULL)
                    g_string_append_c (string, ' ');
                }
              break;

            case 'u':
            case 'U':
              for (li = uri_list; li != NULL; li = li->next)
                {
                  launcher_plugin_exec_append_quoted (string, (const gchar *)
                                                      li->data);

                  if (*p == 'u')
                    break;
                  if (li->next != NULL)
                    g_string_append_c (string, ' ');
                }
              break;

            case 'i':
              tmp = garcon_menu_item_get_icon_name (item);
              if (!exo_str_is_empty (tmp))
                {
                  g_string_append (string, "--icon ");
                  launcher_plugin_exec_append_quoted (string, tmp);
                }
              break;

            case 'c':
              tmp = garcon_menu_item_get_name (item);
              if (!exo_str_is_empty (tmp))
                launcher_plugin_exec_append_quoted (string, tmp);
              break;

            case 'k':
              uri = garcon_menu_item_get_uri (item);
              if (!exo_str_is_empty (uri))
                launcher_plugin_exec_append_quoted (string, uri);
              g_free (uri);
              break;

            case '%':
              g_string_append_c (string, '%');
              break;
            }
        }
      else
        {
          g_string_append_c (string, *p);
        }
    }

  result = g_shell_parse_argv (string->str, NULL, argv, error);
  g_string_free (string, TRUE);

  return result;
}



static GSList *
launcher_plugin_uri_list_extract (GtkSelectionData *data)
{
  GSList  *list = NULL;
  gchar  **array;
  guint    i;
  gchar   *uri;

  /* leave if there is no data */
  if (data->length <= 0)
    return NULL;

  /* extract the files */
  if (data->target == gdk_atom_intern_static_string ("text/uri-list"))
    {
      /* extract the list of uris */
      array = g_uri_list_extract_uris ((gchar *) data->data);
      if (G_UNLIKELY (array == NULL))
        return NULL;

      /* create the list of uris */
      for (i = 0; array[i] != NULL; i++)
        {
          if (!exo_str_is_empty (array[i]))
            list = g_slist_prepend (list, array[i]);
          else
            g_free (array[i]);
        }

      g_free (array);
    }
  else
    {
      /* split the data on new lines */
      array = g_strsplit_set ((const gchar *) data->data, "\n\r", -1);
      if (G_UNLIKELY (array == NULL))
        return NULL;

      /* create the list of uris */
      for (i = 0; array[i] != NULL; i++)
        {
          /* skip empty strings */
          if (!!exo_str_is_empty (array[i]))
            continue;

          uri = NULL;

          if (g_path_is_absolute (array[i]))
            uri = g_filename_to_uri (array[i], NULL, NULL);
          else if (exo_str_looks_like_an_uri (array[i]))
            uri = g_strdup (array[i]);

          /* append the uri if we extracted one */
          if (G_LIKELY (uri != NULL))
            list = g_slist_prepend (list, uri);
        }

      g_strfreev (array);
    }

  return g_slist_reverse (list);
}



static void
launcher_plugin_uri_list_free (GSList *uri_list)
{
  if (uri_list != NULL)
    {
      g_slist_foreach (uri_list, (GFunc) g_free, NULL);
      g_slist_free (uri_list);
    }
}



GSList *
launcher_plugin_get_items (LauncherPlugin *plugin)
{
  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), NULL);

  /* set extra reference and return a copy of the list */
  g_slist_foreach (plugin->items, (GFunc) g_object_ref, NULL);
  return g_slist_copy (plugin->items);
}



gchar *
launcher_plugin_unique_filename (LauncherPlugin *plugin)
{
  gchar        *filename, *path;
  static guint  counter = 0;
  GTimeVal      timeval;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), NULL);

  g_get_current_time (&timeval);
  filename = g_strdup_printf (RELATIVE_CONFIG_PATH G_DIR_SEPARATOR_S "%ld%d.desktop",
                              xfce_panel_plugin_get_name (XFCE_PANEL_PLUGIN (plugin)),
                              xfce_panel_plugin_get_unique_id (XFCE_PANEL_PLUGIN (plugin)),
                              timeval.tv_sec, ++counter);
  path = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, filename, TRUE);
  g_free (filename);

  return path;

}



static void
launcher_plugin_garcon_menu_pool_add (GarconMenu *menu,
                                      GHashTable *pool)
{
  GList          *li, *items;
  GList          *menus;
  GarconMenuItem *item;
  const gchar    *desktop_id;

  panel_return_if_fail (GARCON_IS_MENU (menu));

  items = garcon_menu_get_items (menu);
  for (li = items; li != NULL; li = li->next)
    {
      item = GARCON_MENU_ITEM (li->data);
      panel_assert (GARCON_IS_MENU_ITEM (item));

      /* skip invisible items */
      if (!garcon_menu_element_get_visible (GARCON_MENU_ELEMENT (item)))
        continue;

      /* skip dupplicates */
      desktop_id = garcon_menu_item_get_desktop_id (item);
      if (g_hash_table_lookup (pool, desktop_id) != NULL)
        continue;

      /* insert the item */
      g_hash_table_insert (pool, g_strdup (desktop_id),
                           g_object_ref (G_OBJECT (item)));
    }
  g_list_free (items);

  menus = garcon_menu_get_menus (menu);
  for (li = menus; li != NULL; li = li->next)
    launcher_plugin_garcon_menu_pool_add (li->data, pool);
  g_list_free (menus);
}



GHashTable *
launcher_plugin_garcon_menu_pool (void)
{
  GHashTable *pool;
  GarconMenu *menu;
  GError     *error = NULL;

  /* always return a hash table, even if it's empty */
  pool = g_hash_table_new_full (g_str_hash, g_str_equal,
                                (GDestroyNotify) g_free,
                                (GDestroyNotify) g_object_unref);

  menu = garcon_menu_new_applications ();
  if (G_LIKELY (menu != NULL))
    {
      if (garcon_menu_load (menu, NULL, &error))
        {
          launcher_plugin_garcon_menu_pool_add (menu, pool);
        }
      else
        {
          g_warning ("Failed to load the applications menu: %s.", error->message);
          g_error_free (error);
        }

      g_object_unref (G_OBJECT (menu));
    }
  else
    {
      g_warning ("Failed to create the applications menu");
    }

  return pool;
}



gboolean
launcher_plugin_item_is_editable (LauncherPlugin *plugin,
                                  GarconMenuItem *item,
                                  gboolean       *can_delete)
{
  GFile     *item_file;
  gboolean   editable = FALSE;
  GFileInfo *file_info;

  panel_return_val_if_fail (XFCE_IS_LAUNCHER_PLUGIN (plugin), FALSE);
  panel_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  item_file = garcon_menu_item_get_file (item);
  if (!g_file_has_prefix (item_file, plugin->config_directory))
    goto out;

  file_info = g_file_query_info (item_file,
                                 G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE ","
                                 G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE,
                                 G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (G_LIKELY (file_info != NULL))
    {
      editable = g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

      if (editable && can_delete != NULL)
        *can_delete = g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);

      g_object_unref (G_OBJECT (file_info));
    }

out:
  g_object_unref (G_OBJECT (item_file));

  return editable;
}
