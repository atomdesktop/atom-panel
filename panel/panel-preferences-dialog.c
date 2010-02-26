/* $Id$ */
/*
 * Copyright (C) 2008 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <exo/exo.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>

#include <panel/panel-window.h>
#include <panel/panel-glue.h>
#include <panel/panel-application.h>
#include <panel/panel-module.h>
#include <panel/panel-itembar.h>
#include <panel/panel-preferences-dialog.h>
#include <panel/panel-preferences-dialog-glade.h>

#define BORDER               (6)
#define PREFERENCES_HELP_URL "http://www.xfce.org"



static void panel_preferences_dialog_class_init (PanelPreferencesDialogClass *klass);
static void panel_preferences_dialog_init (PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_finalize (GObject *object);
static void panel_preferences_dialog_help (GtkWidget *button);


static void panel_preferences_dialog_bindings_unbind (PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_bindings_add (PanelPreferencesDialog *dialog, const gchar *property1, const gchar *property2);
static void panel_preferences_dialog_bindings_update (PanelPreferencesDialog *dialog);


static void panel_preferences_dialog_panel_combobox_changed (GtkComboBox *combobox, PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_panel_combobox_rebuild (PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_panel_add (GtkWidget *widget, PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_panel_remove (GtkWidget *widget, PanelPreferencesDialog *dialog);


static XfcePanelPluginProvider *panel_preferences_dialog_item_get_selected (PanelPreferencesDialog *dialog, GtkTreeIter *return_iter);
static void panel_preferences_dialog_item_store_rebuild (PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_item_move (GtkWidget *button, PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_item_remove (GtkWidget *button, PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_item_add (GtkWidget *button, PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_item_properties (GtkWidget *button, PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_item_about (GtkWidget *button, PanelPreferencesDialog *dialog);
static void panel_preferences_dialog_item_selection_changed (GtkTreeSelection *selection, PanelPreferencesDialog *dialog);



enum
{
  COLUMN_ICON_NAME,
  COLUMN_DISPLAY_NAME,
  COLUMN_PROVIDER,
  N_COLUMNS
};

struct _PanelPreferencesDialogClass
{
  GtkBuilderClass __parent__;
};

struct _PanelPreferencesDialog
{
  GtkBuilder  __parent__;

  /* application we're handling */
  PanelApplication *application;

  /* currently selected window in the selector */
  PanelWindow      *active;

  /* list of exo bindings */
  GSList           *bindings;

  /* store for the items list */
  GtkListStore     *store;
};



G_DEFINE_TYPE (PanelPreferencesDialog, panel_preferences_dialog, GTK_TYPE_BUILDER);



static void
panel_preferences_dialog_class_init (PanelPreferencesDialogClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = panel_preferences_dialog_finalize;
}

#define connect_signal(name,detail_signal,c_handler) \
  object = gtk_builder_get_object (GTK_BUILDER (dialog), name); \
  panel_return_if_fail (G_IS_OBJECT (object)); \
  g_signal_connect (G_OBJECT (object), detail_signal, G_CALLBACK (c_handler), dialog);

