/// image8bit - A simple image processing module.
///
/// This module is part of a programming project
/// for the course AED, DETI / UA.PT
///
/// You may freely use and modify this code, at your own risk,
/// as long as you give proper credit to the original and subsequent authors.
///
/// João Manuel Rodrigues <jmr@ua.pt>
/// 2013, 2023

// Student authors (fill in below):
// NMec:  Name:
// 
// 
// 
// Date:
//

#include "image8bit.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "instrumentation.h"
#include <math.h>

// The data structure
//
// An image is stored in a structure containing 3 fields:
// Two integers store the image width and height.
// The other field is a pointer to an array that stores the 8-bit gray
// level of each pixel in the image.  The pixel array is one-dimensional
// and corresponds to a "raster scan" of the image from left to right,
// top to bottom.
// For example, in a 100-pixel wide image (img->width == 100),
//   pixel position (x,y) = (33,0) is stored in img->pixel[33];
//   pixel position (x,y) = (22,1) is stored in img->pixel[122].
// 
// Clients should use images only through variables of type Image,
// which are pointers to the image structure, and should not access the
// structure fields directly.

// Maximum value you can store in a pixel (maximum maxval accepted)
const uint8 PixMax = 255;

// Internal structure for storing 8-bit graymap images
struct image {
  int width;
  int height;
  int maxval;   // maximum gray value (pixeis with maxval are pure WHITE)
  uint8* pixel; // pixel data (a raster scan)
};


// This module follows "design-by-contract" principles.
// Read `Design-by-Contract.md` for more details.

/// Error handling functions

// In this module, only functions dealing with memory allocation or file
// (I/O) operations use defensive techniques.
// 
// When one of these functions fails, it signals this by returning an error
// value such as NULL or 0 (see function documentation), and sets an internal
// variable (errCause) to a string indicating the failure cause.
// The errno global variable thoroughly used in the standard library is
// carefully preserved and propagated, and clients can use it together with
// the ImageErrMsg() function to produce informative error messages.
// The use of the GNU standard library error() function is recommended for
// this purpose.
//
// Additional information:  man 3 errno;  man 3 error;

// Variable to preserve errno temporarily
static int errsave = 0;

// Error cause
static char* errCause;

/// Error cause.
/// After some other module function fails (and returns an error code),
/// calling this function retrieves an appropriate message describing the
/// failure cause.  This may be used together with global variable errno
/// to produce informative error messages (using error(), for instance).
///
/// After a successful operation, the result is not garanteed (it might be
/// the previous error cause).  It is not meant to be used in that situation!
char* ImageErrMsg() { ///
  return errCause;
}


// Defensive programming aids
//
// Proper defensive programming in C, which lacks an exception mechanism,
// generally leads to possibly long chains of function calls, error checking,
// cleanup code, and return statements:
//   if ( funA(x) == errorA ) { return errorX; }
//   if ( funB(x) == errorB ) { cleanupForA(); return errorY; }
//   if ( funC(x) == errorC ) { cleanupForB(); cleanupForA(); return errorZ; }
//
// Understanding such chains is difficult, and writing them is boring, messy
// and error-prone.  Programmers tend to overlook the intricate details,
// and end up producing unsafe and sometimes incorrect programs.
//
// In this module, we try to deal with these chains using a somewhat
// unorthodox technique.  It resorts to a very simple internal function
// (check) that is used to wrap the function calls and error tests, and chain
// them into a long Boolean expression that reflects the success of the entire
// operation:
//   success = 
//   check( funA(x) != error , "MsgFailA" ) &&
//   check( funB(x) != error , "MsgFailB" ) &&
//   check( funC(x) != error , "MsgFailC" ) ;
//   if (!success) {
//     conditionalCleanupCode();
//   }
//   return success;
// 
// When a function fails, the chain is interrupted, thanks to the
// short-circuit && operator, and execution jumps to the cleanup code.
// Meanwhile, check() set errCause to an appropriate message.
// 
// This technique has some legibility issues and is not always applicable,
// but it is quite concise, and concentrates cleanup code in a single place.
// 
// See example utilization in ImageLoad and ImageSave.
//
// (You are not required to use this in your code!)


