#include <gtk/gtk.h>
#include <string.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <ctype.h>
#include <math.h>
//#include "navigation_map.h"
#include "interface_module.h"
#include "main_module.h"
#include "information_manager_module.h"
#include <cairo/cairo.h>
// Forward declaration for the animation function
void interface_animate_path(int (*pixels)[2], int length);
// Vehicle Structure 
GtkWidget *drawing_area = NULL;
int (*path)[2] = NULL;       // Pointer to 2D array for path
int path_length = 0;         // Total number of points in the path
int path_index = 0;  
// Global widgets
const char *vehicle_types[] = { "Car", "Ambulance", "Truck", "Bike", NULL };

GList *active_paths = NULL;
GdkTexture *image_texture = NULL;
GtkWidget *traffic_info_textview;
GtkWidget *source_entry, *destination_entry, *vehicle_dropdown;
GtkWidget *error_label;
GtkWidget *map_image_widget;
GtkWidget *scrolled_window;
// Zoom and pan variables
// Global scale and pan variables
double current_scale = 1.0;
double min_scale = 0.5;
double max_scale = 5.0;

gboolean is_panning = FALSE;
double pan_start_x = 0.0;
double pan_start_y = 0.0;

// Original map size (or texture size)
static const int original_width = 2048;
static const int original_height = 2048;

typedef struct {
    int (*path)[2];        // 2D array of coordinates
    int length;
    int index;
    guint timeout_id;
    gboolean show_label;
} AnimatedPath;

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
    GtkStringList *vehicle_model = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(vehicle_dropdown)));
    guint selected_index = gtk_drop_down_get_selected(GTK_DROP_DOWN(vehicle_dropdown));
    const gchar *selected_vehicle = gtk_string_list_get_string(vehicle_model, selected_index);
    Vehicle vehicle;
    vehicle.vehicle_type = selected_vehicle;
    vehicle.source = source;
    vehicle.destination = destination;
    g_print("Adding vehicle: %s from %s to %s\n", selected_vehicle, source, destination);
    add_new_vehicle(vehicle);
}
void display_traffic_info(const char *info_text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(traffic_info_textview));
    gtk_text_buffer_set_text(buffer, info_text, -1);
}

void interpolate_pixels(int x1, int y1, int x2, int y2, int (*output)[2], int *count) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        output[*count][0] = x1;
        output[*count][1] = y1;
        (*count)++;

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

