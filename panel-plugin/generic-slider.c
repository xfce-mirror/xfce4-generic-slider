#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4ui/libxfce4ui.h>
#define TIMEOUT 1000
#define WIDTH 8

typedef struct generic_slider {
	GtkWidget *slider;
	GtkWidget *label;
	GdkRGBA color;
	GtkCssProvider *css_provider;
	char *description;
	char *adjust_command;
	char *sync_command;
	/* Whether we show label, slider or both */
	int mode;
	/* Corresponds to the function that runs every second */
	int timeout_id;
	int description_denominator;
	int adjust_denominator;
	int sync_denominator;
	/* These two are between 0 and 1 */
	double value;
	double delta;
	int active;
	int ignoring_color;
} Generic_Slider;

char *parse_command(char *primitive, int value, int delta) {
	gchar *command;
	int numds = 0;
	int numvs = 0;
	int i;
	
	if (!strcmp(primitive, "")) {
		/* Functions that free this string later need something to free */
		command = g_strdup("");
		return command;
	}
	
	for (i = 0; i < strlen(primitive); i++) {
		if (primitive[i-1] == '%') {
			if (primitive[i] == 'd') {
				numds++;
			} else if (primitive[i] == 'v') {
				numvs++;
			}
		}
	}
		
	command = g_strdup(primitive);
	
	for (i = 0; i < numds; i++) {
		gchar **frags = g_strsplit(command, "%d", 2);
		gchar *delta_string = g_strdup_printf("%d", delta);
		
		g_free(command);
		command = g_strconcat(frags[0], delta_string, frags[1], NULL);
		
		g_strfreev(frags);
		g_free(delta_string);
	}
	for (i = 0; i < numvs; i++) {
		gchar **frags = g_strsplit(command, "%v", 2);
		gchar *value_string = g_strdup_printf("%d", value);
		
		g_free(command);
		command = g_strconcat(frags[0], value_string, frags[1], NULL);
		
		g_strfreev(frags);
		g_free(value_string);
	}
	return command;
}

static gint timer_cb(Generic_Slider *generic_slider) {
	static int repetition = 0;
	char *label_text;
	
	if ((generic_slider -> active) && (strcmp(generic_slider -> sync_command, ""))) {
		generic_slider -> active = 0;
		FILE *stream = popen(parse_command(generic_slider -> sync_command, (generic_slider -> sync_denominator) * (generic_slider -> value), (generic_slider -> sync_denominator) * (generic_slider -> delta)), "r");
		int new_value = 0;
		int i;
		int c;
		
		repetition++;
		
		/* Gets the output of the command knowing that numbers are 48 less than their ASCII equivalents */
		for (i = 0; i < 3; i++) {
			c = fgetc(stream);
						
			if ((c >= 48) && (c <= 57)) {
				new_value = (10 * new_value) + (c - 48);
			} else {
				break;
			}
		}
		
		if (new_value <= (generic_slider -> sync_denominator)) {
			generic_slider -> value = ((double) new_value) / ((double) generic_slider -> sync_denominator);
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(generic_slider -> slider), generic_slider -> value);
			
			label_text = parse_command(generic_slider -> description, (generic_slider -> description_denominator) * (generic_slider -> value), (generic_slider -> description_denominator) * (generic_slider -> delta));
			gtk_label_set_text(GTK_LABEL(generic_slider -> label), label_text);
			
			if (repetition == 3) {
				repetition = 0;
				gtk_widget_set_tooltip_text(generic_slider -> slider, label_text);
				gtk_widget_set_tooltip_text(generic_slider -> label, label_text);
			}
		}
		
		free(label_text);
		pclose(stream);
		generic_slider -> active = 1;
	}
	
	return TRUE;
}