// Check a condition and set errCause to failmsg in case of failure.
// This may be used to chain a sequence of operations and verify its success.
// Propagates the condition.
// Preserves global errno!
static int check(int condition, const char* failmsg) {
  errCause = (char*)(condition ? "" : failmsg);
  return condition;
}


/// Init Image library.  (Call once!)
/// Currently, simply calibrate instrumentation and set names of counters.
void ImageInit(void) { ///
  InstrCalibrate();
  InstrName[0] = "pixmem";  // InstrCount[0] will count pixel array acesses
  // Name other counters here...
  
}

// Macros to simplify accessing instrumentation counters:
#define PIXMEM InstrCount[0]
// Add more macros here...

// TIP: Search for PIXMEM or InstrCount to see where it is incremented!


/// Image management functions

/// Create a new black image.
///   width, height : the dimensions of the new image.
///   maxval: the maximum gray level (corresponding to white).
/// Requires: width and height must be non-negative, maxval > 0.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageCreate(int width, int height, uint8 maxval) { ///
  assert (width >= 0);
  assert (height >= 0);
  assert (0 < maxval && maxval <= PixMax);
  
  // Aloca memória para a estrutura da imagem
  Image img = (Image)malloc(sizeof(struct image));

  if (img == NULL) {
        // Falha na alocação de memória
        return NULL;
    }

  img->width = width;
  img->height = height;
  img->maxval = maxval;

  // Aloca memória para o array de pixeis
  img->pixel = (uint8 *)malloc(width * height * sizeof(uint8));
  if (img->pixel == NULL) {
    // Falha na alocação de memória para os pixeis
    free(img);
    return NULL;
  }

  // Inicializa todos os pixeis com 0 (preto)
  for (int i = 0; i < width * height; i++) {
    img->pixel[i] = 0;
  }

  return img;
}

/// Destroy the image pointed to by (*imgp).
///   imgp : address of an Image variable.
/// If (*imgp)==NULL, no operation is performed.
/// Ensures: (*imgp)==NULL.
/// Should never fail, and should preserve global errno/errCause.
void ImageDestroy(Image* imgp) { ///
  assert (imgp != NULL);
  if (imgp == NULL || *imgp == NULL) {
    // Imagem já é nula ou ponteiro é nulo, não faz nada
    return;
  }

  // Libera a memória do array de pixeis
  free((*imgp)->pixel);


  // Libera a memória da estrutura da imagem
  free(*imgp);

  // Define o ponteiro da imagem como nulo
  *imgp = NULL;
}


/// PGM file operations

// See also:
// PGM format specification: http://netpbm.sourceforge.net/doc/pgm.html

// Match and skip 0 or more comment lines in file f.
// Comments start with a # and continue until the end-of-line, inclusive.
// Returns the number of comments skipped.
static int skipComments(FILE* f) {
  char c;
  int i = 0;
  while (fscanf(f, "#%*[^\n]%c", &c) == 1 && c == '\n') {
    i++;
  }
  return i;
}

/// Load a raw PGM file.
/// Only 8 bit PGM files are accepted.
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageLoad(const char* filename) { ///
  int w, h;
  int maxval;
  char c;
  FILE* f = NULL;
  Image img = NULL;

  int success = 
  check( (f = fopen(filename, "rb")) != NULL, "Open failed" ) &&
  // Parse PGM header
  check( fscanf(f, "P%c ", &c) == 1 && c == '5' , "Invalid file format" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d ", &w) == 1 && w >= 0 , "Invalid width" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d ", &h) == 1 && h >= 0 , "Invalid height" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d", &maxval) == 1 && 0 < maxval && maxval <= (int)PixMax , "Invalid maxval" ) &&
  check( fscanf(f, "%c", &c) == 1 && isspace(c) , "Whitespace expected" ) &&
  // Allocate image
  (img = ImageCreate(w, h, (uint8)maxval)) != NULL &&
  // Read pixeis
  check( fread(img->pixel, sizeof(uint8), w*h, f) == w*h , "Reading pixeis" );
  PIXMEM += (unsigned long)(w*h);  // count pixel memory accesses

  // Cleanup
  if (!success) {
    errsave = errno;
    ImageDestroy(&img);
    errno = errsave;
  }
  if (f != NULL) fclose(f);
  return img;
}

