//write here

#ifndef _ROTATION_H
#define _ROTATION_H

int set_roatation(int degree);

int rotlock_read (int degree, int range);

int rotlock_write(int degree, int range);

int rotunlock_read(int degree, int range);

int rotunlock_write(int degree, int range);

void exit_rotlock (void);

#endif