static void execute_command(char *command) {
	pid_t pid;
	char **arglist;
	int first_space = 0;
	int max_arg_length = 0;
	int num_args = 2;
	int i, j, k;
	
	if (!strcmp(command, "")) return;
	
	/* Allocates the arglist appropriately */
	
	j = 0;
	for (i = 0; i < strlen(command); i++) {
		if (command[i] == ' ') {
			if (!first_space) {
				first_space = i;
			}
			if (command[i - 1] != ' ') {
				num_args++;
				max_arg_length = MAX(max_arg_length, j);
				j = 0;
			}
		} else {
			j++;
		}
	}
	arglist = malloc(num_args * sizeof(char *));
	
	for (i = 0; i < num_args; i++) {
		arglist[i] = malloc((max_arg_length + 1) * sizeof(char));
	}

	/* Puts the arguments in the arglist */
	
	j = k = 0;
	for (i = 0; i < strlen(command); i++) {
		if (command[i] == ' ') {
			if (command[i - 1] != ' ') {
				arglist[j][k] = '\0';
				k = 0;
				j++;
			}
		} else {
			arglist[j][k] = command[i];
			k++;
		}
	}
	
	arglist[j][k] = '\0';
	arglist[num_args - 1] = NULL;
	
	/* Forks */

	pid = fork();
	if (pid == 0) {
		wait(NULL);
	} else {
		execvp(arglist[0], arglist);
		perror("execvp");
	}

}

static gint scroll_slider_cb(GtkWidget *widget, GdkEventScroll *event, GList *stupid_hack) {
	char *label_text;
	GdkScrollDirection dir;
	Generic_Slider *generic_slider = stupid_hack -> data;
	XfcePanelPlugin *plugin = stupid_hack -> next -> data;
	
	dir = event -> direction;
	/* Scrolling by 10% each time */
	if (xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_VERTICAL) {
		if (dir == GDK_SCROLL_LEFT) {
			if (generic_slider -> value < 0.9) {
				generic_slider -> value += 0.1;
				generic_slider -> delta = 0.1;
			} else {
				generic_slider -> delta = 1.0 - generic_slider -> value;
				generic_slider -> value = 1.0;
			}
		} else if (dir == GDK_SCROLL_RIGHT) {
			if (generic_slider -> value > 0.1) {
				generic_slider -> value -= 0.1;
				generic_slider -> delta = -0.1;
			} else {
				generic_slider -> delta = -(generic_slider -> value);
				generic_slider -> value = 0.0;
			}
		}
	} else {
		if (dir == GDK_SCROLL_UP) {
			if (generic_slider -> value < 0.9) {
				generic_slider -> value += 0.1;
				generic_slider -> delta = 0.1;
			} else {
				generic_slider -> delta = 1.0 - generic_slider -> value;
				generic_slider -> value = 1.0;
			}
		} else if (dir == GDK_SCROLL_DOWN) {
			if (generic_slider -> value > 0.1) {
				generic_slider -> value -= 0.1;
				generic_slider -> delta = -0.1;
			} else {
				generic_slider -> delta = -(generic_slider -> value);
				generic_slider -> value = 0.0;
			}
		}
	}
	
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(generic_slider -> slider), generic_slider -> value);	
	execute_command(parse_command(generic_slider -> adjust_command, (generic_slider -> adjust_denominator) * (generic_slider -> value), (generic_slider -> adjust_denominator) * (generic_slider -> delta)));
	
	label_text = parse_command(generic_slider -> description, (generic_slider -> description_denominator) * (generic_slider -> value), (generic_slider -> description_denominator) * (generic_slider -> delta));
	
	gtk_label_set_text(GTK_LABEL(generic_slider -> label), label_text);
	gtk_widget_set_tooltip_text(generic_slider -> slider, label_text);
	gtk_widget_set_tooltip_text(generic_slider -> label, label_text);
	free(label_text);
	return TRUE;
}

static gint adjust_slider_cb(GtkWidget *widget, GdkEventButton *event, GList *stupid_hack) {
	char *label_text;
	gdouble value_to_try;
	Generic_Slider *generic_slider = stupid_hack -> data;
	XfcePanelPlugin *plugin = stupid_hack -> next -> data;
	GtkAllocation allocation;
	
	if (event -> button == 3) {
		return FALSE;
	}
	
	gtk_widget_get_allocation(widget, &allocation);
	
	if (xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_VERTICAL) {
		value_to_try = (event -> x) / allocation.width;
	} else {
		value_to_try = 1.0 - ((event -> y) / allocation.height);
	}
	
	if ((value_to_try >= 0.0) && (value_to_try <= 1.0)) {
		generic_slider -> delta = value_to_try - generic_slider -> value;
		generic_slider -> value = value_to_try;
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(generic_slider -> slider), value_to_try);	
		execute_command(parse_command(generic_slider -> adjust_command, (generic_slider -> adjust_denominator) * (generic_slider -> value), (generic_slider -> adjust_denominator) * (generic_slider -> delta)));
		
		label_text = parse_command(generic_slider -> description, (generic_slider -> description_denominator) * (generic_slider -> value), (generic_slider -> description_denominator) * (generic_slider -> delta));
		
		gtk_label_set_text(GTK_LABEL(generic_slider -> label), label_text);
		gtk_widget_set_tooltip_text(generic_slider -> slider, label_text);
		gtk_widget_set_tooltip_text(generic_slider -> label, label_text);
		free(label_text);
	}
	
	return FALSE;
}

