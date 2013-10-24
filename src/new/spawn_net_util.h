#ifndef SPAWN_NET_UTIL_H
#define SPAWN_NET_UTIL_H

#include "spawn_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* write string to spawn_net channel */
void spawn_net_write_str(const spawn_net_channel* ch, const char* str);

/* read string from spawn_net channel, returns newly allocated string */
char* spawn_net_read_str(const spawn_net_channel* ch);

/* pack specified strmap and write it to channel */
void spawn_net_write_strmap(const spawn_net_channel* ch, const strmap* map);

/* read packed strmap from channel into specified map */
void spawn_net_read_strmap(const spawn_net_channel* ch, strmap* map);

#ifdef __cplusplus
}
#endif
#endif /* SPAWN_NET_UTIL_H */