/// Save image to PGM file.
/// On success, returns nonzero.
/// On failure, returns 0, errno/errCause are set appropriately, and
/// a partial and invalid file may be left in the system.
int ImageSave(Image img, const char* filename) { ///
  assert (img != NULL);
  int w = img->width;
  int h = img->height;
  uint8 maxval = img->maxval;
  FILE* f = NULL;

  int success =
  check( (f = fopen(filename, "wb")) != NULL, "Open failed" ) &&
  check( fprintf(f, "P5\n%d %d\n%u\n", w, h, maxval) > 0, "Writing header failed" ) &&
  check( fwrite(img->pixel, sizeof(uint8), w*h, f) == w*h, "Writing pixeis failed" ); 
  PIXMEM += (unsigned long)(w*h);  // count pixel memory accesses

  // Cleanup
  if (f != NULL) fclose(f);
  return success;
}


/// Information queries

/// These functions do not modify the image and never fail.

/// Get image width
int ImageWidth(Image img) { ///
  assert (img != NULL);
  return img->width;
}

/// Get image height
int ImageHeight(Image img) { ///
  assert (img != NULL);
  return img->height;
}

/// Get image maximum gray level
int ImageMaxval(Image img) { ///
  assert (img != NULL);
  return img->maxval;
}

/// Pixel stats
/// Find the minimum and maximum gray levels in image.
/// On return,
/// *min is set to the minimum gray level in the image,
/// *max is set to the maximum.
void ImageStats(Image img, uint8* min, uint8* max) { 
  assert (img != NULL);
  assert(min != NULL);
  assert(max != NULL);

  // Inicializa min e max com valores extremos
  *min = 0;
  *max = 255;

  // Percorre cada pixel da imagem
  for (int y = 0; y < img->height; y++) {
    for (int x = 0; x < img->width; x++) {
      // Obtem o valor do pixel atual
      uint8 pixelValue = img->pixel[y * img->width + x];

      // Atualiza o valor maximo, se necessário
      if (pixelValue > *max) { 
        *max = pixelValue;
      }

      // Atualiza o valor minimo, se necessário
      if (pixelValue < *min) {
        *min = pixelValue;
      }
    }
  }
}

/// Check if pixel position (x,y) is inside img.
int ImageValidPos(Image img, int x, int y) { ///
  assert (img != NULL);
  return (0 <= x && x < img->width) && (0 <= y && y < img->height);
}

/// Check if rectangular area (x,y,w,h) is completely inside img.
int ImageValidRect(Image img, int x, int y, int w, int h) { ///
  assert (img != NULL);

  // Verifica se as coordenadas e dimensões do retângulo são positivas
  if (x < 0 || y < 0 || w < 0 || h < 0) {
    return 0; // Retângulo inválido
  }

  // Verifica se o retângulo está dentro dos limites da imagem
  if (x + w <= img->width && y + h <= img->height) {
    return 1; // Retângulo válido
  }

  return 0; // Retângulo inválido
}

/// Pixel get & set operations

/// These are the primitive operations to access and modify a single pixel
/// in the image.
/// These are very simple, but fundamental operations, which may be used to 
/// implement more complex operations.

// Transform (x, y) coords into linear pixel index.
// This internal function is used in ImageGetPixel / ImageSetPixel. 
// The returned index must satisfy (0 <= index < img->width*img->height)
static inline int G(Image img, int x, int y) {
  int index;
  // Verifica se as coordenadas x e y estão dentro dos limites da imagem
  assert(0 <= x && x < img->width);
  assert(0 <= y && y < img->height);

  // Calcula o índice linear baseado nas coordenadas x, y
  index = y * img->width + x;

  assert (0 <= index && index < img->width*img->height);
  return index;
}

