#ifndef EDITOR_H
#define EDITOR_H

#include "../include/types.h"

#define EDITOR_WIDTH 80
#define EDITOR_HEIGHT 25
#define BUFFER_SIZE (EDITOR_WIDTH * EDITOR_HEIGHT)

void editor_render();
void editor_render_status();
void editor_execute_command();
void editor_handle_key(char key);
void editor_load_file(const char *filename);
void editor_save_file(const char *filename);
void editor_run(const char *filename);

#endif // EDITOR_H
