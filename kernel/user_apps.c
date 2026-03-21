#include <desk/desk.h>

static void notes_paint(desk_window_t* win, void* user_data) {
    (void)user_data;

    desk_gfx_fill_rect(win, 0, 0, desk_window_client_width(win), 36, 0xfff3cd);
    desk_gfx_frame_rect(win, 0, 0, desk_window_client_width(win), 36, 0xd7c38a);
    desk_gfx_text(win, 10, 12, "MY APP WINDOW", 1, 0x3a3120);

    desk_gfx_text(win, 12, 58, "THIS APP COMES FROM KERNEL/USER_APPS.C", 1, 0x2b3545);
    desk_gfx_text(win, 12, 78, "EDIT THIS FILE TO CREATE YOUR OWN UI.", 1, 0x2b3545);
}

static void launch_notes(void) {
    (void)desk_window_create("MY NOTES", 220, 140, 360, 220,
                             DESK_WINDOW_CLOSABLE, notes_paint, 0);
}

void desk_register_user_apps(void) {
    (void)desk_register_app("MY NOTES", launch_notes);
}