/// Get the pixel (level) at position (x,y).
uint8 ImageGetPixel(Image img, int x, int y) { ///
  assert (img != NULL);
  assert (ImageValidPos(img, x, y));
  PIXMEM += 1;  // count one pixel access (read)
  return img->pixel[G(img, x, y)];
} 

/// Set the pixel at position (x,y) to new level.
void ImageSetPixel(Image img, int x, int y, uint8 level) { ///
  assert (img != NULL);
  assert (ImageValidPos(img, x, y));
  PIXMEM += 1;  // count one pixel access (store)
  img->pixel[G(img, x, y)] = level;
} 


/// Pixel transformations

/// These functions modify the pixel levels in an image, but do not change
/// pixel positions or image geometry in any way.
/// All of these functions modify the image in-place: no allocation involved.
/// They never fail.


/// Transform image to negative image.
/// This transforms dark pixeis to light pixeis and vice-versa,
/// resulting in a "photographic negative" effect.
void ImageNegative(Image img) { ///
  assert (img != NULL && img->pixel != NULL);
  int totalpixeis = img->width * img->height;

  for (int i = 0; i < totalpixeis; i++) {
    img->pixel[i] = 255 - img->pixel[i];
  };
  

}

/// Apply threshold to image.
/// Transform all pixeis with level<thr to black (0) and
/// all pixeis with level>=thr to white (maxval).
void ImageThreshold(Image img, uint8 thr) {
  assert (img != NULL && img->pixel != NULL);

  int totalpixeis = img->width * img->height;

  for (int i = 0; i < totalpixeis; i++) {
    img->pixel[i] = (img->pixel[i] >= thr) ? 255 : 0;
  };
}

/// Brighten image by a factor.
/// Multiply each pixel level by a factor, but saturate at maxval.
/// This will brighten the image if factor>1.0 and
/// darken the image if factor<1.0.
void ImageBrighten(Image img, double factor) {
  assert (img != NULL);
  assert (factor >= 0.0);

  uint8 maxval = img->maxval;

  for (int i = 0; i < img->width * img->height; i++) {
    //Muda o brilho de cada pixel
    int new_brightness = (img->pixel[i] * factor + 0.5);
        
    // Certifica que o brilho está entre os valores supostos (maxval)
    if (new_brightness > maxval) {
      new_brightness = maxval;
    } 
    else if (new_brightness < 0) {
      new_brightness = 0;
    }
    // Aplica as alterações
    img->pixel[i] = (uint8)new_brightness;
  }
}


/// Geometric transformations

/// These functions apply geometric transformations to an image,
/// returning a new image as a result.
/// 
/// Success and failure are treated as in ImageCreate:
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.

// Implementation hint: 
// Call ImageCreate whenever you need a new image!

/// Rotate an image.
/// Returns a rotated version of the image.
/// The rotation is 90 degrees clockwise.
/// Ensures: The original img is not modified.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageRotate(Image img) { ///
  assert (img != NULL);
  // Cria uma nova imagem com as dimensões invertidas
  Image rotatedImg = ImageCreate(img->height, img->width, img->maxval);
  assert(rotatedImg != NULL);

  // Percorre cada pixel da imagem original
  for (int y = 0; y < img->height; y++) {
    for (int x = 0; x < img->width; x++) {
      // Calcula a nova posição do pixel após a rotação
      int new_x = y;
      int new_y = img->height - 1 - x;

      // Copia o pixel da imagem original para a nova posição na imagem rotacionada
      rotatedImg->pixel[new_y * rotatedImg->width + new_x] = img->pixel[y * img->width + x];
    }
  }

  return rotatedImg;
}

