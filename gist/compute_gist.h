#ifndef COMPUTE_GIST_H_INCLUDED
#define COMPUTE_GIST_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include "gist.h"

using namespace std;
using namespace cv;

float* compute_gist(unsigned char *file, int *orient_per_pcale, int filesize = 0, int nblocks = 4, int has_orient_per_pcale = 0);

#endif