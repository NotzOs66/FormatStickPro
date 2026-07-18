#ifndef _URCU_UATOMIC_H
#define _URCU_UATOMIC_H

#include <stdint.h>

#define uatomic_read(ptr) (*(ptr))
#define uatomic_set(ptr, v) (*(ptr) = (v))
#define uatomic_add(ptr, v) __sync_fetch_and_add(ptr, v)
#define uatomic_sub(ptr, v) __sync_fetch_and_sub(ptr, v)
#define uatomic_inc(ptr) __sync_fetch_and_add(ptr, 1)
#define uatomic_dec(ptr) __sync_fetch_and_sub(ptr, 1)
#define uatomic_add_return(ptr, v) __sync_add_and_fetch(ptr, v)
#define uatomic_sub_return(ptr, v) __sync_sub_and_fetch(ptr, v)
#define uatomic_cmpxchg(ptr, old, new) __sync_val_compare_and_swap(ptr, old, new)

#define cmm_smp_mb() __sync_synchronize()

#endif
