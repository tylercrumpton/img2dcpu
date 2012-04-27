/*
  img2dcpu - A utility for converting an image into DCPU assembly code for 0x10c.

  Copyright 2012 Tyler Crumpton.
  This utility is licensed under a GPLv3 License (see COPYING).
  Source at: https://github.com/tac0010/img2dcpu

  April 26, 2012 - v0.7: Notably reduced output code size. Not up to 1.5 spec.
  April 20, 2012 - v0.6: Added cross-platform support, fixed 32x24 animation.
  April 19, 2012 - v0.5: Added animation support to all three resolutions.
  April 17, 2012 - v0.4: Added support for 64x64 centered images.
  April 16, 2012 - v0.3: Added support for high-resolution images.
  April 15, 2012 - v0.2: Added support for full color images.
  April 13, 2012 - v0.1: Initial Release. Only works with 24-bit bitmaps.
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#ifdef __WIN32__
    #include <windows.h>
#else
    #include <unistd.h>

    typedef FILE* HANDLE;

    typedef unsigned char BYTE;
    typedef BYTE BOOLEAN;
    typedef BOOLEAN boolean;
    typedef uint32_t DWORD;
    typedef int32_t LONG;
    typedef uint16_t WORD;

    #pragma pack(push, 2)

    // from Wingdi.h
    typedef struct tagBITMAPFILEHEADER {
        WORD  bfType;
        DWORD bfSize;
        WORD  bfReserved1;
        WORD  bfReserved2;
        DWORD bfOffBits;
    } BITMAPFILEHEADER, *PBITMAPFILEHEADER;

    typedef struct tagBITMAPINFOHEADER {
        DWORD biSize;
        LONG  biWidth;
        LONG  biHeight;
        WORD  biPlanes;
        WORD  biBitCount;
        DWORD biCompression;
        DWORD biSizeImage;
        LONG  biXPelsPerMeter;
        LONG  biYPelsPerMeter;
        DWORD biClrUsed;
        DWORD biClrImportant;
    } BITMAPINFOHEADER, *PBITMAPINFOHEADER;

    typedef struct tagRGBTRIPLE {
        BYTE rgbtBlue;
        BYTE rgbtGreen;
        BYTE rgbtRed;
    } RGBTRIPLE;

    #pragma pack(pop)

#endif

using namespace std;

void readImage(char *filename);
void saveFile(char *filename);
string generateLowResTile(int index, RGBTRIPLE firstPixel, RGBTRIPLE secondPixel);
string int2hex(int i, int width);
int roundColorValue(RGBTRIPLE color);
string genFontSpace(int imageMode);
string generateDCPUFull();
string generateDCPUSmall();
string generateHighResFullTile(int pxIndex, int imgIndex);
string generateHighResSmallTile(int pxIndex, int imgIndex);

HANDLE hfile;
DWORD written;
BITMAPFILEHEADER bfh;
BITMAPINFOHEADER bih;
RGBTRIPLE *image;


enum {LOW_RES_FULL, HIGH_RES_FULL, HIGH_RES_SMALL};

const int LOW_RES_FULL_W = 32;
const int LOW_RES_FULL_H = 24;
const int HIGH_RES_FULL_W = 64;
const int HIGH_RES_FULL_H = 48;
const int HIGH_RES_SMALL_W = 64;
const int HIGH_RES_SMALL_H = 64;

bool animationFlag = false;
int imageMode;
int centerOffset = 64 + 8;

int main (int argc, char **argv) {

    //Checks to see if only one argument is supplied:
    if (argc > 3) {
        cout << "Too many arguments. Please run 'img2dcpu -help' for list of applicable arguments.";
        return -1;
    }

    //Display the help message
    if (argc == 1 || string(argv[1]) == "-help")
    {
        cout << "Converts a 24-bit bitmap image into DCPU code for 0x10c.\n\n";
        cout << "img2dcpu [imagefilename] [outputfilename]\n\n";
        cout << "imagefilename   The filename of the bitmap image that is to be converted.\n";
        cout << "outputfilename  The filename of the text file that will contain the DCPU code.\n\n";
        cout << "In order to generate an animation, you must input an image contains all frames,\n";
        cout << "in order, from left to right. Each frame must have a resolution supported by\n";
        cout << "img2dcpu. See the /examples folder for some sample images.\n\n";
        cout << "Note: img2dcpu currently only works with 32x24 color, 64x48 or 64x64 b&w images.\n";
        return 0;
    }

    if (argc == 3) { // All arguments are included

        cout << "Loading image...";
        readImage(argv[1]); //Read in the bitmap image
        cout << " Done.\n\n";

        cout << " Image Width : " << bih.biWidth << "\n";     //Will output the width of the bitmap
        cout << "Image Height : " << bih.biHeight << "\n"; //Will output the height of the bitmap

        if (bih.biWidth == 32 && bih.biHeight == 24) {
            imageMode = LOW_RES_FULL;
            animationFlag = false;
            cout << "   Mode Used : 32x24 Full Color, Full Screen.\n";
        }
        else if (bih.biWidth % 32 == 0 && bih.biHeight == 24) {
            imageMode = LOW_RES_FULL;
            animationFlag = true;
            cout << "   Mode Used : 32x24 Full Color, Full Screen, Animated.\n";
        }
        else if (bih.biWidth == 64 && bih.biHeight == 48) {
            imageMode = HIGH_RES_FULL;
            animationFlag = false;
            cout << "   Mode Used : 64x48 Black and White, Full Screen.\n";
        }
        else if (bih.biWidth % 64 == 0 && bih.biHeight == 48) {
            imageMode = HIGH_RES_FULL;
            animationFlag = true;
            cout << "   Mode Used : 64x48 Black and White, Full Screen, Animated.\n";
        }
        else if (bih.biWidth == 64 && bih.biHeight == 64) {
            imageMode = HIGH_RES_SMALL;
            animationFlag = false;
            cout << "   Mode Used : 64x64 Black and White, Centered.\n";
        }
        else if (bih.biWidth % 64 == 0 && bih.biHeight == 64) {
            imageMode = HIGH_RES_SMALL;
            animationFlag = true;
            cout << "   Mode Used : 64x64 Black and White, Centered, Animated.\n";
        }
        else {
            cout << "\nError: img2dcpu currently only supports 32x24 color or 64x48/64x64 b&w images.";
            return 2;
        }

        cout << "\nGenerating DCPU file...";
        saveFile(argv[2]);
        cout << " Done.\n";
    }

    else { // If only one argument is included, error
        cout << "\nEither image or save file was not specified; file will not be saved.\n";
        return 1;
    }

    return 0; //exit
}

//Reads in 24-bit bitmap file into "image[]" array of RGBTRIPLEs.
void readImage(char *filename) {
    #ifdef __WIN32__
        //Open the file
        hfile = CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,NULL,NULL);
        //Read the header
        ReadFile(hfile,&bfh,sizeof(bfh),&written,NULL);
        ReadFile(hfile,&bih,sizeof(bih),&written,NULL);
        //Read image
        int imagesize = bih.biWidth*bih.biHeight; //Determine the image size for memory allocation
        image = new RGBTRIPLE[imagesize]; //Create a new blank image array
        ReadFile(hfile,image,imagesize*sizeof(RGBTRIPLE),&written,NULL); //Reads it off the disk
        CloseHandle(hfile);  //Close source file
    #else
        //Open the file
        hfile = fopen(filename, "rb");
        //Read the header
        fread(&bfh, sizeof(bfh), 1, hfile);
        fread(&bih, sizeof(bih), 1, hfile);
        //Read image
        int imagesize = bih.biWidth*bih.biHeight; //Determine the image size for memory allocation
        image = new RGBTRIPLE[imagesize]; //Create a new blank image array
        fread(image, sizeof(RGBTRIPLE), imagesize, hfile); //Reads it off the disk
        fclose(hfile);  //Close source file
    #endif
}

//Generates and saves DCPU code from the "image[]" array.
void saveFile(char *filename) {

    ofstream oFile;
    oFile.open(filename); //Open file for writing (overwrites file)

    if (imageMode == HIGH_RES_SMALL) {
        oFile << generateDCPUSmall();
    }
    else {
        oFile << generateDCPUFull();
    }

    oFile.close(); //Close the file
}

string generateDCPUFull() {
    stringstream output;
    int frameWidth = 0;
    if (imageMode == LOW_RES_FULL) {
        frameWidth = LOW_RES_FULL_W;
    }
    else if (imageMode == HIGH_RES_FULL) {
        frameWidth = HIGH_RES_FULL_W;
    }

    int frames = bih.biWidth / frameWidth;

    output << "SET A, 0\n" <<
              "SET B, font_space\n" <<
              ":font_loop\n" <<
              "SET [0x8180 + A], [B]\n" <<
              "ADD A, 1\n" <<
              "ADD B, 1\n";

    if (imageMode == LOW_RES_FULL) {
        output << "IFN A, 2\n";
    }
    else if (imageMode == HIGH_RES_FULL) {
        output << "IFN A, 256\n";
    }

    output << "SET PC, font_loop\n\n" <<
              ":full_loop\n" <<
              "SET B, tile_space\n" <<
              "SET C, " << frames << "\n";


    if (animationFlag == true) {
        output << ":frame_loop\n";
    }

    output << "SET A, 0\n" <<
              ":tile_loop\n" <<
              "SET [0x8000 + A], [B]\n" <<
              "ADD A, 1\n" <<
              "ADD B, 1\n" <<
              "IFN A, 0x0180\n" <<
              "SET PC, tile_loop\n\n";

    if (animationFlag == true) {
        output << "SUB C, 1\n" <<
                  "JSR delay\n" <<
                  "IFE C, 0\n" <<
                  "SET PC, full_loop\n" <<
                  "SET PC, frame_loop\n\n" <<
                  ":delay\n" <<
                  "SET X, 0\n" <<
                  ":loop\n" <<
                  "ADD X, 1\n" <<
                  "IFN X, 1000\n" <<
                  "SET PC, loop\n" <<
                  "SET PC, POP\n";
    }

    output << "BRK\n";

    output << ":font_space DAT ";
    output << genFontSpace(imageMode); //Set up custom font

    output << "\n:tile_space DAT ";



    for (int x=0;x<frames;++x) {
        //Calculate the DCPU code for each "pixel" (tile)
        //Skip every other row, because we take 2 at a time.
        if (imageMode == LOW_RES_FULL) {
            for (int i=0; i<bih.biHeight - 1; i+=2) {
                for (int j=0; j<frameWidth; ++j) {
                    int pxIndex = (32 * (i/2)) + j; //Index of the DCPU pixel (or tile);
                    int imgIndex = (bih.biWidth * (bih.biHeight - (i+1) )) + j + (x * frameWidth); //Index of pixel in BMP
                    output << generateLowResTile(pxIndex, image[imgIndex], image[imgIndex - bih.biWidth]);
                }
            }
        }
        else if (imageMode == HIGH_RES_FULL) {
            for (int i=0; i<bih.biHeight - 3; i+=4) {
                for (int j=0; j<frameWidth - 1; j+=2) {
                    int pxIndex = (32 * (i/4)) + j/2; //Index of the DCPU pixel (or tile)
                    int imgIndex = (bih.biWidth * (bih.biHeight - (i+1) )) + j + (x * frameWidth); //Index of pixel in BMP
                    //Analyze tile:
                    output << generateHighResFullTile(pxIndex, imgIndex);
                }
            }
        }
    }

    return output.str();
}

//NON WORKING
string generateDCPUSmall() {
    stringstream output;
    int frameWidth = HIGH_RES_SMALL_W;
    int frames = bih.biWidth / frameWidth;

    output << "SET A, 0\n" <<
              "SET B, tile_space\n" <<
              ":skip_loop\n" <<
              "SET C, 0\n" <<
              "ADD A, 16\n" <<
              "IFE A, 272\n" <<
              "SET PC, full_loop\n" <<
              ":tile_loop\n" <<
              "SET [0x8038 + A], [B]\n" <<
              "ADD A, 1\n" <<
              "ADD B, 1\n" <<
              "ADD C, 1\n" <<
              "IFE C, 16\n" <<
              "SET PC, skip_loop\n" <<
              "SET PC, tile_loop\n" <<
              ":full_loop\n" <<
              "SET B, font_space\n";



    if (animationFlag == true) {
        output << "SET C, " << frames << "\n" <<
                  ":frame_loop\n";
    }

    output << "SET A, 0\n" <<
              ":font_loop\n" <<
              "SET [0x8180 + A], [B]\n" <<
              "ADD A, 1\n" <<
              "ADD B, 1\n" <<
              "IFN A, 256\n" <<
              "SET PC, font_loop\n\n";

    if (animationFlag == true) {
        output << "SUB C, 1\n" <<
                  "JSR delay\n" <<
                  "IFE C, 0\n" <<
                  "SET PC, full_loop\n" <<
                  "SET PC, frame_loop\n\n" <<
                  ":delay\n" <<
                  "SET X, 0\n" <<
                  ":loop\n" <<
                  "ADD X, 1\n" <<
                  "IFN X, 1000\n" <<
                  "SET PC, loop\n" <<
                  "SET PC, POP\n";
    }

    output << "BRK\n";

    output << ":tile_space DAT ";
    output << genFontSpace(imageMode); //Set up custom font

    output << "\n:font_space DAT ";

    for (int x=0;x<frames;++x) {
        for (int i=0; i<bih.biHeight - 7; i+=8) {
            for (int j=0; j<frameWidth - 3; j+=4) {
                int pxIndex = 2 * ((16 * (i/8)) + j/4); //Index of the DCPU pixel (or tile)
                int imgIndex = (bih.biWidth * (bih.biHeight - (i+1) )) + j + (x * frameWidth); //Index of pixel in BMP
                //Analyze tile:
                output << generateHighResSmallTile(pxIndex, imgIndex);
            }
        }
    }
    return output.str();
}

//Converts and integer to a HEX string of a certain width.
string int2hex(int i, int width) {
  stringstream stream;
  stream << hex << setfill('0') << setw(width) << i;
  return stream.str();
}

//Rounds off colors to the nearest possible value for the DCPU
int roundColorValue(RGBTRIPLE color) {
    //Array of possible colors in 0x10c in 24-bit representation:
    int possibleColors[16][3] = {{0x00,0x00,0x00}, {0x00,0x00,0xaa}, {0x00,0xaa,0x00}, {0x00,0xaa,0xaa},
                                 {0xaa,0x00,0x00}, {0xaa,0x00,0xaa}, {0xaa,0x55,0x00}, {0xaa,0xaa,0xaa},
                                 {0x55,0x55,0x55}, {0x55,0x55,0xff}, {0x55,0xff,0x55}, {0x55,0xff,0xff},
                                 {0xff,0x55,0x55}, {0xff,0x55,0xff}, {0xff,0xff,0x55}, {0xff,0xff,0xff}};

    int minRGBdiff = 765; //Set a high enough minimum to guarantee it will be overwritten
    int closestColor = 0; //Closest DCPU color to the actual color
    for (int i=0; i<16; ++i) {
        int RGBdiff = abs(color.rgbtRed - possibleColors[i][0]);
        RGBdiff += abs(color.rgbtGreen - possibleColors[i][1]);
        RGBdiff += abs(color.rgbtBlue - possibleColors[i][2]);
        //Select the color with the smallest deviation:
        if (RGBdiff < minRGBdiff) {
            minRGBdiff = RGBdiff;
            closestColor = i;
        }
    }

    return closestColor;
}

//Generates the DCPU for the custom font space, depending on the font width required
string genFontSpace(int imageMode) {
    stringstream stream;

    //Set up custom low res font space:
    if (imageMode == LOW_RES_FULL) {
        stream << "0x0f0f, 0x0f0f\n";
    }

    //Set up custom full high res font space:
    else if (imageMode == HIGH_RES_FULL) {
        int decValues[4] = {0, 3, 12, 15};
        for (int i=0; i<4; ++i) {
            for (int j=0; j<4; ++j) {
                for (int k=0; k<4; ++k) {
                    for (int l=0; l<2; ++l) {
                        string ij = int2hex(decValues[i], 1) + int2hex(decValues[j], 1);
                        string kl = int2hex(decValues[k], 1) + int2hex(decValues[l], 1);
                        stream << "0x" + ij + ij + ", ";
                        stream << "0x" + kl + kl + ", ";
                    }
                }
            }
        }
    }

    //Sets up custom highres "letter space":
    else if (imageMode == HIGH_RES_SMALL) {
        for (int i=0; i<8; ++i) {
            for (int j=0; j<16; ++j) {
                int charIndex = i*16 + j;
                stream << "0x0f" + int2hex(charIndex, 2) + ", ";
            }
        }
    }

  return stream.str();
}

//Generates DCPU code for two vertically adjacent BMP pixels (one DCMP tile).
string generateLowResTile(int index, RGBTRIPLE firstPixel, RGBTRIPLE secondPixel) {
    string output;

    //Find the closest allowable hex value for each color:
    int fRGB = roundColorValue(firstPixel);
    int sRGB = roundColorValue(secondPixel);

    output = "0x" + int2hex(fRGB, 1) + int2hex(sRGB, 1) + "00, ";

    return output;
}

//Generates the High Res Full Size tile when using the higher resolution 7-bit font width
string generateHighResFullTile(int pxIndex, int imgIndex) {
    boolean invertFlag;
    string output;
    int i, j, k, l;

    if (roundColorValue(image[(imgIndex+1) - bih.biWidth]) == 0) {
        invertFlag = false;

        if (roundColorValue(image[imgIndex+1]) == 0) {
            l = 0;
        }
        else {
            l = 1;
        }

        if (roundColorValue(image[(imgIndex+1) - 2* bih.biWidth]) == 0) {
            if (roundColorValue(image[(imgIndex+1) - 3* bih.biWidth]) == 0) {
                k = 0;
            }
            else {
                k = 2;
            }
        }
        else {
            if (roundColorValue(image[(imgIndex+1) - 3* bih.biWidth]) == 0) {
                k = 1;
            }
            else {
                k = 3;
            }
        }

        if (roundColorValue(image[imgIndex]) == 0) {
            if (roundColorValue(image[imgIndex - bih.biWidth]) == 0) {
                j = 0;
            }
            else {
                j = 2;
            }
        }
        else {
            if (roundColorValue(image[imgIndex - bih.biWidth]) == 0) {
                j = 1;
            }
            else {
                j = 3;
            }
        }

        if (roundColorValue(image[imgIndex - 2* bih.biWidth]) == 0) {
            if (roundColorValue(image[imgIndex - 3* bih.biWidth]) == 0) {
                i = 0;
            }
            else {
                i = 2;
            }
        }
        else {
            if (roundColorValue(image[imgIndex - 3* bih.biWidth]) == 0) {
                i = 1;
            }
            else {
                i = 3;
            }
        }
    }
    else {
        invertFlag = true;

        if (roundColorValue(image[imgIndex+1]) == 0) {
            l = 1;
        }
        else {
            l = 0;
        }

        if (roundColorValue(image[(imgIndex+1) - 2* bih.biWidth]) == 0) {
            if (roundColorValue(image[(imgIndex+1) - 3* bih.biWidth]) == 0) {
                k = 3;
            }
            else {
                k = 1;
            }
        }
        else {
            if (roundColorValue(image[(imgIndex+1) - 3* bih.biWidth]) == 0) {
                k = 2;
            }
            else {
                k = 0;
            }
        }

        if (roundColorValue(image[imgIndex]) == 0) {
            if (roundColorValue(image[imgIndex - bih.biWidth]) == 0) {
                j = 3;
            }
            else {
                j = 1;
            }
        }
        else {
            if (roundColorValue(image[imgIndex - bih.biWidth]) == 0) {
                j = 2;
            }
            else {
                j = 0;
            }
        }

        if (roundColorValue(image[imgIndex - 2* bih.biWidth]) == 0) {
            if (roundColorValue(image[imgIndex - 3* bih.biWidth]) == 0) {
                i = 3;
            }
            else {
                i = 1;
            }
        }
        else {
            if (roundColorValue(image[imgIndex - 3* bih.biWidth]) == 0) {
                i = 2;
            }
            else {
                i = 0;
            }
        }
    }

    //Convert IJKL character into font array index
    int character =  32*i + 8*j + 2*k + l;
    //Invert the tile is needed:
    if (invertFlag == true) {
        //return "SET [0x8" + int2hex(pxIndex, 3) + "], 0x0f" + int2hex(character, 2) + "\n";
        output = "0x0f" + int2hex(character, 2) + ", ";


    }
    else {
        //return "SET [0x8" + int2hex(pxIndex, 3) + "], 0xf0" + int2hex(character, 2) + "\n";
        output = "0xf0" + int2hex(character, 2) + ", ";
    }

    return output;
}

//Generates the High Res Small Size tile when using the higher resolution 7-bit font width
string generateHighResSmallTile(int pxIndex, int imgIndex) {

    int ijkl[4] = {0,0,0,0};
    int jVals[8] = {1, 2, 4, 8, 16, 32, 64, 128};
    for (int i=0; i<4; ++i) {
        for (int j=0; j<8; ++j) {
            if (roundColorValue(image[(imgIndex + i) - j*bih.biWidth]) == 0) {
                ijkl[i] += jVals[j];
            }
        }
    }
    return "0x" + int2hex(ijkl[0], 2) + int2hex(ijkl[1], 2) + ", " +
           "0x" + int2hex(ijkl[2], 2) + int2hex(ijkl[3], 2) + ", ";
}