static void generic_slider_write_rc_file(XfcePanelPlugin *plugin, Generic_Slider *generic_slider) {
	XfceRc *rc;
	gchar *file;
	gchar *color_string;
	
	/* Gdk colors sure have a lot of digits */
	color_string = gdk_rgba_to_string(&(generic_slider -> color));
	file = xfce_panel_plugin_save_location(plugin, TRUE);
	
	if (!file) {
		return;
	}
	
	rc = xfce_rc_simple_open(file, FALSE);
	g_free(file);
	
	if (!rc) {
		return;
	}
	
	xfce_rc_write_entry(rc, "adjust_command", generic_slider -> adjust_command);
	xfce_rc_write_entry(rc, "sync_command", generic_slider -> sync_command);
	xfce_rc_write_entry(rc, "description", generic_slider -> description);
	xfce_rc_write_entry(rc, "adjust_denominator", g_strdup_printf("%d", generic_slider -> adjust_denominator));
	xfce_rc_write_entry(rc, "sync_denominator", g_strdup_printf("%d", generic_slider -> sync_denominator));
	xfce_rc_write_entry(rc, "description_denominator", g_strdup_printf("%d", generic_slider -> description_denominator));
	xfce_rc_write_entry(rc, "mode",  g_strdup_printf("%d", generic_slider -> mode));
	xfce_rc_write_entry(rc, "ignoring_color",  g_strdup_printf("%d", generic_slider -> ignoring_color));
	xfce_rc_write_entry(rc, "color", color_string);
	xfce_rc_close(rc);
	
	g_free(color_string);
}

static void generic_slider_read_rc_file(XfcePanelPlugin *plugin, Generic_Slider *generic_slider) {
	XfceRc *rc;
	gchar *file;
	gchar *color_string;
	GdkRGBA color_temp;
	const gchar *tmp;
	
	gdk_rgba_parse(&color_temp, "blue");
	color_string = gdk_rgba_to_string(&color_temp);
	file = xfce_panel_plugin_lookup_rc_file(plugin);
	
	if (file != NULL) {
		rc = xfce_rc_simple_open(file, TRUE);
		g_free(file);
		
		if (rc != NULL) {
			tmp = xfce_rc_read_entry(rc, "adjust_command", "");
			if (tmp != NULL) {
				generic_slider -> adjust_command = g_strdup(tmp);
			}
			tmp = xfce_rc_read_entry(rc, "sync_command", "");
			if (tmp != NULL) {
				generic_slider -> sync_command = g_strdup(tmp);
			}
			tmp = xfce_rc_read_entry(rc, "description", "");
			if (tmp != NULL) {
				generic_slider -> description = g_strdup(tmp);
			}
			tmp = xfce_rc_read_entry(rc, "adjust_denominator", "100");
			if (tmp != NULL) {
				generic_slider -> adjust_denominator = (int) g_strtod(tmp, NULL);
			}
			tmp = xfce_rc_read_entry(rc, "sync_denominator", "100");
			if (tmp != NULL) {
				generic_slider -> sync_denominator = (int) g_strtod(tmp, NULL);
			}
			tmp = xfce_rc_read_entry(rc, "description_denominator", "100");
			if (tmp != NULL) {
				generic_slider -> description_denominator = (int) g_strtod(tmp, NULL);
			}
			tmp = xfce_rc_read_entry(rc, "mode", "0");
			if (tmp != NULL) {
				generic_slider -> mode = (int) g_strtod(tmp , NULL);
			}
			tmp = xfce_rc_read_entry(rc, "ignoring_color", "1");
			if (tmp != NULL) {
				generic_slider -> ignoring_color = (int) g_strtod(tmp , NULL);
			}
			tmp = xfce_rc_read_entry(rc, "color", color_string);
			if (tmp != NULL) {
				gdk_rgba_parse(&(generic_slider -> color), tmp);
			}
			xfce_rc_close(rc);
		}
	}
	
	g_free(color_string);
	generic_slider -> timeout_id = g_timeout_add(TIMEOUT, (GSourceFunc)timer_cb, generic_slider);
	generic_slider -> active = 1;
}

