#ifndef BUTTON_TASK_H
#define BUTTON_TASK_H

#include <stdbool.h>
#include <stdint.h>

void button_task_init(void);
void button_task_handle(void);
bool button_task_is_pressed(void);
uint32_t button_task_get_press_count(void);

#endif