static void
panel_preferences_dialog_init (PanelPreferencesDialog *dialog)
{
  GObject           *window;
  GObject           *object;
  GObject           *treeview;
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeSelection  *selection;

  /* init */
  dialog->bindings = NULL;
  dialog->application = panel_application_get ();

  /* block all autohides */
  panel_application_windows_autohide (dialog->application, TRUE);

  /* load the builder data into the object */
  gtk_builder_add_from_string (GTK_BUILDER (dialog), panel_preferences_dialog_glade,
                               panel_preferences_dialog_glade_length, NULL);

  /* get the dialog */
  window = gtk_builder_get_object (GTK_BUILDER (dialog), "dialog");
  panel_application_take_dialog (dialog->application, GTK_WINDOW (window));
  g_object_weak_ref (G_OBJECT (window), (GWeakNotify) g_object_unref, dialog);

  /* close button */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "button-close");
  g_signal_connect_swapped (G_OBJECT (object), "clicked", G_CALLBACK (gtk_widget_destroy), window);

  /* help button */
  connect_signal ("button-help", "clicked", panel_preferences_dialog_help);

  /* panel selector buttons and combobox */
  connect_signal ("panel-add", "clicked", panel_preferences_dialog_panel_add);
  connect_signal ("panel-remove", "clicked", panel_preferences_dialog_panel_remove);
  connect_signal ("panel-combobox", "changed", panel_preferences_dialog_panel_combobox_changed);

  /* items treeview and buttons */
  connect_signal ("item-up", "clicked", panel_preferences_dialog_item_move);
  connect_signal ("item-down", "clicked", panel_preferences_dialog_item_move);
  connect_signal ("item-remove", "clicked", panel_preferences_dialog_item_remove);
  connect_signal ("item-add", "clicked", panel_preferences_dialog_item_add);
  connect_signal ("item-properties", "clicked", panel_preferences_dialog_item_properties);
  connect_signal ("item-about", "clicked", panel_preferences_dialog_item_about);

  /* create store for panel items */
  dialog->store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT);

  /* build tree for panel items */
  treeview = gtk_builder_get_object (GTK_BUILDER (dialog), "item-treeview");
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (dialog->store));

  /* setup tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (panel_preferences_dialog_item_selection_changed), dialog);

  /* icon renderer */
  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer, "icon-name", COLUMN_ICON_NAME, NULL);
  g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_BUTTON, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  /* text renderer */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer, "text", COLUMN_DISPLAY_NAME, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  /* TODO remove when implemented by glade */
  GtkCellRenderer *cell1 = gtk_cell_renderer_text_new ();
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-combobox");
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), cell1, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), cell1, "text", 0, NULL);

  GtkCellRenderer *cell2 = gtk_cell_renderer_text_new ();
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "horizontal");
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), cell2, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), cell2, "text", 0, NULL);

  /* rebuild the panel combobox */
  panel_preferences_dialog_panel_combobox_rebuild (dialog);

  /* show the dialog */
  gtk_widget_show (GTK_WIDGET (window));
}



static void
panel_preferences_dialog_finalize (GObject *object)
{
  PanelPreferencesDialog *dialog = PANEL_PREFERENCES_DIALOG (object);

  /* thaw all autohide blocks */
  panel_application_windows_autohide (dialog->application, FALSE);

  /* deselect all windows */
  panel_application_window_select (dialog->application, NULL);

  /* release the application */
  g_object_unref (G_OBJECT (dialog->application));

  /* release the store */
  g_object_unref (G_OBJECT (dialog->store));

  (*G_OBJECT_CLASS (panel_preferences_dialog_parent_class)->finalize) (object);
}



static void
panel_preferences_dialog_help (GtkWidget *button)
{
  GError    *error = NULL;
  GdkScreen *screen;

  /* get the dialog screen */
  screen = gtk_widget_get_screen (button);

  /* open the help url */
  if (exo_url_show_on_screen (PREFERENCES_HELP_URL, NULL, screen, &error) == FALSE)
    {
      /* show error and cleanup */
      g_warning ("Failed to open help: %s", error->message);
      g_error_free (error);
    }
}



static void
panel_preferences_dialog_bindings_unbind (PanelPreferencesDialog *dialog)
{
  GSList *li;

  if (dialog->bindings)
    {
      /* remove all bindings */
      for (li = dialog->bindings; li != NULL; li = li->next)
        exo_mutual_binding_unbind (li->data);

      /* cleanup */
      g_slist_free (dialog->bindings);
      dialog->bindings = NULL;
    }
}



static void
panel_preferences_dialog_bindings_add (PanelPreferencesDialog *dialog,
                                       const gchar            *property1,
                                       const gchar            *property2)
{
  ExoMutualBinding *binding;
  GObject          *object;

  /* get the object from the builder */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), property1);

  /* debug check */
  panel_return_if_fail (G_IS_OBJECT (object));

  /* create the binding and prepend to the list */
  binding = exo_mutual_binding_new (G_OBJECT (dialog->active), property1, object, property2);
  dialog->bindings = g_slist_prepend (dialog->bindings, binding);
}



static void
panel_preferences_dialog_bindings_update (PanelPreferencesDialog *dialog)
{
  /* remove all the active bindings */
  panel_preferences_dialog_bindings_unbind (dialog);

  /* hook up the bindings */
  panel_preferences_dialog_bindings_add (dialog, "horizontal", "active");
  panel_preferences_dialog_bindings_add (dialog, "locked", "active");
  panel_preferences_dialog_bindings_add (dialog, "autohide", "active");
  panel_preferences_dialog_bindings_add (dialog, "size", "value");
  panel_preferences_dialog_bindings_add (dialog, "length", "value");
  panel_preferences_dialog_bindings_add (dialog, "background-alpha", "value");
  panel_preferences_dialog_bindings_add (dialog, "enter-opacity", "value");
  panel_preferences_dialog_bindings_add (dialog, "leave-opacity", "value");
}



