#include "DigitLedDisplay.h"

// declare global ld
extern DigitLedDisplay ld;

uint8_t convertCharToSegment(char ch) {
  // Implement a function to convert a character to its 7-segment representation
  switch (ch) {
    // numbers 0 to 9
    case '0': return B1111110; // ABCDEFG = 1111110
    case '1': return B0110000; // ABCDEFG = 0110000
    case '2': return B1101101; // ABCDEFG = 1101101
    case '3': return B1111001; // ABCDEFG = 1111001
    case '4': return B0110011; // ABCDEFG = 0110011
    case '5': return B1011011; // ABCDEFG = 1011011
    case '6': return B1011111; // ABCDEFG = 1011111
    case '7': return B1110000; // ABCDEFG = 1110000
    case '8': return B1111111; // ABCDEFG = 1111111
    case '9': return B1111011; // ABCDEFG = 1111011
    case '-': return B0000001; // ABCDEFG = 0000001

    case 'A': return B1110111;
    case 'a': return B1110111;
    case 'b': return B0011111;
    case 'B': return B01111111;
    case 'c': return B0001101;
    case 'C': return B01001110;
    case 'D': return B0111101;
    case 'd': return B0111101;
    case 'e': return B1001111;
    case 'E': return B1001111;
    case 'F': return B1000111;
    case 'f': return B1000111;
    case 'g': return B1111011;
    case 'G': return B1011110;
    case 'h': return B0010111;
    case 'H': return B0110111;
    case 'i': return B0000100;
    case 'I': return B00110000;
    case 'j': return B0111000;
    case 'J': return B0111000;
    case 'L': return B0001110;
    case 'l': return B00110000;
    case 'n': return B0010101;
    case 'N': return B01110110;
    case 'O': return B01111110;
    case 'o': return B0011101;
    case 'P': return B1100111;
    case 'p': return B1100111;
    case 'Q': return B11100111;
    case 'q': return B11100111;
    case 'r': return B0000101;
    case 'R': return B0000101;
    case 's': return B1011011;
    case 'S': return B1011011;
    case 'T': return B00001111;
    case 't': return B00001111;
    case 'u': return B0011100;
    case 'U': return B0111110;
    case 'y': return B0110011;
    case 'Y': return B0110011;
    case ' ': return B00000000; // Space
    default:  return B00000000; // Default to space for unknown characters
  }
}

void writeLetterAtPos(int pos, char ch) {
  ld.write(pos, convertCharToSegment(ch));
}


void writeText(String text) {
  ld.clear();
  // Display up to 8 characters starting from the current position
  for (int digit = 0; digit < 8; ++digit) {
    int charPos = digit;
    int textLength = text.length();
    if (charPos >= 0 && charPos < textLength) {
      char ch = text[charPos];
      uint8_t seg = convertCharToSegment(ch);
      // if the next character is a dot, add it to the current character
      if (charPos + 1 < textLength && text[charPos + 1] == '.') {
        seg |= B10000000;
      }
      ld.write(8 - digit, seg);
    }
  }
}