CC = gcc
test_timer:simple_timer.o main.o
	gcc $^ -o $@ -I. -lrt -lpthread

%.o : %.c
	gcc -c $< -o $@ -std=c99 -D_POSIX_C_SOURCE=199309L
	
clean:
	rm -rf *.o test_timer