static void
panel_preferences_dialog_panel_combobox_changed (GtkComboBox            *combobox,
                                                 PanelPreferencesDialog *dialog)
{
  gint nth;

  panel_return_if_fail (GTK_IS_COMBO_BOX (combobox));
  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  /* set the selected window */
  nth = gtk_combo_box_get_active (combobox);
  dialog->active = panel_application_get_window (dialog->application, nth);

  /* rebind the dialog bindings */
  panel_preferences_dialog_bindings_update (dialog);

  /* update selection state */
  panel_application_window_select (dialog->application, dialog->active);

  /* update the items treeview */
  panel_preferences_dialog_item_store_rebuild (dialog);
}



static void
panel_preferences_dialog_panel_combobox_rebuild (PanelPreferencesDialog *dialog)
{
  GObject *store, *combo;
  gint     n, n_items;
  gchar   *name;

  /* get the combo box and model */
  store = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-liststore");
  combo = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-combobox");

  panel_return_if_fail (GTK_IS_LIST_STORE (store));
  panel_return_if_fail (GTK_IS_COMBO_BOX (combo));

  /* block signal */
  g_signal_handlers_block_by_func (combo, panel_preferences_dialog_panel_combobox_changed, dialog);

  /* empty the combo box */
  gtk_list_store_clear (GTK_LIST_STORE (store));

  /* add new names */
  n_items = panel_application_get_n_windows (dialog->application);
  for (n = 0; n < n_items; n++)
    {
      name = g_strdup_printf ("Panel %d", n + 1);
      gtk_list_store_insert_with_values (GTK_LIST_STORE (store), NULL, n, 0, name, -1);
      g_free (name);
    }

  /* unblock signal */
  g_signal_handlers_unblock_by_func (combo, panel_preferences_dialog_panel_combobox_changed, dialog);
}



static void
panel_preferences_dialog_panel_add (GtkWidget              *widget,
                                    PanelPreferencesDialog *dialog)
{
  gint         active;
  PanelWindow *window;
  GObject     *object;

  /* create new window */
  window = panel_application_new_window (dialog->application, gtk_widget_get_screen (widget));

  /* block autohide */
  panel_window_freeze_autohide (window);

  /* rebuild the selector */
  panel_preferences_dialog_panel_combobox_rebuild (dialog);

  /* set the sensitivity of the remove button */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "remove-panel");
  gtk_widget_set_sensitive (GTK_WIDGET (object), panel_application_get_n_windows (dialog->application) > 1);

  /* select new panel (new window is appended) */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-combobox");
  active = panel_application_get_n_windows (dialog->application) - 1;
  gtk_combo_box_set_active (GTK_COMBO_BOX (object), active);

  /* show window */
  gtk_widget_show (GTK_WIDGET (window));
}



static void
panel_preferences_dialog_panel_remove (GtkWidget              *widget,
                                       PanelPreferencesDialog *dialog)
{
  gint     nth;
  GObject *combo;

  /* get active panel */
  nth = panel_application_get_window_index (dialog->application, dialog->active);

  /* destroy the window */
  if (xfce_dialog_confirm (dialog, GTK_STOCK_REMOVE, NULL,
                           "Are you sure you want to remove panel %d?", nth + 1))
    {
      /* release the bindings */
      panel_preferences_dialog_bindings_unbind (dialog);

      /* destroy the panel */
      gtk_widget_destroy (GTK_WIDGET (dialog->active));

      /* set the sensitivity of the remove button */
      gtk_widget_set_sensitive (widget, panel_application_get_n_windows (dialog->application) > 1);

      /* rebuild the selector */
      panel_preferences_dialog_panel_combobox_rebuild (dialog);

      /* select new active window */
      combo = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-combobox");
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), MAX (0, nth - 1));
    }
}



