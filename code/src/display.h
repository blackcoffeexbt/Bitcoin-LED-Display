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


int getScrollLength(String text) {
  int length = 0;
  for (int i = 0; i < text.length(); i++) {
    if (text[i] != '.' || (i > 0 && text[i-1] == '.')) {
      length++;
    }
  }
  return length;
}

bool isAtEnd(String text, int pos) {
  int effectiveLength = getScrollLength(text);
  return pos >= effectiveLength - 8;
}

// Global variables or part of a struct
int textPos = 0;
bool scrollForward = true;

void scrollText(String text) {
  ld.clear();
  int textLength = text.length();

  int nextCharPos = textPos;
  for (int digit = 0; digit < 8; ++digit) {
    if (nextCharPos >= textLength) {
      break; // Exit loop if end of text is reached
    }

    char ch = text[nextCharPos];
    uint8_t seg = convertCharToSegment(ch);

    // If the next character is a dot, add it to the current character
    if (nextCharPos + 1 < textLength && text[nextCharPos + 1] == '.') {
      seg |= B10000000; // Add the DP to the current segment
      nextCharPos++; // Increment to skip the dot in the next iteration
    }

    ld.write(8 - digit, seg);
    nextCharPos++; // Increment to the next character
  }

  // Update position for scrolling
  if (scrollForward) {
    if (!isAtEnd(text, textPos)) {
      textPos++; // Move forward
    } else {
      scrollForward = false; // Change direction at the end
    }
  } else {
    if (textPos > 0) {
      textPos--; // Move backward
    } else {
      scrollForward = true; // Change direction at the beginning
    }
  }
  // Pause between frames
  delay(250);
}

void writeTextCentered(String text) {
  // get text length minus dots
  int textLength = text.length();
  for (int i = 0; i < text.length(); ++i) {
    if (text[i] == '.') {
      textLength--;
    }
  }
  int pos = (8 - textLength) / 2;
  
  ld.clear();
  int nextCharPos = 0;

  // Display up to 8 characters starting from the specified position
  for (int digit = pos; digit < 8; ++digit) {
    if (nextCharPos >= text.length()) {
      break; // Exit loop if end of text is reached
    }

    char ch = text[nextCharPos];
    uint8_t seg = convertCharToSegment(ch);

    // If the next character is a dot, add it to the current character
    if (nextCharPos + 1 < text.length() && text[nextCharPos + 1] == '.') {
      seg |= B10000000; // Add the DP to the current segment
      nextCharPos++; // Increment to skip the dot in the next iteration
    }

    ld.write(8 - digit, seg);
    nextCharPos++; // Increment to the next character
  }
}

void writeText(String text, bool shouldClear = true) {
  if(shouldClear) ld.clear();
  // if text length > 8 chars, scroll, otherwise center
  if (text.length() > 8) {
    // add spaces to the beginning and end of the text to make it more readable
    text = "    " + text + "    ";
    scrollText(text);
  } else {
    writeTextCentered(text);
  }
}