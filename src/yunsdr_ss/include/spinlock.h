
#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__
#include <stdbool.h>

void spinlock_init();

void spinlock_deinit();

void lock();

bool trylock();

void unlock();

#endif