static XfcePanelPluginProvider *
panel_preferences_dialog_item_get_selected (PanelPreferencesDialog *dialog,
                                            GtkTreeIter            *return_iter)
{
  GObject                 *treeview;
  XfcePanelPluginProvider *provider = NULL;
  GtkTreeModel            *model;
  GtkTreeIter              iter;
  GtkTreeSelection        *selection;

  panel_return_val_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog), NULL);

  /* get the treeview selection */
  treeview = gtk_builder_get_object (GTK_BUILDER (dialog), "item-treeview");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  /* get the selection item */
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      /* get the selected provider */
      gtk_tree_model_get (model, &iter, COLUMN_PROVIDER, &provider, -1);
      panel_return_val_if_fail (XFCE_IS_PANEL_PLUGIN_PROVIDER (provider), NULL);

      if (return_iter)
        *return_iter = iter;
    }

  return provider;
}



static void
panel_preferences_dialog_item_store_rebuild (PanelPreferencesDialog *dialog)
{
  GtkWidget   *itembar;
  GList       *items, *li;
  guint        i;
  PanelModule *module;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));
  panel_return_if_fail (GTK_IS_LIST_STORE (dialog->store));

  /* clear the store */
  gtk_list_store_clear (dialog->store);

  /* get the panel items */
  itembar = gtk_bin_get_child (GTK_BIN (dialog->active));
  items = gtk_container_get_children (GTK_CONTAINER (itembar));

  /* add items to the store */
  for (li = items, i = 0; li != NULL; li = li->next, i++)
    {
      /* get the panel module from the plugin */
      module = panel_module_get_from_plugin_provider (li->data);

      /* insert in the store */
      gtk_list_store_insert_with_values (dialog->store, NULL, i,
                                         COLUMN_ICON_NAME,
                                         panel_module_get_icon_name (module),
                                         COLUMN_DISPLAY_NAME,
                                         panel_module_get_name (module),
                                         COLUMN_PROVIDER, li->data, -1);
    }

  /* cleanup */
  g_list_free (items);
}



static void
panel_preferences_dialog_item_move (GtkWidget              *button,
                                    PanelPreferencesDialog *dialog)
{
  GObject                 *treeview, *object;
  GtkTreeSelection        *selection;
  GtkTreeIter              iter_a, iter_b;
  XfcePanelPluginProvider *provider;
  GtkWidget               *itembar;
  gint                     position;
  gint                     direction;
  GtkTreePath             *path;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  /* direction */
  object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-up");
  direction = G_OBJECT (button) == object ? -1 : 1;

  /* get the selected item */
  provider = panel_preferences_dialog_item_get_selected (dialog, &iter_a);

  /* get the selection item */
  if (G_LIKELY (provider))
    {
      /* get the itembar */
      itembar = gtk_bin_get_child (GTK_BIN (dialog->active));

      /* get the provider position on the panel */
      position = panel_itembar_get_child_index (PANEL_ITEMBAR (itembar),
                                                GTK_WIDGET (provider));

      if (G_LIKELY (position != -1))
        {
          /* move the item on the panel */
          panel_itembar_reorder_child (PANEL_ITEMBAR (itembar),
                                       GTK_WIDGET (provider),
                                       position + direction);


          /* most the item up or down in the list */
          if (direction == 1)
            {
              /* init the 2nd iter */
              iter_b = iter_a;

              /* swap the items in the list */
              if (gtk_tree_model_iter_next (GTK_TREE_MODEL (dialog->store), &iter_b))
                gtk_list_store_swap (dialog->store, &iter_a, &iter_b);
            }
          else
            {
              /* determine the tree path to the iter */
              path = gtk_tree_model_get_path (GTK_TREE_MODEL (dialog->store), &iter_a);
              if (gtk_tree_path_prev (path))
                {
                  /* determine the iter for the previous item */
                  gtk_tree_model_get_iter (GTK_TREE_MODEL (dialog->store), &iter_b, path);

                  /* swap the items in the list */
                  gtk_list_store_swap (dialog->store, &iter_a, &iter_b);
                }

              /* release the path */
              gtk_tree_path_free (path);
            }

          /* fake update the selection */
          treeview = gtk_builder_get_object (GTK_BUILDER (dialog), "item-treeview");
          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
          panel_preferences_dialog_item_selection_changed (selection, dialog);
        }
    }
}



static void
panel_preferences_dialog_item_remove (GtkWidget              *button,
                                      PanelPreferencesDialog *dialog)
{
  XfcePanelPluginProvider *provider;
  GtkTreeIter              iter;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  /* get the selected item in the treeview */
  provider = panel_preferences_dialog_item_get_selected (dialog, &iter);

  /* TODO, question dialog */

  /* send signal */
  xfce_panel_plugin_provider_send_signal (provider, PROVIDER_SIGNAL_REMOVE_PLUGIN);

  /* remove from treeview */
  gtk_list_store_remove (dialog->store, &iter);
}



