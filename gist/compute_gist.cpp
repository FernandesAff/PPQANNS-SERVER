#include "compute_gist.h"

static color_image_t *load_ppm(const char *fname){
  FILE *f=fopen(fname,"r");
  if(!f) {
    perror("could not open infile");
    exit(1);
  }
  int px,width,height,maxval;
  if(fscanf(f,"P%d %d %d %d",&px,&width,&height,&maxval)!=4 || 
     maxval!=255 || (px!=6 && px!=5)) {
    fprintf(stderr,"Error: input not a raw PGM/PPM with maxval 255\n");
    exit(1);    
  }
  fgetc(f); /* eat the newline */
  color_image_t *im=color_image_new(width,height);

  int i;
  for(i=0;i<width*height;i++) {
    im->c1[i]=fgetc(f);
    if(px==6) {
      im->c2[i]=fgetc(f);
      im->c3[i]=fgetc(f);    
    } else {
      im->c2[i]=im->c1[i];
      im->c3[i]=im->c1[i];   
    }
  }
  
  fclose(f);
  return im;
}

static color_image_t *load_image(unsigned char *file, int filesize) {
  Mat decodedImage;
  
  if(filesize == 0){
    decodedImage  = imread((char*)file, CV_LOAD_IMAGE_COLOR);
  }else{
    Mat rawData( 1, filesize, CV_8UC1, file);
    decodedImage  =  imdecode(rawData, CV_LOAD_IMAGE_COLOR);
  }

  cout << "rows" << decodedImage.rows << "cols" << decodedImage.cols;

  color_image_t *im=color_image_new(decodedImage.cols,decodedImage.rows);

  uint8_t* pixelPtr = (uint8_t*)decodedImage.data;
  Scalar_<uint8_t> bgrPixel;
  int cn = decodedImage.channels();
  for(int i = 0; i < decodedImage.rows; i++) {
    for(int j = 0; j < decodedImage.cols; j++) {
        im->c1[i*decodedImage.cols+j] = pixelPtr[i*decodedImage.cols*cn + j*cn + 2]; // R
        im->c2[i*decodedImage.cols+j] = pixelPtr[i*decodedImage.cols*cn + j*cn + 1]; // G
        im->c3[i*decodedImage.cols+j] = pixelPtr[i*decodedImage.cols*cn + j*cn + 0]; // B
    }
  }
  return im;
}

static color_image_t *load_jpeg(unsigned char *file, int filesize) {
  FILE *file_buff;
  
  if(filesize == 0){
    file_buff = fopen( (char*)file, "rb" );
    if ( file_buff == NULL ){
      return NULL;
    }
  }

  struct jpeg_decompress_struct info; //for our jpeg info
  struct jpeg_error_mgr err; //the error handler

  info.err = jpeg_std_error( &err );     
  jpeg_create_decompress( &info ); //fills info structure

  if(filesize == 0){
    jpeg_stdio_src( &info, file_buff); 
  }else{
    jpeg_mem_src( &info, file, filesize);    
  }
  jpeg_read_header( &info, true );

  jpeg_start_decompress( &info );

  int w = info.output_width;
  int h = info.output_height;
  int numChannels = info.num_components; // 3 = RGB, 4 = RGBA
  unsigned long dataSize = w * h * numChannels;

  // read RGB(A) scanlines one at a time into jdata[]
  unsigned char *data = (unsigned char *)malloc( dataSize );
  unsigned char* rowptr;
  while ( info.output_scanline < h ) {
      rowptr = data + info.output_scanline * w * numChannels;
      jpeg_read_scanlines( &info, &rowptr, 1 );
  }

  jpeg_finish_decompress( &info );    

  color_image_t *im=color_image_new(w,h);

  int i;
  for(i=0;i<w*h;i++) {
    im->c1[i] = data[i*3];
    im->c2[i] = data[i*3 + 1];
    im->c3[i] = data[i*3 + 2];
  }

  return im;
}

//compute_gist options... [infilename]
//infile is a PPM raw file
//options:
//[-nblocks nb] use a grid of nb*nb cells (default 4)
//[-orientationsPerScale o_1,..,o_n] use n scales and compute o_i orientations for scale i

float* compute_gist(unsigned char *file, int *orient_per_scale, int filesize, int nblocks, int has_orient_per_scale) {
  FILE *file_buff;
  
  int n_scale = 3;
  int *orientations_per_scale = (int *) malloc(sizeof(int)*50);
  orientations_per_scale[0] = 8;
  orientations_per_scale[1] = 8;
  orientations_per_scale[2] = 4;

  if(has_orient_per_scale) {
      n_scale=0;
      orientations_per_scale = orient_per_scale;
  }
  color_image_t *im = load_image(file, filesize);
  // color_image_t *im=load_jpeg(file, filesize);
  
  float *desc=color_gist_scaletab(im,nblocks,n_scale,orientations_per_scale);

  int i;
  
  int descsize=0;
  /* compute descriptor size */
  for(i=0;i<n_scale;i++) 
    descsize+=nblocks*nblocks*orientations_per_scale[i];

  descsize*=3; /* color */

  color_image_delete(im);

  return desc; 
}