static void draw_cb(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
   // g_print("\n[DRAW CALLBACK] Called with area: %p, width: %d, height: %d\n", area, width, height);

    GtkWidget *scrolled_window = gtk_widget_get_ancestor(GTK_WIDGET(area), GTK_TYPE_SCROLLED_WINDOW);
    if (!scrolled_window) {
      //  g_print("[WARN] Scrolled window not found for drawing area!\n");
        return;
    }

    GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));

    double x_off = gtk_adjustment_get_value(hadj);
    double y_off = gtk_adjustment_get_value(vadj);

    //g_print("[SCROLL] x_offset: %.2f, y_offset: %.2f\n", x_off, y_off);
   // g_print("[ZOOM] current_scale: %.2f\n", current_scale);

    cairo_save(cr);  // Save state before any transformation

    cairo_scale(cr, current_scale, current_scale);
    cairo_translate(cr, -x_off / current_scale, -y_off / current_scale);

    // --- Draw Background Image ---
    if (image_texture) {
        int iw = gdk_texture_get_width(image_texture);
        int ih = gdk_texture_get_height(image_texture);
       // g_print("[IMAGE] Drawing background of size: %d x %d\n", iw, ih);

        cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, iw, ih);
        unsigned char *pixels = cairo_image_surface_get_data(surface);
        gdk_texture_download(image_texture, pixels, cairo_image_surface_get_stride(surface));
        cairo_surface_mark_dirty(surface);

        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_paint(cr);
        cairo_surface_destroy(surface);
    } else {
        //g_print("[IMAGE] No texture available to draw.\n");
    }

    // --- Draw Vehicle Paths ---
    int path_count = 0;
    for (GList *l = active_paths; l != NULL; l = l->next) {
        AnimatedPath *anim = (AnimatedPath *)l->data;

        if (anim->path && anim->length > 1 && anim->index >= 0) {
            path_count++;
           // g_print("[PATH] Drawing path %d with %d points (index: %d)\n", path_count, anim->length, anim->index);

            cairo_set_source_rgba(cr, 0.68, 0.85, 0.90, 1.0);
            cairo_set_line_width(cr, 5.0 / current_scale);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

            cairo_move_to(cr, anim->path[0][0], anim->path[0][1]);
            for (int i = 1; i <= anim->index && i < anim->length; i++) {
                cairo_line_to(cr, anim->path[i][0], anim->path[i][1]);
            }
            cairo_stroke(cr);
        }

        if (anim->show_label && anim->length > 0) {
            int end_x = anim->path[anim->length - 1][0];
            int end_y = anim->path[anim->length - 1][1];

           // g_print("[LABEL] Drawing destination label at (%d, %d)\n", end_x, end_y);

            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 20.0 / current_scale);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

            double offset_x = 10.0 / current_scale;
            double offset_y = 10.0 / current_scale;

            cairo_move_to(cr, end_x + offset_x, end_y - offset_y);
            cairo_show_text(cr, "Destination");
        }
    }

   // g_print("[DRAW COMPLETE] %d path(s) drawn.\n", path_count);

    cairo_restore(cr);  // Restore state after drawing
}




gboolean remove_label_cb(gpointer user_data) {
    AnimatedPath *anim = (AnimatedPath *)user_data;
    //g_print("[REMOVE] Removing path and freeing memory for animation at address: %p\n", anim);

    active_paths = g_list_remove(active_paths, anim);

    if (anim->path) {
        g_free(anim->path);
        //g_print("[FREE] Freed anim->path\n");
    }
    g_free(anim);
   // g_print("[FREE] Freed anim struct\n");

    gtk_widget_queue_draw(drawing_area);
   // g_print("[DRAW] Requested redraw after label removal\n");

    return G_SOURCE_REMOVE;
}

gboolean tick_cb(gpointer user_data) {
    AnimatedPath *anim = (AnimatedPath *)user_data;

    if (anim->index < anim->length - 1) {
        anim->index++;
      //  g_print("[TICK] Advancing path index to: %d\n", anim->index);
        gtk_widget_queue_draw(drawing_area);
        return G_SOURCE_CONTINUE;
    }

    //g_print("[TICK COMPLETE] Path finished. Showing label and scheduling removal...\n");
    anim->show_label = TRUE;
    g_timeout_add(2000, remove_label_cb, anim);

    gtk_widget_queue_draw(drawing_area);
    return G_SOURCE_REMOVE;
}

void draw_path_on_map(int (*new_path)[2], int new_length) {
   // g_print("[DRAW PATH] Received path with %d points\n", new_length);

    AnimatedPath *anim = g_malloc0(sizeof(AnimatedPath));
   // g_print("[ALLOC] Allocated AnimatedPath at address: %p\n", anim);

    anim->path = g_malloc0(sizeof(int[2]) * new_length);
    anim->length = new_length;
    anim->index = 0;

    for (int i = 0; i < new_length; i++) {
        anim->path[i][0] = new_path[i][0];
        anim->path[i][1] = new_path[i][1];
        //g_print("[PATH %d] x: %d, y: %d\n", i, anim->path[i][0], anim->path[i][1]);
    }

    active_paths = g_list_append(active_paths, anim);
   // g_print("[LIST] Added path to active_paths list\n");

    anim->timeout_id = g_timeout_add(8, tick_cb, anim);
   // g_print("[TIMER] Animation started with timeout ID: %d\n", anim->timeout_id);
}