static void
panel_preferences_dialog_item_add (GtkWidget              *button,
                                   PanelPreferencesDialog *dialog)
{
  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  /* TODO */
}



static void
panel_preferences_dialog_item_properties (GtkWidget              *button,
                                          PanelPreferencesDialog *dialog)
{
  XfcePanelPluginProvider *provider;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  /* get the selected item in the treeview */
  provider = panel_preferences_dialog_item_get_selected (dialog, NULL);

  /* emit configure-plugin signal */
  xfce_panel_plugin_provider_show_configure (provider);
}



static void
panel_preferences_dialog_item_about (GtkWidget              *button,
                                     PanelPreferencesDialog *dialog)
{
  XfcePanelPluginProvider *provider;

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  /* get the selected item in the treeview */
  provider = panel_preferences_dialog_item_get_selected (dialog, NULL);

  /* emit about signal */
  xfce_panel_plugin_provider_show_about (provider);
}



static void
panel_preferences_dialog_item_selection_changed (GtkTreeSelection       *selection,
                                                 PanelPreferencesDialog *dialog)
{
  XfcePanelPluginProvider *provider;
  GtkWidget               *itembar;
  gint                     position;
  gint                     items;
  gboolean                 active;
  GObject                 *object;
  guint                    i;
  const gchar             *button_names[] = { "item-remove", "item-up",
                                              "item-down", "item-about",
                                              "item-properties" };

  panel_return_if_fail (PANEL_IS_PREFERENCES_DIALOG (dialog));

  /* get the selected item */
  provider = panel_preferences_dialog_item_get_selected (dialog, NULL);

  if (provider)
    {
      /* get the itembar */
      itembar = gtk_bin_get_child (GTK_BIN (dialog->active));

      /* get the current position and the items on the bar */
      position = panel_itembar_get_child_index (PANEL_ITEMBAR (itembar), GTK_WIDGET (provider));
      items = panel_itembar_get_n_children (PANEL_ITEMBAR (itembar)) - 1;

      /* update sensitivity of buttons */
      object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-up");
      gtk_widget_set_sensitive (GTK_WIDGET (object), !!(position > 0 && position <= items));

      object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-down");
      gtk_widget_set_sensitive (GTK_WIDGET (object), !!(position >= 0 && position < items));

      object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-remove");
      gtk_widget_set_sensitive (GTK_WIDGET (object), TRUE);

      object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-properties");
      active = xfce_panel_plugin_provider_get_show_configure (provider);
      gtk_widget_set_sensitive (GTK_WIDGET (object), active);

      object = gtk_builder_get_object (GTK_BUILDER (dialog), "item-about");
      active = xfce_panel_plugin_provider_get_show_about (provider);
      gtk_widget_set_sensitive (GTK_WIDGET (object), active);
    }
  else
    {
      /* make all items insensitive, except the add button */
      for (i = 0; i < G_N_ELEMENTS (button_names); i++)
        {
          object = gtk_builder_get_object (GTK_BUILDER (dialog), button_names[i]);
          gtk_widget_set_sensitive (GTK_WIDGET (object), FALSE);
        }
    }
}



void
panel_preferences_dialog_show (PanelWindow *active)
{
  static PanelPreferencesDialog *dialog = NULL;
  gint                           idx = 0;
  GObject                       *window, *combo;

  panel_return_if_fail (active == NULL || PANEL_IS_WINDOW (active));

  if (G_LIKELY (dialog == NULL))
    {
      /* create new dialog singleton */
      dialog = g_object_new (PANEL_TYPE_PREFERENCES_DIALOG, NULL);
      g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer) &dialog);
    }

  /* get the active window index */
  if (G_LIKELY (active))
    idx = panel_application_get_window_index (dialog->application, active);
  else
    active = panel_application_get_window (dialog->application, idx);

  /* show the dialog on the same screen as the panel */
  window = gtk_builder_get_object (GTK_BUILDER (dialog), "dialog");
  gtk_window_set_screen (GTK_WINDOW (window), gtk_widget_get_screen (GTK_WIDGET (active)));
  gtk_window_present (GTK_WINDOW (window));

  /* select the active window in the dialog */
  combo = gtk_builder_get_object (GTK_BUILDER (dialog), "panel-combobox");
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), idx);
}
