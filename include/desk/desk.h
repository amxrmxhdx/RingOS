#ifndef DESK_H
#define DESK_H

#include "../types.h"

typedef struct desk_window desk_window_t;

typedef void (*desk_paint_fn)(desk_window_t* win, void* user_data);
typedef void (*desk_app_entry_fn)(void);

typedef struct {
	int x;
	int y;
	int w;
	int h;
} desk_rect_t;

#define DESK_WINDOW_CLOSABLE   0x00000001u
#define DESK_WINDOW_RESIZABLE  0x00000002u /* reserved for future use */

/* Register an app entry shown in the Start menu. */
bool desk_register_app(const char* name, desk_app_entry_fn entry);

/* Optional hook called by the desktop during startup. */
void desk_register_user_apps(void);

/* Create/close windows. Coordinates are desktop pixels. */
desk_window_t* desk_window_create(const char* title, int x, int y, int w, int h,
								  uint32_t flags, desk_paint_fn paint, void* user_data);
void desk_window_close(desk_window_t* win);
void desk_window_set_title(desk_window_t* win, const char* title);
void desk_window_set_position(desk_window_t* win, int x, int y);
void desk_window_request_repaint(desk_window_t* win);

/* Query geometry in client-space pixels (excluding border/title bar). */
int desk_window_client_width(const desk_window_t* win);
int desk_window_client_height(const desk_window_t* win);

/* Drawing API for app callbacks (client-space coordinates). */
void desk_gfx_fill_rect(desk_window_t* win, int x, int y, int w, int h, uint32_t color);
void desk_gfx_frame_rect(desk_window_t* win, int x, int y, int w, int h, uint32_t color);
void desk_gfx_text(desk_window_t* win, int x, int y, const char* text, int scale, uint32_t color);

#endif
