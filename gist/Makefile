# Lear's GIST implementation, version 1.1, (c) INRIA 2009, Licence: GPL


all: gist.a gist.o standalone_image.o compute_gist.o

WFFTINC=
WFFTLIB=
OPENCV= `pkg-config --libs --cflags opencv`
# WFFTINC=-I/scratch/adonis/mordelet/install/include
# WFFTLIB=-L/scratch/adonis/mordelet/install/lib


# gist.c: ../../descriptors/gist.c 
#	cp $< $@

gist.a: gist.o standalone_image.o compute_gist.o
	ar rcs libgist.a gist.o standalone_image.o compute_gist.o

compute_gist.o: compute_gist.cpp compute_gist.h gist.h standalone_image.h
	g++ -std=c++11 -c -Wall -g $< $(OPENCV) -lfftw3f -lm -ljpeg

gist.o: gist.cpp gist.h standalone_image.h
	g++ -std=c++11 -c -Wall -g $< -DUSE_GIST -DSTANDALONE_GIST -lfftw3f -lm

standalone_image.o: standalone_image.cpp standalone_image.h
	g++ -std=c++11 -c -Wall -g $< 

clean: 
	rm -f *.o

