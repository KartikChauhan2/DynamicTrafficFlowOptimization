#include <gtk/gtk.h>
#include <string.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <ctype.h>
#include <math.h>
#include "navigation_map.h"
#include "interface_module.h"
#include "main_module_h"
const char *vehicle_types[] = { "Car", "Ambulance", "Truck", "Bike", NULL };
// Vehicle Structure 
typedef struct {
    const char *vehicle_type;
    const char *source;
    const char *destination;
} Vehicle;
// Global widgets
GtkWidget *traffic_info_textview;
GtkWidget *source_entry, *destination_entry, *vehicle_dropdown;
GtkWidget *error_label;
GtkWidget *map_image_widget;
GtkWidget *scrolled_window;
// Zoom and pan variables
double current_scale = 1.0;
double min_scale = 0.5;
double max_scale = 5.0;
gboolean is_panning = FALSE;
double pan_start_x, pan_start_y;
void validate_input(GtkButton *button, gpointer user_data) {
    const gchar *source = gtk_editable_get_text(GTK_EDITABLE(source_entry));
    const gchar *destination = gtk_editable_get_text(GTK_EDITABLE(destination_entry));
    if (strlen(source) != 1 || strlen(destination) != 1 ||
        !isupper(source[0]) || !isupper(destination[0]) ||
        source[0] < 'A' || source[0] > 'O' ||
        destination[0] < 'A' || destination[0] > 'O') {
        gtk_label_set_text(GTK_LABEL(error_label),
                        "Enter values from A to O only (uppercase).");
        gtk_widget_set_visible(error_label, TRUE);
        return;
    }
    if (source[0] == destination[0]) {
        gtk_label_set_text(GTK_LABEL(error_label),
                        "Source and Destination cannot be the same.");
        gtk_widget_set_visible(error_label, TRUE);
        return;
    }
    gtk_widget_set_visible(error_label, FALSE);
    GtkWidget *selected_item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(vehicle_dropdown));
    const gchar *selected_vehicle = (const gchar *)selected_item;
    // Vehicle Structure 
    Vehicle vehicle;
    vehicle.vehicle_type = selected_vehicle;
    vehicle.source = source;
    vehicle.destination = destination;
    // Call the function to add the new vehicle
    add_new_vehicle(vehicle);
}
void display_traffic_info(const char *info_text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(traffic_info_textview));
    gtk_text_buffer_set_text(buffer, info_text, -1);
}