static void generic_slider_properties_dialog_response(GtkWidget *dialog, gint response, GList *stupid_hack) {
	Generic_Slider *generic_slider = stupid_hack -> data;
	XfcePanelPlugin *plugin = stupid_hack -> next -> data;

	xfce_panel_plugin_unblock_menu(plugin);
	gtk_widget_destroy(dialog);

	generic_slider_write_rc_file(plugin, generic_slider);
	generic_slider -> active = 1;
}

static void generic_slider_update_color(GtkColorChooser *picker, Generic_Slider *generic_slider) {
	GdkRGBA new_color;
	char *css;
	
	gtk_color_chooser_get_rgba(picker, &new_color);
	generic_slider -> color = new_color;
	css = g_strdup_printf("progressbar progress { background-color: %s; }", gdk_rgba_to_string(&new_color));
	gtk_css_provider_load_from_data(generic_slider -> css_provider, css, strlen(css), NULL);
	free(css);
}

static void generic_slider_update_default(GtkToggleButton *check, Generic_Slider *generic_slider) {
	GtkWidget *bbox;
	GtkWidget *picker;
	
	bbox = gtk_widget_get_ancestor(GTK_WIDGET(check), GTK_TYPE_BUTTON_BOX);
	picker = gtk_container_get_children(GTK_CONTAINER(bbox)) -> next -> data;
	
	if (gtk_toggle_button_get_active(check)) {
		gtk_widget_set_sensitive(picker, FALSE);
		generic_slider -> ignoring_color = 1;
		gtk_style_context_remove_provider(GTK_STYLE_CONTEXT(gtk_widget_get_style_context(generic_slider -> slider)), GTK_STYLE_PROVIDER(generic_slider -> css_provider));
	} else {
		gtk_widget_set_sensitive(picker, TRUE);
		generic_slider -> ignoring_color = 0;
		gtk_style_context_add_provider(GTK_STYLE_CONTEXT(gtk_widget_get_style_context(generic_slider -> slider)), GTK_STYLE_PROVIDER(generic_slider -> css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
}

static void generic_slider_update_mode(GtkToggleButton *radio, Generic_Slider *generic_slider) {
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *box;
	
	if (!gtk_toggle_button_get_active(radio)) {
		return;
	}
	
	hbox = gtk_widget_get_ancestor(GTK_WIDGET(radio), GTK_TYPE_BOX);
	label = gtk_container_get_children(GTK_CONTAINER(hbox)) -> data;
	box = gtk_widget_get_ancestor(generic_slider -> slider, GTK_TYPE_BOX);
	gtk_widget_show_all(box);
	
	if (!strncmp(gtk_label_get_text(GTK_LABEL(label)), "S", 1)) {
		/* Only the slider should be shown */
		generic_slider -> mode = 1;
		gtk_widget_hide(generic_slider -> label);
	} else if (!strncmp(gtk_label_get_text(GTK_LABEL(label)), "L", 1)) {
		/* Only the label should be shown */
		generic_slider -> mode = 2;
		gtk_widget_hide(generic_slider -> slider);
	} else {
		/* Both */
		generic_slider -> mode = 0;
	}
}

static void generic_slider_update_denominators(GtkAdjustment *adjustment, int *denominator) {
	*denominator = (int)(gtk_adjustment_get_value(adjustment));
}

static void generic_slider_update_commands(GtkWidget *entry, Generic_Slider *generic_slider) {
	const gchar *name;
	char *label_text;
	
	name = gtk_widget_get_name(entry);
	
	if (!strncmp(name, "A", 1)) {
		/* We're changing the command to adjust */
		free(generic_slider -> adjust_command);
		generic_slider -> adjust_command = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	} else if (!strncmp(name, "B", 1)) {
		/* We're changing the command with which to synchronize */
		free(generic_slider -> sync_command);
		generic_slider -> sync_command = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	} else {
		/* We're changing the slider's label */
		free(generic_slider -> description);
		generic_slider -> description = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
		
		label_text = parse_command(generic_slider -> description, (generic_slider -> description_denominator) * (generic_slider -> value), (generic_slider -> description_denominator) * (generic_slider -> delta));
		
		gtk_label_set_text(GTK_LABEL(generic_slider -> label), label_text);
		gtk_widget_set_tooltip_text(generic_slider -> slider, label_text);
		gtk_widget_set_tooltip_text(generic_slider -> label, label_text);
		free(label_text);
	}
}

static void generic_slider_properties_dialog(XfcePanelPlugin *plugin, Generic_Slider *generic_slider) {
	GtkWidget *hbox_1;
	GtkWidget *hbox_2;
	GtkWidget *hbox_3;
	GtkWidget *hbox_4;
	GtkWidget *hbox_5;
	GtkWidget *entry_1;
	GtkWidget *entry_2;
	GtkWidget *entry_3;
	GtkWidget *radio_1;
	GtkWidget *radio_2;
	GtkWidget *radio_3;
	GtkWidget *label_1_a;
	GtkWidget *label_1_b;
	GtkWidget *label_2_a;
	GtkWidget *label_2_b;
	GtkWidget *label_3_a;
	GtkWidget *label_3_b;
	GtkWidget *label_4;
	GtkWidget *label_5;
	GtkWidget *label_6;
	GtkWidget *label_7;
	GtkWidget *label_8;
	GtkWidget *frame_1;
	GtkWidget *frame_2;
	GtkWidget *bbox_1;
	GtkWidget *bbox_2;
	GtkWidget *check;
	GtkWidget *picker;
	GtkWidget *table;
	GtkWidget *dialog;
	
	GtkAdjustment *adjustment_1;
	GtkAdjustment *adjustment_2;
	GtkAdjustment *adjustment_3;
	GtkWidget *spin_1;
	GtkWidget *spin_2;
	GtkWidget *spin_3;
	
	/* Using pointers to give a one argument function two arguments is a stupid hack.
	 * I still stand by that.
	 */
	GList *stupid_hack = NULL;
	
	/* We don't want the slider invoking a nonexistent command while the command is being changed */
	generic_slider -> active = 0;
	
	dialog = xfce_titled_dialog_new_with_buttons(_("Generic Slider"), GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin))), GTK_DIALOG_DESTROY_WITH_PARENT, _("Close"), GTK_RESPONSE_OK, NULL);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "xfce4-settings");
	
	stupid_hack = g_list_append(stupid_hack, generic_slider);
	stupid_hack = g_list_append(stupid_hack, plugin);
	g_signal_connect(dialog, "response", G_CALLBACK(generic_slider_properties_dialog_response), stupid_hack);

	label_1_a = gtk_label_new("Adjust this command:");
	label_1_b = gtk_label_new("Denominator for adjusting:");
	label_2_a = gtk_label_new("Synchronize with this command:");
	label_2_b = gtk_label_new("Denominator for synchronizing:");
	label_3_a = gtk_label_new("Label for slider:");
	label_3_b = gtk_label_new("Denominator for label:");
	entry_1 = gtk_entry_new();
	entry_2 = gtk_entry_new();
	entry_3 = gtk_entry_new();
	gtk_widget_set_name(entry_1, "A");
	gtk_widget_set_name(entry_2, "B");
	gtk_widget_set_name(entry_3, "C");
	gtk_entry_set_text(GTK_ENTRY(entry_1), g_strdup(generic_slider -> adjust_command));
	gtk_entry_set_text(GTK_ENTRY(entry_2), g_strdup(generic_slider -> sync_command));
	gtk_entry_set_text(GTK_ENTRY(entry_3), g_strdup(generic_slider -> description));
	g_signal_connect(G_OBJECT(entry_1), "changed", G_CALLBACK(generic_slider_update_commands), generic_slider);
	g_signal_connect(G_OBJECT(entry_2), "changed", G_CALLBACK(generic_slider_update_commands), generic_slider);
	g_signal_connect(G_OBJECT(entry_3), "changed", G_CALLBACK(generic_slider_update_commands), generic_slider);
	adjustment_1 = gtk_adjustment_new(((gdouble)(generic_slider -> adjust_denominator)), 1.0, 65535.0, 1.0, 1.0, 0.0);
	adjustment_2 = gtk_adjustment_new(((gdouble)(generic_slider -> sync_denominator)), 1.0, 65535.0, 1.0, 1.0, 0.0);
	adjustment_3 = gtk_adjustment_new(((gdouble)(generic_slider -> description_denominator)), 1.0, 65535.0, 1.0, 1.0, 0.0);
	spin_1 = gtk_spin_button_new(adjustment_1, 0.5, 0);
	spin_2 = gtk_spin_button_new(adjustment_2, 0.5, 0);
	spin_3 = gtk_spin_button_new(adjustment_3, 0.5, 0);
	g_signal_connect(G_OBJECT(adjustment_1), "value_changed", G_CALLBACK(generic_slider_update_denominators), &(generic_slider -> adjust_denominator));
	g_signal_connect(G_OBJECT(adjustment_2), "value_changed", G_CALLBACK(generic_slider_update_denominators), &(generic_slider -> sync_denominator));
	g_signal_connect(G_OBJECT(adjustment_3), "value_changed", G_CALLBACK(generic_slider_update_denominators), &(generic_slider -> description_denominator));
	table = gtk_grid_new();
	gtk_grid_attach(GTK_GRID(table), label_1_a, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(table), entry_1, 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(table), label_1_b, 2, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(table), spin_1, 3, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(table), label_2_a, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(table), entry_2, 1, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(table), label_2_b, 2, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(table), spin_2, 3, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(table), label_3_a, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(table), entry_3, 1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(table), label_3_b, 2, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(table), spin_3, 3, 2, 1, 1);
	bbox_1 = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
	bbox_2 = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox_1), GTK_BUTTONBOX_SPREAD);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox_2), GTK_BUTTONBOX_SPREAD);
	//gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox_1), 10);
	//gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox_2), 10);
	label_4 = gtk_label_new("Slider");
	label_5 = gtk_label_new("Label");
	label_6 = gtk_label_new("Both");
	radio_1 = gtk_radio_button_new(NULL);
	radio_2 = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(radio_1));
	radio_3 = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(radio_2));
	g_signal_connect(G_OBJECT(radio_1), "toggled", G_CALLBACK(generic_slider_update_mode), generic_slider);
	g_signal_connect(G_OBJECT(radio_2), "toggled", G_CALLBACK(generic_slider_update_mode), generic_slider);
	g_signal_connect(G_OBJECT(radio_3), "toggled", G_CALLBACK(generic_slider_update_mode), generic_slider);
	hbox_1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	hbox_2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	hbox_3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	hbox_4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox_1), label_4, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_1), radio_1, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_2), label_5, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_2), radio_2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_3), label_6, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_3), radio_3, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(bbox_1), hbox_1);
	gtk_container_add(GTK_CONTAINER(bbox_1), hbox_2);
	gtk_container_add(GTK_CONTAINER(bbox_1), hbox_3);
	label_7 = gtk_label_new("Use default color");
	check = gtk_check_button_new();
	picker = gtk_color_button_new();
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(picker), &(generic_slider -> color));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), (generic_slider -> ignoring_color == 1) ? TRUE : FALSE);
	g_signal_connect(G_OBJECT(check), "toggled", G_CALLBACK(generic_slider_update_default), generic_slider);
	g_signal_connect(G_OBJECT(picker), "color-set", G_CALLBACK(generic_slider_update_color), generic_slider);
	gtk_box_pack_start(GTK_BOX(hbox_4), label_7, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_4), check, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(bbox_2), hbox_4);
	gtk_container_add(GTK_CONTAINER(bbox_2), picker);
	hbox_5 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	frame_1 = gtk_frame_new("Show:");
	frame_2 = gtk_frame_new("Color:");
	gtk_container_add(GTK_CONTAINER(frame_1), bbox_1);
	gtk_container_add(GTK_CONTAINER(frame_2), bbox_2);
	gtk_box_pack_start(GTK_BOX(hbox_5), frame_1, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(hbox_5), frame_2, TRUE, TRUE, 5);
	label_8 = gtk_label_new("%v for value between 0 and the denominator, %d for the change since last update");
	
	if (generic_slider -> mode == 1) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_1), TRUE);
	} else if (generic_slider -> mode == 2) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_2), TRUE);
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_3), TRUE);
	}
	
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), table, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox_5, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), label_8, FALSE, FALSE, 0);
	gtk_widget_show_all(dialog);
}

