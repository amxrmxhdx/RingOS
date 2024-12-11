#ifndef RINGOS_SHELL_H
#define RINGOS_SHELL_H

void shell_init(void);
void shell_return_from_program(void);
void shell_run(void);
void shell_process_command(void);
void shell_handle_keypress(char c);

#endif /* RINGOS_SHELL_H */
