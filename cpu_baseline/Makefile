CC ?= gcc
CFLAGS ?= -O3 -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off
LDLIBS ?= -lm

.PHONY: all clean

all: nn_benchmark energy_benchmark

nn_benchmark: benchmark.o nn_inference.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

energy_benchmark: energy_benchmark.o nn_inference.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

benchmark.o: benchmark.c nn_inference.h generated/model_data.h
	$(CC) $(CFLAGS) -c -o $@ benchmark.c

energy_benchmark.o: energy_benchmark.c nn_inference.h generated/model_data.h
	$(CC) $(CFLAGS) -c -o $@ energy_benchmark.c

nn_inference.o: nn_inference.c nn_inference.h generated/model_data.h
	$(CC) $(CFLAGS) -c -o $@ nn_inference.c

clean:
	rm -f nn_benchmark energy_benchmark benchmark.o energy_benchmark.o nn_inference.o c_scores_f32.bin