static void generic_slider_orientation_or_mode_changed(XfcePanelPlugin *plugin, gint vertical, Generic_Slider *generic_slider) {
	GtkWidget *slider = generic_slider -> slider;
	GtkWidget *label = generic_slider -> label;
	GtkWidget *box = gtk_widget_get_ancestor(label, GTK_TYPE_BOX);
	
	if (vertical) {
		gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
		gtk_widget_set_valign(box, GTK_ALIGN_FILL);
		gtk_orientable_set_orientation(GTK_ORIENTABLE(slider), GTK_ORIENTATION_HORIZONTAL);
		gtk_widget_set_size_request(slider, -1, WIDTH);
		gtk_widget_set_size_request(GTK_WIDGET(plugin), xfce_panel_plugin_get_size(plugin), -1);
		gtk_orientable_set_orientation(GTK_ORIENTABLE(box), GTK_ORIENTATION_VERTICAL);
	} else {
		gtk_widget_set_halign(box, GTK_ALIGN_FILL);
		gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
		gtk_orientable_set_orientation(GTK_ORIENTABLE(slider), GTK_ORIENTATION_VERTICAL);
		gtk_widget_set_size_request(slider, WIDTH, -1);
		gtk_widget_set_size_request(GTK_WIDGET(plugin), -1, xfce_panel_plugin_get_size(plugin));
		gtk_orientable_set_orientation(GTK_ORIENTABLE(box), GTK_ORIENTATION_HORIZONTAL);
	}
}