// Animation callback implementation
void interface_animate_path(int (*pixels)[2], int length) {
    draw_path_on_map(pixels, length);
}

static gboolean on_button_press(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    if (n_press == 1) {
        is_panning = TRUE;
        pan_start_x = x;
        pan_start_y = y;
        return TRUE;
    }
    return FALSE;
}

static gboolean on_button_release(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    is_panning = FALSE;
    return FALSE;
}

static gboolean on_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
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

static gboolean zoom_map(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data) {
    GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
    GdkModifierType state = gdk_event_get_modifier_state(event);

    if (!(state & GDK_CONTROL_MASK) || dy == 0)
        return FALSE;

    double zoom_factor = (dy < 0) ? 1.1 : 0.9;
    double new_scale = current_scale * zoom_factor;
    new_scale = CLAMP(new_scale, min_scale, max_scale);
    if (new_scale == current_scale)
        return TRUE;

    double pointer_x, pointer_y;
    gdk_event_get_position(event, &pointer_x, &pointer_y);

    GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
    double old_value_x = gtk_adjustment_get_value(hadj);
    double old_value_y = gtk_adjustment_get_value(vadj);

    // ✅ Store original dimensions of map
    // ✅ Apply new zoom scale
    current_scale = new_scale;

    // ✅ Resize both drawing area and image to match new scale
    int new_width = original_width * current_scale;
    int new_height = original_height * current_scale;

    gtk_widget_set_size_request(map_image_widget, new_width, new_height);
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(drawing_area), new_width);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(drawing_area), new_height);

    // ✅ Queue resize and redraw
    gtk_widget_queue_resize(map_image_widget);
    gtk_widget_queue_draw(map_image_widget);

    gtk_widget_queue_resize(drawing_area);
    gtk_widget_queue_draw(drawing_area);

    // ✅ Adjust scroll position to keep zoom centered on pointer
    double new_value_x = (old_value_x + pointer_x) * zoom_factor - pointer_x;
    double new_value_y = (old_value_y + pointer_y) * zoom_factor - pointer_y;
   // g_print("Zoom triggered: dy = %f, ctrl pressed = %s\n", dy, (state & GDK_CONTROL_MASK) ? "yes" : "no");

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
    gtk_picture_set_content_fit(GTK_PICTURE(map_image_widget), GTK_CONTENT_FIT_FILL);
    gtk_widget_set_size_request(map_image_widget, 4000, 3000);
    GtkWidget *overlay = gtk_overlay_new();
gtk_overlay_set_child(GTK_OVERLAY(overlay), map_image_widget);
// Drawing area on top for paths
drawing_area = gtk_drawing_area_new();
gtk_widget_set_hexpand(drawing_area, TRUE);
gtk_widget_set_vexpand(drawing_area, TRUE);
gtk_overlay_add_overlay(GTK_OVERLAY(overlay), drawing_area);
gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), draw_cb, NULL, NULL);
// Put overlay inside the scrolled window
gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), overlay);

    // Zoom controller
    GtkEventController *scroll_controller = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll_controller, "scroll", G_CALLBACK(zoom_map), NULL);
    gtk_widget_add_controller(overlay, scroll_controller);
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
    // Only register the animation callback, do not trigger animation
    extern void (*animate_path_callback)(int (*pixels)[2], int length);
    animate_path_callback = interface_animate_path;
    // After creating traffic_info_textview, clear the info panel
    //display_traffic_info("");
    // Before showing the window, clear any existing active_paths
    for (GList *l = active_paths; l != NULL; l = l->next) {
        AnimatedPath *anim = (AnimatedPath *)l->data;
        if (anim) {
            if (anim->path) g_free(anim->path);
            g_free(anim);
        }
    }
    g_list_free(active_paths);
    active_paths = NULL;
    start_display_loop(); 
    gtk_window_present(GTK_WINDOW(window));
    
}
int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.traffic.optimizer", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
