#ifndef COUNTER_H
#define COUNTER_H

extern int counter_value;

void counter_reset(int value);
int counter_next(void);
int counter_read(void);

#endif