static void generic_slider_orientation_changed(XfcePanelPlugin *plugin, GtkOrientation orientation, Generic_Slider *generic_slider) {
	if (orientation == GTK_ORIENTATION_VERTICAL) {
		generic_slider_orientation_or_mode_changed(plugin, 1, generic_slider);
	} else {
		generic_slider_orientation_or_mode_changed(plugin, 0, generic_slider);
	}
}

static gboolean generic_slider_set_size(XfcePanelPlugin *plugin, int size) {
	if (xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_HORIZONTAL) {
		gtk_widget_set_size_request(GTK_WIDGET(plugin), -1, size);
	} else {
		gtk_widget_set_size_request(GTK_WIDGET(plugin), size, -1);
	}

	return TRUE;
}

static void generic_slider_free_data(XfcePanelPlugin *plugin, Generic_Slider *generic_slider) {
	GtkWidget *event_box;
	GtkWidget *slider;
	GtkWidget *label;
	GtkWidget *box;
	
	slider = generic_slider -> slider;
	label = generic_slider -> label;
	box = gtk_widget_get_ancestor(label, GTK_TYPE_BOX);
	event_box = gtk_widget_get_ancestor(slider, GTK_TYPE_EVENT_BOX);
	
	g_return_if_fail(plugin != NULL);
	g_return_if_fail(event_box != NULL);
	g_return_if_fail(box != NULL);
	g_return_if_fail(slider != NULL);
	g_return_if_fail(label != NULL);
	
	g_object_unref(G_OBJECT(slider));
	g_object_unref(G_OBJECT(label));
	g_object_unref(G_OBJECT(box));
	g_object_unref(G_OBJECT(event_box));
	
	if (generic_slider -> timeout_id != 0) {
		g_source_remove(generic_slider -> timeout_id);
	}
	if (generic_slider -> adjust_command != NULL) {
		free(generic_slider -> adjust_command);
	}
	if (generic_slider -> sync_command != NULL) {
		free(generic_slider -> sync_command);
	}
	if (generic_slider -> description != NULL) {
		free(generic_slider -> description);
	}
	free(generic_slider);
}

