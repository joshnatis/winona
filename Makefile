CC=g++
CFLAGS=-lncurses -Ilibraries -Wall -Wextra
LIBDIR=./libraries

winona: $(LIBDIR)/dr_flac.h $(LIBDIR)/dr_mp3.h $(LIBDIR)/dr_wav.h $(LIBDIR)/miniaudio.h winona.h winona.cpp
	$(CC) winona.cpp $(CFLAGS) -o winona
