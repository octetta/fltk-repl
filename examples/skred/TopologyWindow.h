#ifndef SKRED_TOPOLOGY_WINDOW_H
#define SKRED_TOPOLOGY_WINDOW_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Render the machine-readable Skred voice graph in a dedicated window. */
int topology_show_voice(int voice, int depth, char *error, size_t error_capacity);
void topology_hide(void);

#ifdef __cplusplus
}
#endif

#endif
