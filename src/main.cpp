/*
  img2dcpu - A utility for converting an image into DCPU assembly code for 0x10c.
  
  Copyright 2012 Tyler Crumpton.
  This utility is licensed under a GPLv3 License (see COPYING).
  Source at: https://github.com/tac0010/img2dcpu
  
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

HANDLE hfile;
DWORD written;
BITMAPFILEHEADER bfh;
BITMAPINFOHEADER bih;
RGBTRIPLE *image;


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
        cout << "outputfilename  The filename of the text file that will contain the DCPU code.\n";
        return 0;
    }

    cout << "Loading image...";
    readImage(argv[1]); //Read in the bitmap image
    cout << " Done.\n\n";

    cout<<"The image width is "<<bih.biWidth<<"\n";     //Will output the width of the bitmap
    cout<<"The image height is "<<bih.biHeight<<"\n\n"; //Will output the height of the bitmap

    // Display the image for verification (in black and white, based on blue values)
    // Note: It only gives you an idea of whether it loaded correctly or not;
    //       it may not be exactly the same aspect ratio.
    for (int i=bih.biHeight-1; i>=0; --i) { //Bitmap needs to be flipped, so start at the end
        for (int j=0; j<bih.biWidth; ++j) {
            if (image[bih.biWidth * i + j].rgbtBlue == 255) {
                cout << (char)(219); //Display white pixel
            }
            else {
                cout << " "; //Display black (blank) pixel
            }
        }
        cout << "\n";
    }


    if (argc == 3) { // All arguments are included
        cout << "\nGenerating DCPU file...";
        saveFile(argv[2]);
        cout << " Done.\n";
    }

    else { // If only one argument is included, error
        cout << "\nEither image or save file was not specified; file will not be saved.\n";
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
    //Set up custom 1-bit font
    oFile << "SET [0x8180], 0x0f0f\n";
    oFile << "SET [0x8181], 0x0f0f\n";
    //Calculate the DCPU code for each "pixel" (tile)
    //Skip every other row, because we take 2 at a time.
    for (int i=0; i<bih.biHeight - 1; i+=2) {
        for (int j=0; j<bih.biWidth; ++j) {
            int pxIndex = (bih.biWidth * (i/2)) + j; //Index of the DCPU pixel (or tile)
            int imgIndex = (bih.biWidth * (bih.biHeight - (i+1) )) + j; //Index of pixel in BMP
            oFile << generateDCPU(pxIndex, image[imgIndex], image[imgIndex - bih.biWidth]);
        }
    }
    oFile.close(); //Close the file
}

//Generates DCPU code for two vertically adjacent BMP pixels (one DCMP tile).
string generateDCPU(int index, RGBTRIPLE firstPixel, RGBTRIPLE secondPixel) {
    string output;
    //Get binary rgb color for first (top) pixel:
    int fR = firstPixel.rgbtRed/255;
    int fG = firstPixel.rgbtGreen/255;
    int fB = firstPixel.rgbtBlue/255;
    //Get binary rgb color for second (bottom) pixel:
    int sR = secondPixel.rgbtRed/255;
    int sG = secondPixel.rgbtGreen/255;
    int sB = secondPixel.rgbtBlue/255;

    //Calculate HEX RGB value for each pixel:
    int fRGB = 0 + 4*fR + 2*fG + fB;
    int sRGB = 0 + 4*sR + 2*sG + sB;

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


