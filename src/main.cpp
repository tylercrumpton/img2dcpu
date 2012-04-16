/*
  img2dcpu - A utility for converting an image into DCPU assembly code for 0x10c.

  Copyright 2012 Tyler Crumpton.
  This utility is licensed under a GPLv3 License (see COPYING).
  Source at: https://github.com/tac0010/img2dcpu

  April 16, 2012 - v0.3: Added support for high-resolution images.
  April 15, 2012 - v0.2: Added support for full color images.
  April 13, 2012 - v0.1: Initial Release. Only works with 24-bit bitmaps.
*/

#include <iostream>
#include <fstream>
#include <windows.h>
#include <sstream>
#include <iomanip>

using namespace std;

void readImage(char *filename);
void saveFile(char *filename);
string generateDCPU(int index, RGBTRIPLE firstPixel, RGBTRIPLE secondPixel);
string int2hex(int i, int width);
int roundColorValue(RGBTRIPLE color);
string genFontSpace(int fontWidth);
string generateDCPUTile(int pxIndex, int imgIndex);

HANDLE hfile;
DWORD written;
BITMAPFILEHEADER bfh;
BITMAPINFOHEADER bih;
RGBTRIPLE *image;


enum {ONE_BIT, SEVEN_BIT};
int fontWidth;

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
        cout << "Note: img2dcpu currently only works with 32x24 color or 64x48 b&w images.";
        return 0;
    }

    if (argc == 3) { // All arguments are included

        cout << "Loading image...";
        readImage(argv[1]); //Read in the bitmap image
        cout << " Done.\n\n";

        cout << "The image width is " << bih.biWidth << "\n";     //Will output the width of the bitmap
        cout << "The image height is " << bih.biHeight << "\n"; //Will output the height of the bitmap

        if (bih.biWidth == 32 && bih.biHeight == 24) {
            fontWidth = ONE_BIT;
        }
        else if (bih.biWidth == 64 && bih.biHeight == 48) {
            fontWidth = SEVEN_BIT;
        }
        else {
            cout << "\nError: img2dcpu currently only supports 32x24 color or 64x48 b&w images.";
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
}

//Generates and saves DCPU code from the "image[]" array.
void saveFile(char *filename) {
    ofstream oFile;
    oFile.open(filename); //Open file for writing (overwrites file)

    oFile << genFontSpace(fontWidth); //Set up custom font

    //Calculate the DCPU code for each "pixel" (tile)
    //Skip every other row, because we take 2 at a time.
    if (fontWidth == ONE_BIT) {
        for (int i=0; i<bih.biHeight - 1; i+=2) {
            for (int j=0; j<bih.biWidth; ++j) {
                int pxIndex = (32 * (i/2)) + j; //Index of the DCPU pixel (or tile);
                int imgIndex = (bih.biWidth * (bih.biHeight - (i+1) )) + j; //Index of pixel in BMP
                oFile << generateDCPU(pxIndex, image[imgIndex], image[imgIndex - bih.biWidth]);
            }
        }
    }
    else if (fontWidth == SEVEN_BIT) {
        for (int i=0; i<bih.biHeight - 3; i+=4) {
            for (int j=0; j<bih.biWidth - 1; j+=2) {
                int pxIndex = (32 * (i/4)) + j/2; //Index of the DCPU pixel (or tile)
                int imgIndex = (bih.biWidth * (bih.biHeight - (i+1) )) + j; //Index of pixel in BMP
                //Analyze tile:
                oFile << generateDCPUTile(pxIndex, imgIndex);

            }
        }
    }
    oFile.close(); //Close the file
}

//Generates DCPU code for two vertically adjacent BMP pixels (one DCMP tile).
string generateDCPU(int index, RGBTRIPLE firstPixel, RGBTRIPLE secondPixel) {
    string output;

    //Find the closest allowable hex value for each color:
    int fRGB = roundColorValue(firstPixel);
    int sRGB = roundColorValue(secondPixel);

    //Create DCPU code in the form of "SET [0x8AAA], 0xFS00".
    //AAA = Address offset, F = RGB of top pixel, S = RGB of bottom pixel
    output = "SET [0x8" + int2hex(index, 3) + "], 0x" + int2hex(fRGB, 1) + int2hex(sRGB, 1) + "00\n";

    return output;
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
string genFontSpace(int fontWidth) {
    stringstream stream;

    //Set up custom 1-bit font:
    if (fontWidth == ONE_BIT) {

        stream << "SET [0x8180], 0x0f0f\n";
        stream << "SET [0x8181], 0x0f0f\n";
    }

    //Set up custom 7-bit font
    else if (fontWidth == SEVEN_BIT) {
        int decValues[4] = {0, 3, 12, 15};
        int count = 0;
        for (int i=0; i<4; ++i) {
            for (int j=0; j<4; ++j) {
                for (int k=0; k<4; ++k) {
                    for (int l=0; l<2; ++l) {
                        string ij = int2hex(decValues[i], 1) + int2hex(decValues[j], 1);
                        string kl = int2hex(decValues[k], 1) + int2hex(decValues[l], 1);
                        stream << "SET [0x8" + int2hex(384 + count++, 3) + "], 0x" + ij + ij + "\n";
                        stream << "SET [0x8" + int2hex(384 + count++, 3) + "], 0x" + kl + kl + "\n";
                    }
                }
            }
        }
    }


  return stream.str();
}

//Generates the DCPU tile when using the higher resolution 7-bit font width
string generateDCPUTile(int pxIndex, int imgIndex) {
    boolean invertFlag;
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
        return "SET [0x8" + int2hex(pxIndex, 3) + "], 0x0f" + int2hex(character, 2) + "\n";
    }
    else {
        return "SET [0x8" + int2hex(pxIndex, 3) + "], 0xf0" + int2hex(character, 2) + "\n";
    }
}