static gboolean on_button_press(GtkGestureClick *gesture, 
                               int n_press, 
                               double x, 
                               double y, 
                               gpointer user_data) {
    if (n_press == 1) {
        is_panning = TRUE;
        pan_start_x = x;
        pan_start_y = y;
        return TRUE;
    }
    return FALSE;
}
static gboolean on_button_release(GtkGestureClick *gesture, 
                                 int n_press, 
                                 double x, 
                                 double y, 
                                 gpointer user_data) {
    is_panning = FALSE;
    return FALSE;
}
static gboolean on_motion(GtkEventControllerMotion *controller,
                         double x,
                         double y,
                         gpointer user_data) {
    if (is_panning) {
        GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
        double dx = pan_start_x - x;
        double dy = pan_start_y - y;
        gtk_adjustment_set_value(hadj, gtk_adjustment_get_value(hadj) + dx);
        gtk_adjustment_set_value(vadj, gtk_adjustment_get_value(vadj) + dy);
        pan_start_x = x;
        pan_start_y = y;
        return TRUE;
    }
    return FALSE;
}
static gboolean zoom_map(GtkEventControllerScroll *controller,
    double dx,
    double dy,
    gpointer user_data) {
    GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
    GdkModifierType state = gdk_event_get_modifier_state(event);
    if (!(state & GDK_CONTROL_MASK)) {
        return FALSE;
    }
    if (dy == 0) {
        return FALSE;
    }
    double zoom_factor = (dy < 0) ? 1.1 : 0.9;
    double new_scale = current_scale * zoom_factor;
    // Clamp the scale
    new_scale = CLAMP(new_scale, min_scale, max_scale);
    if (new_scale == current_scale) {
        return TRUE;
    }
    double pointer_x, pointer_y;
    gdk_event_get_position(event, &pointer_x, &pointer_y);
    GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
    double old_value_x = gtk_adjustment_get_value(hadj);
    double old_value_y = gtk_adjustment_get_value(vadj);
    // Set new scale
    current_scale = new_scale;
    // Assume original image size is 400x300
    int original_width = 2048;
    int original_height = 2048;
    gtk_widget_set_size_request(map_image_widget,
               original_width * current_scale,
               original_height * current_scale);
    // Calculate new scroll values to keep zoom centered at cursor
    double new_value_x = (old_value_x + pointer_x) * zoom_factor - pointer_x;
    double new_value_y = (old_value_y + pointer_y) * zoom_factor - pointer_y;
    gtk_adjustment_set_value(hadj, new_value_x);
    gtk_adjustment_set_value(vadj, new_value_y);
    return TRUE;
}
static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Dynamic Traffic Flow Optimizer");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 600);
    // Main layout - uses GtkPaned for resizable split
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_window_set_child(GTK_WINDOW(window), paned);
    // ===== Left Side (Map) =====
    GtkWidget *map_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(map_box, 5);
    gtk_widget_set_margin_end(map_box, 5);
    gtk_widget_set_margin_top(map_box, 5);
    gtk_widget_set_margin_bottom(map_box, 5);
    gtk_paned_set_start_child(GTK_PANED(paned), map_box);
    // Map header
    GtkWidget *map_header = gtk_label_new("Live Map");
    gtk_widget_set_halign(map_header, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(map_header, "title-4");
    gtk_box_append(GTK_BOX(map_box), map_header);
    // Scrolled window for map
    scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_box_append(GTK_BOX(map_box), scrolled_window);
    // Map image
    if (g_file_test("map_placeholder2.png", G_FILE_TEST_EXISTS)) {
        map_image_widget = gtk_picture_new_for_filename("map_placeholder2.png");
    } else {
        map_image_widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget *fallback_label = gtk_label_new("Map Placeholder (image not found)");
        gtk_widget_set_halign(fallback_label, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(fallback_label, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(map_image_widget), fallback_label);
        gtk_widget_add_css_class(map_image_widget, "map-fallback");
    }
    gtk_picture_set_content_fit(GTK_PICTURE(map_image_widget), GTK_CONTENT_FIT_COVER);
    gtk_widget_set_size_request(map_image_widget, 400, 300);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), map_image_widget);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Zoom controller
    GtkEventController *scroll_controller = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll_controller, "scroll", G_CALLBACK(zoom_map), NULL);
    gtk_widget_add_controller(map_image_widget, scroll_controller);
    // Pan controllers
    GtkGesture *click_gesture = gtk_gesture_click_new();
    g_signal_connect(click_gesture, "pressed", G_CALLBACK(on_button_press), NULL);
    g_signal_connect(click_gesture, "released", G_CALLBACK(on_button_release), NULL);
    gtk_widget_add_controller(map_image_widget, GTK_EVENT_CONTROLLER(click_gesture));
    GtkEventController *motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "motion", G_CALLBACK(on_motion), NULL);
    gtk_widget_add_controller(map_image_widget, motion_controller);
    // ===== Right Side (Controls) =====
    GtkWidget *control_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(control_box, 5);
    gtk_widget_set_margin_end(control_box, 5);
    gtk_widget_set_margin_top(control_box, 5);
    gtk_widget_set_margin_bottom(control_box, 5);
    gtk_widget_set_size_request(control_box, 300, -1);
    gtk_paned_set_end_child(GTK_PANED(paned), control_box);
    // Header
    GtkWidget *panel_header = gtk_label_new("Traffic Controls");
    gtk_widget_set_halign(panel_header, GTK_ALIGN_START);
    gtk_widget_add_css_class(panel_header, "title-2");
    gtk_box_append(GTK_BOX(control_box), panel_header);
    // Input fields
    source_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(source_entry), "Source (A-O)");
    gtk_box_append(GTK_BOX(control_box), source_entry);
    destination_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(destination_entry), "Destination (A-O)");
    gtk_box_append(GTK_BOX(control_box), destination_entry);
    // Vehicle dropdown
    GListModel *vehicle_model = G_LIST_MODEL(gtk_string_list_new(vehicle_types));
    vehicle_dropdown = gtk_drop_down_new(vehicle_model, NULL);
    gtk_box_append(GTK_BOX(control_box), vehicle_dropdown);
    // Add button
    GtkWidget *add_btn = gtk_button_new_with_label("Add Vehicle");
    gtk_box_append(GTK_BOX(control_box), add_btn);
    // Error label
    error_label = gtk_label_new("");
    gtk_widget_add_css_class(error_label, "error");
    gtk_widget_set_visible(error_label, FALSE);
    gtk_box_append(GTK_BOX(control_box), error_label);
    // Traffic information display
    GtkWidget *info_header = gtk_label_new("Traffic Information");
    gtk_widget_set_halign(info_header, GTK_ALIGN_START);
    gtk_widget_add_css_class(info_header, "title-3");
    gtk_box_append(GTK_BOX(control_box), info_header);
    GtkWidget *info_scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(info_scrolled_window, TRUE);
    gtk_widget_set_hexpand(info_scrolled_window, TRUE);
    gtk_box_append(GTK_BOX(control_box), info_scrolled_window);
    // Create a GtkTextView for traffic info
    traffic_info_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(traffic_info_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(traffic_info_textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(traffic_info_textview), GTK_WRAP_WORD);
    gtk_widget_add_css_class(traffic_info_textview, "monospace");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(info_scrolled_window), traffic_info_textview);
    // Set monospace font for the textview
     traffic_info_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(traffic_info_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(traffic_info_textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(traffic_info_textview), GTK_WRAP_WORD);
    gtk_widget_add_css_class(traffic_info_textview, "monospace");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(info_scrolled_window), traffic_info_textview);
     GtkCssProvider *font_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(font_provider, 
        "textview.monospace { font-family: monospace; font-size: 10pt; }");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                             GTK_STYLE_PROVIDER(font_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    // Connect signals
    g_signal_connect(add_btn, "clicked", G_CALLBACK(validate_input), NULL);
    // CSS styling
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        " .title-2 { font-size: 24px; font-weight: bold; margin-bottom: 10px; }"
        " .title-3 { font-size: 18px; font-weight: bold; margin-top: 15px; margin-bottom: 5px; }"
        " .title-4 { font-size: 18px; font-weight: bold; margin-bottom: 5px; }"
        " .error { color: red; font-size: 14px; margin-top: 5px; }"
        " .map-fallback { background-color: #f0f0f0; padding: 50px; }"
        " .monospace { font-family: monospace; }"
    );
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                             GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);
    // Display initial traffic info
    // u dont even have to call it here 
    display_traffic_info(
        "ID\tVehicle Type\tSource\tDestination\tElapsed(s)\tTotal(s)\n"
        "---     ------------    ------   -----------    -----------    ----------\n"
        "154\tCar\t\tA\t\tB\t\t2\t\t5\n"
        "200\tBus\t\tX\t\tY\t\t3\t\t7\n"
        "301\tAmbulance\tC\t\tD\t\t1\t\t4\n"
        "422\tTruck\t\tE\t\tF\t\t5\t\t12\n"
    );
    gtk_window_present(GTK_WINDOW(window));
}
int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.traffic.optimizer", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
