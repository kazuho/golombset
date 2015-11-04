all: test.exe golombset

check: test.exe
	./test.exe

test.exe:
	$(CC) -Wall test.c -o $@

golombset:
	$(CC) -Wall cmd.c -o $@

clean:
	rm -f test.exe golombset

.PHONY: check clean