/// Mirror an image = flip left-right.
/// Returns a mirrored version of the image.
/// Ensures: The original img is not modified.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageMirror(Image img) { ///
  assert (img != NULL);

  // Cria uma nova imagem para o resultado espelhado
  Image mirroredImg = ImageCreate(img->width, img->height, img->maxval);
  assert(mirroredImg != NULL);

  // Percorre cada linha da imagem
  for (int y = 0; y < img->height; y++) {
    for (int x = 0; x < img->width; x++) {
      // Calcula a posição espelhada do pixel
      int mirroredX = img->width - 1 - x;
      // Copia o pixel da imagem original para a posição espelhada na nova imagem
      mirroredImg->pixel[y * img->width + mirroredX] = img->pixel[y * img->width + x];
    }
  }

  return mirroredImg;
}

/// Crop a rectangular subimage from img.
/// The rectangle is specified by the top left corner coords (x, y) and
/// width w and height h.
/// Requires:
///   The rectangle must be inside the original image.
/// Ensures:
///   The original img is not modified.
///   The returned image has width w and height h.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageCrop(Image img, int x, int y, int w, int h) { ///
  assert (img != NULL);
  //Verificar compatibilidade de dimensões
  assert (ImageValidRect(img, x, y, w, h));

  // Cria uma nova imagem para o resultado do corte
  Image croppedImg = ImageCreate(w, h, img->maxval );
  assert(croppedImg != NULL);

  // Copia os pixeis relevantes
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      // Copia o pixel da imagem original para a nova imagem
      croppedImg->pixel[j * w + i] = img->pixel[(y + j) * img->width + (x + i)];
    }
  }

    return croppedImg;  
}


/// Operations on two images

/// Paste an image into a larger image.
/// Paste img2 into position (x, y) of img1.
/// This modifies img1 in-place: no allocation involved.
/// Requires: img2 must fit inside img1 at position (x, y).
void ImagePaste(Image img1, int x, int y, Image img2) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  //Verificar compatibilidade de dimensões
  assert (ImageValidRect(img1, x, y, img2->width, img2->height));

  // Percorre cada pixel de img2
  for (int j = 0; j < img2->height; j++) {
    for (int i = 0; i < img2->width; i++) {
      // Calcula a posição correspondente em img1
      int posX = x + i;
      int posY = y + j;

      // Copia o pixel de img2 para img1
      img1->pixel[posY * img1->width + posX] = img2->pixel[j * img2->width + i];
    }
  }
}

/// Blend an image into a larger image.
/// Blend img2 into position (x, y) of img1.
/// This modifies img1 in-place: no allocation involved.
/// Requires: img2 must fit inside img1 at position (x, y).
/// alpha usually is in [0.0, 1.0], but values outside that interval
/// may provide interesting effects.  Over/underflows should saturate.
void ImageBlend(Image img1, int x, int y, Image img2, double alpha) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  assert (ImageValidRect(img1, x, y, img2->width, img2->height));

  // Percorre cada pixel de img2
  for (int j = 0; j < img2->height; j++) {
    for (int i = 0; i < img2->width; i++) {
      // Calcula a posição correspondente em img1
      int posX = x + i;
      int posY = y + j;

      // Realiza a mistura dos pixeis
      uint8 pixel1 = img1->pixel[posY * img1->width + posX];
      uint8 pixel2 = img2->pixel[j * img2->width + i];

      // Calcula o valor do pixel misturado
      uint8 blendedValue = (int)(pixel1 * (1.0 - alpha) + pixel2 * alpha + 0.5); //rounding

      // Saturação para garantir que o valor final esteja dentro dos limites (0-255)
      if (blendedValue > 255) {
        blendedValue = 255;
      } else if (blendedValue < 0) {
        blendedValue = 0;
      }

      // Atualiza o valor do pixel misturado
      img1->pixel[posY * img1->width + posX] = (uint8)blendedValue;
    }
  }  
}