static void generic_slider_construct(XfcePanelPlugin *plugin) {
	Generic_Slider *generic_slider = calloc(1, sizeof(Generic_Slider));
	GtkStyle *pre_rc;
	GtkRcStyle *rc;
	GtkWidget *event_box;
	GtkWidget *slider;
	GtkWidget *label;
	GtkWidget *box;
	GList *stupid_hack = NULL;
	char *label_text;
	char *css;
	
	event_box = gtk_event_box_new();
	slider = gtk_progress_bar_new();
	label = gtk_label_new("");
	generic_slider -> timeout_id = 0;
	generic_slider -> value = 0.0;
	generic_slider -> delta = 0.0;
	generic_slider -> active = 0;
	generic_slider -> ignoring_color = 1;
	generic_slider -> adjust_denominator = 100;
	generic_slider -> sync_denominator = 100;
	generic_slider -> description_denominator = 100;
	generic_slider -> mode = 0;
	generic_slider -> slider = slider;
	generic_slider -> label = label;
	generic_slider -> description = calloc(1, sizeof(char));
	generic_slider -> adjust_command = calloc(1, sizeof(char));
	generic_slider -> sync_command = calloc(1, sizeof(char));
	generic_slider -> css_provider = gtk_css_provider_new();
	gdk_rgba_parse(&(generic_slider -> color), "blue");

	stupid_hack = g_list_append(stupid_hack, generic_slider);
	stupid_hack = g_list_append(stupid_hack, plugin);
	g_signal_connect(G_OBJECT(event_box), "button-press-event", G_CALLBACK(adjust_slider_cb), stupid_hack);
	g_signal_connect(G_OBJECT(event_box), "scroll-event", G_CALLBACK(scroll_slider_cb), stupid_hack);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(slider), 0.5);
	gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(slider), TRUE);
	
	if (xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_HORIZONTAL) {
		gtk_orientable_set_orientation(GTK_ORIENTABLE(slider), GTK_ORIENTATION_VERTICAL);
		gtk_widget_set_size_request(slider, WIDTH, -1);
		box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	} else {
		gtk_orientable_set_orientation(GTK_ORIENTABLE(slider), GTK_ORIENTATION_HORIZONTAL);
		gtk_widget_set_size_request(slider, -1, WIDTH);
		box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	}
	
	gtk_container_add(GTK_CONTAINER(event_box), slider);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), event_box, FALSE, FALSE, 0);
	
	xfce_panel_plugin_menu_show_configure (plugin);
	
	g_signal_connect(plugin, "orientation-changed", G_CALLBACK(generic_slider_orientation_changed), generic_slider);
	g_signal_connect(plugin, "configure-plugin", G_CALLBACK(generic_slider_properties_dialog), generic_slider);
	g_signal_connect(plugin, "size-changed", G_CALLBACK(generic_slider_set_size), NULL);
	g_signal_connect(plugin, "free-data", G_CALLBACK(generic_slider_free_data), generic_slider);
	g_signal_connect(plugin, "save", G_CALLBACK(generic_slider_write_rc_file), generic_slider);

	gtk_container_add(GTK_CONTAINER(plugin), box);
	xfce_panel_plugin_add_action_widget(plugin, box);
	xfce_panel_plugin_add_action_widget(plugin, event_box);
	xfce_panel_plugin_add_action_widget(plugin, slider);
	xfce_panel_plugin_add_action_widget(plugin, label);
	gtk_widget_show_all(box);
	
	generic_slider_read_rc_file(plugin, generic_slider);
	/* The rc file might've told us to hide some widgets or change the label or color */
	css = g_strdup_printf("progressbar progress { background-color: %s; }", gdk_rgba_to_string(&(generic_slider -> color)));
	gtk_css_provider_load_from_data(generic_slider -> css_provider, css, strlen(css), NULL);
	label_text = parse_command(generic_slider -> description, (generic_slider -> description_denominator) * (generic_slider -> value), (generic_slider -> description_denominator) * (generic_slider -> delta));
	gtk_label_set_text(GTK_LABEL(label), label_text);
	gtk_widget_set_tooltip_text(slider, label_text);
	gtk_widget_set_tooltip_text(label, label_text);
	free(label_text);
	free(css);
	
	if ((generic_slider -> mode) == 1) {
		gtk_widget_hide(label);
	} else if ((generic_slider -> mode) == 2) {
		gtk_widget_hide(slider);
	}
	
	if (generic_slider -> ignoring_color == 0) {
		gtk_style_context_add_provider(GTK_STYLE_CONTEXT(gtk_widget_get_style_context(generic_slider -> slider)), GTK_STYLE_PROVIDER(generic_slider -> css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
}

XFCE_PANEL_PLUGIN_REGISTER(generic_slider_construct);
