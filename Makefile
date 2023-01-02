CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -g
LDFLAGS = -lsqlite3 -lcurl

tagger: initfolders build/tagger.o build/database.o build/provider_utils.o
	$(CC) build/tagger.o build/database.o build/provider_utils.o $(LDFLAGS) -o tagger

build/tagger.o: src/tagger.c include/tagger.h
	$(CC) $(CFLAGS) -c src/tagger.c -o build/tagger.o

build/database.o: src/database.c include/database.h
	$(CC) $(CFLAGS) -c src/database.c -o build/database.o

build/provider_utils.o: src/provider_utils.c include/provider_utils.h
	$(CC) $(CFLAGS) -c src/provider_utils.c -o build/provider_utils.o

initfolders:
	mkdir -p build

clean:
	rm -rf build/*
	rm -f tagger
	rm -f test.tdb

testleaks: tagger
	valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         ./tagger

