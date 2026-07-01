CC ?= gcc
CFLAGS ?= -O0 -Wall -Wextra
OBJS = main.o math.o

demo: $(OBJS)
	@echo "LINK $@"
	$(CC) $(OBJS) -o $@

%.o: %.c math.h
	@echo "CC $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) demo
