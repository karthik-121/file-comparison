all: compare
compare: compare.c
	gcc -Wall -Werror -pthread -Wvla -fsanitize=address,undefined -std=c99 compare.c -o compare -lm

clean: 
	rm -f compare