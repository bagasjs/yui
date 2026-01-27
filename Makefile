CC := clang
CFLAGS := -Wall -Wextra -pedantic -g -fsanitize=address
LFLAGS := -L$(HOME)\Software\lib -lraylibdll

main.exe: main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)