/// Compare an image to a subimage of a larger image.
/// Returns 1 (true) if img2 matches subimage of img1 at pos (x, y).
/// Returns 0, otherwise.
int ImageMatchSubImage(Image img1, int x, int y, Image img2) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  assert (ImageValidPos(img1, x, y));

  // Percorre cada pixel de img2
  for (int j = 0; j < img2->height; j++) {
    for (int i = 0; i < img2->width; i++) {
      // Calcula a posição correspondente em img1
      int posX = x + i;
      int posY = y + j;

      // Compara o pixel de img2 com o pixel correspondente em img1
      if (img1->pixel[posY * img1->width + posX] != img2->pixel[j * img2->width + i]) {
        return 0; // Os pixeis não correspondem
      }
    }
  }

    return 1; // Todos os pixeis correspondem
}

/// Locate a subimage inside another image.
/// Searches for img2 inside img1.
/// If a match is found, returns 1 and matching position is set in vars (*px, *py).
/// If no match is found, returns 0 and (*px, *py) are left untouched.
int ImageLocateSubImage(Image img1, int* px, int* py, Image img2) { ///
  assert  (img1 != NULL);
  assert  (img2 != NULL);
  assert  (px != NULL);
  assert  (py != NULL);

  // Percorre a imagem img1 para encontrar a subimagem img2
  for (int y = 0; y <= img1->height - img2->height; y++) {
    for (int x = 0; x <= img1->width - img2->width; x++) {
      // Verifica se img2 corresponde a partir desta posição (x, y)
      int match = 1;
      for (int j = 0; j < img2->height && match; j++) {
        for (int i = 0; i < img2->width && match; i++) {
          if (img1->pixel[(y + j) * img1->width + (x + i)] != img2->pixel[j * img2->width + i]) {
            match = 0; // pixeis não correspondem
          }
        }
      }

      // Se encontrou uma correspondência, atualiza px e py e retorna 1
      if (match) {
        *px = x;
        *py = y;
        return 1;
      }
    }
  }
  
  return 0; // Nenhuma correspondência encontrada
}


/// Filtering

/// Blur an image by a applying a (2dx+1)x(2dy+1) mean filter.
/// Each pixel is substituted by the mean of the pixeis in the rectangle
/// [x-dx, x+dx]x[y-dy, y+dy].
/// The image is changed in-place.
void ImageBlur(Image img, int dx, int dy) { ///

  assert (img != NULL);
  assert(img->width >= 0 && img->height >= 0);

  int width = img->width;
  int height = img->height;

  //Cria uma nova imagem com as dimensões da imagem original
  Image blurredImage = ImageCreate (width,height, ImageMaxval(img));

  if (blurredImage == NULL){
    return;
  }

  //percorrer cada pixel da imagem
  for (int y = 0; y < img->height; y++){
    for (int x = 0; x < img->width; x++){

      //Inicializa duas variáveis para calcular a soma dos valores dos pixels dentro do filtro e contar quantos pixels foram somados.
      long sum = 0;
      int count = 0;

      for (int i = -dy; i <= dy; i++){
        for (int j = -dx; j <= dx; j++){

          //Calcula as coordenadas next_x e next_y dos pixels vizinhos a serem incluídos no cálculo da média.
          int next_x = x + j;
          int next_y = y + i;

          //Verifica se o pixel vizinho está dentro dos limites da imagem.
          if (ImageValidPos(img,next_x,next_y)){

            sum += ImageGetPixel (img, next_x, next_y);
            count++;

          }
        }
      }

      //Calcula o valor médio dos pixels no retângulo e aplica arredondamento ao converter para uint8.
      uint8 blurredValue = (uint8)((double)sum / count + 0.5); //rounding
      ImageSetPixel(blurredImage,x,y,blurredValue);
    }
  }

  //Depois que todos os pixels foram processados, este loop copia os pixels da imagem temporária blurredImage de volta para a imagem original
  for (int i = 0; i < height * width; i++){
    img->pixel[i] = blurredImage->pixel[i];
  }
  
  //Libera a memória alocada pela imagem temporária
  ImageDestroy(&blurredImage);
}