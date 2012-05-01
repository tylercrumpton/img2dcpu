/*
  img2dcpu - A utility for converting an image into DCPU assembly code for 0x10c.

  Copyright 2012 Tyler Crumpton.
  This utility is licensed under a GPLv3 License (see COPYING).
  Source at: https://github.com/tac0010/img2dcpu

  April 30, 2012 - v0.8: Updated to 1.7 spec. Added custom palette. Double buffering.
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
int roundColorToPalette(RGBTRIPLE color);
string genFontSpace(int imageMode);
string generateDCPUFull();
string generateDCPUSmall();
string generateHighResFullTile(int pxIndex, int imgIndex);
string generateHighResSmallTile(int pxIndex, int imgIndex);
void generateColorPalette();
string setupMonitor();
string genPaletteSpace();

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

int currentPalette[16][3] = {};

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
        cout << " Image Width : " << bih.biWidth << "\n";  //Will output the width of the bitmap
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
    generateColorPalette();
    output << setupMonitor();

    //Set up the DCPU custom font:
    output << "SET B, font_space\n" <<
              "SET A, 1\n" <<
              "HWI [monitor]\n\n";

    //Set up the DCPU custom color palette:
    output << "SET B, palette_space\n" <<
              "SET A, 2\n" <<
              "HWI [monitor]\n\n";

    output << "SET A, 0\n" <<
              "SET B, tile_space\n" <<
              "HWI [monitor]\n\n";

    if (animationFlag == true) {
        output << ":frame_loop\n" <<
                  "IFE B, exit\n" <<
                  "SET B, tile_space\n" <<
                  "HWI [monitor]\n" <<
                  "JSR delay\n" <<
                  "ADD B, 0x0180\n" <<
                  "SET PC, frame_loop\n\n" <<
                  ":delay\n" <<
                  "SET X, 0\n" <<
                  ":loop\n" <<
                  "ADD X, 1\n" <<
                  "IFN X, 1000\n" <<
                  "SET PC, loop\n" <<
                  "SET PC, POP\n";
    }

    if (animationFlag == false) {
        output << "BRK\n";
    }

    output << ":font_space DAT " << genFontSpace(imageMode) << "\n"; //Set up custom font
    if (imageMode == LOW_RES_FULL) {
        output << ":palette_space DAT " << genPaletteSpace();
    }
    else if (imageMode == HIGH_RES_FULL) {
        output << ":palette_space DAT " << "0x0000, 0x0FFF";
    }

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
    output << "\n:exit dat 0\n" <<
              ":monitor dat 0\n" <<
              ":not_found SET PC, 0\n";
    return output.str();
}

string setupMonitor() {
    stringstream result;
    result << "HWN Z\n" <<
              ":get_monitor\n" <<
              "IFE Z, 0\n" <<
              "SET PC, not_found\n" <<
              "SUB Z, 1\n" <<
              "HWQ Z\n" <<
              "IFN A, 0xF615\n" <<
              "SET PC, get_monitor\n" <<
              "SET [monitor], Z\n\n";
    return result.str();
}

string generateDCPUSmall() {
    stringstream output;
    int frameWidth = HIGH_RES_SMALL_W;
    int frames = bih.biWidth / frameWidth;
    output << setupMonitor();

    //Set up the DCPU custom font:
    output << "SET B, tile_space\n" <<
              "SET A, 0\n" <<
              "HWI [monitor]\n\n";

    //Set up the DCPU custom color palette:
    output << "SET B, palette_space\n" <<
              "SET A, 2\n" <<
              "HWI [monitor]\n\n";

    output << "SET A, 1\n" <<
              "SET B, font_space\n" <<
              "HWI [monitor]\n\n";

    if (animationFlag == true) {
        output << ":frame_loop\n" <<
                  "IFE B, exit\n" <<
                  "SET B, font_space\n" <<
                  "HWI [monitor]\n" <<
                  "JSR delay\n" <<
                  "ADD B, 512\n" <<
                  "SET PC, frame_loop\n\n" <<
                  ":delay\n" <<
                  "SET X, 0\n" <<
                  ":loop\n" <<
                  "ADD X, 1\n" <<
                  "IFN X, 1000\n" <<
                  "SET PC, loop\n" <<
                  "SET PC, POP\n";
    }

    if (animationFlag == false) {
        output << "BRK\n";
    }

    output << ":palette_space DAT " << "0x0000, 0x0FFF";

    output << "\n:tile_space DAT ";
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

    output << "\n:exit dat 0\n" <<
              ":monitor dat 0\n" <<
              ":not_found SET PC, 0\n";
    return output.str();
}

//Converts and integer to a HEX string of a certain width.
string int2hex(int i, int width) {
  stringstream stream;
  stream << hex << setfill('0') << setw(width) << i;
  return stream.str();
}

//Rounds off colors to the nearest possible value for the current DCPU palette
int roundColorToPalette(RGBTRIPLE color) {
    int minRGBdiff = 60000; //Set a high enough minimum to guarantee it will be overwritten
    int closestColor = 0; //Closest palette color to the actual color
    for (int i=0; i<16; ++i) {
        int RGBdiff = abs(color.rgbtRed - (currentPalette[i][0] << 4));
        RGBdiff += abs(color.rgbtGreen - (currentPalette[i][1] << 4));
        RGBdiff += abs(color.rgbtBlue - (currentPalette[i][2] << 4));
        //Select the color with the smallest deviation:
        if (RGBdiff < minRGBdiff) {
            minRGBdiff = RGBdiff;
            closestColor = i;
        }
    }

    return closestColor;
}

int roundColorValue(RGBTRIPLE color) {

    //Convert 8-bit color values to 4-bit:
    int closestR = (color.rgbtRed + 8) / 16;
    if (closestR == 16)
        closestR = 15;
    int closestG = (color.rgbtGreen + 8) / 16;
    if (closestG == 16)
        closestG = 15;
    int closestB = (color.rgbtBlue + 8) / 16;
    if (closestB == 16)
        closestB = 15;

    //Concatenate RGB values:
    return closestR * 256 + closestG * 16 + closestB;
}

//Creates a color palette that closely matches the image.
void generateColorPalette() {
    int imageSize = bih.biWidth * bih.biHeight;
    unsigned int colorCounts[4096] = {};
    unsigned int tempCounts[16] = {};
    unsigned int tempPalette[16] = {};

    //Find color frequencies:
    for (int i=0; i<imageSize; ++i) {
        ++colorCounts[roundColorValue(image[i])];
    }
    //Find 16 most common colors:
    for (int i=0; i<4096; ++i) {
        for (int j=0; j<16; ++j) {
            if (colorCounts[i] > tempCounts[j]) {
                for (int k=15; k>j; --k) {
                    tempPalette[k] = tempPalette[k-1];
                    tempCounts[k] = tempCounts[k-1];
                }
                tempPalette[j] = i;
                tempCounts[j] = colorCounts[i];
                break;
            }

        }
    }

    //Generate current palette format:
    for (int i=0; i<16; ++i) {
        currentPalette[i][0] = (tempPalette[i] & 0b111100000000) >> 8;
        currentPalette[i][1] = (tempPalette[i] & 0b000011110000) >> 4;
        currentPalette[i][2] = (tempPalette[i] & 0b000000001111);
    }
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
        for (int i=0; i<64; ++i) {
            stream << "0x0000, ";
        }
        for (int i=0; i<8; ++i) {
            for (int j=0; j<8; ++j) {
                stream << "0x0000, ";
            }
            for (int j=0; j<16; ++j) {
                int charIndex = i*16 + j;
                stream << "0x01" + int2hex(charIndex, 2) + ", ";
            }
            for (int j=0; j<8; ++j) {
                stream << "0x0000, ";
            }
        }
        for (int i=0; i<64; ++i) {
            stream << "0x0000, ";
        }
    }

  return stream.str();
}

string genPaletteSpace() {
    stringstream result;
    for (int i=0; i<16; ++i) {
        int r, g, b;
        r = currentPalette[i][0];
        g = currentPalette[i][1];
        b = currentPalette[i][2];
        result << "0x0" << int2hex(r,1) << int2hex(g,1) << int2hex(b,1) << ", ";
    }
    return result.str();
}

//Generates DCPU code for two vertically adjacent BMP pixels (one DCMP tile).
string generateLowResTile(int index, RGBTRIPLE firstPixel, RGBTRIPLE secondPixel) {
    string output;

    //Find the closest allowable hex value for each color:
    int fRGB = roundColorToPalette(firstPixel);
    int sRGB = roundColorToPalette(secondPixel);

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
        output = "0x01" + int2hex(character, 2) + ", ";
    }
    else {
        output = "0x10" + int2hex(character, 2) + ", ";
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
