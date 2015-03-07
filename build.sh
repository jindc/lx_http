gcc -W -Werror -g  -I. -I ./test  -I../lxlib -l pthread -o test/test test/*.c *.c ../lxlib/*.c
