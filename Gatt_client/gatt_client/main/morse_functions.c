// #include <stdio.h> // we only need this one if we decide to add print statements or similar in here.
#include "morse_functions.h"

char getLetterMorseCode(int decimalValue)
{
    /*
    decimalValue has a leading 1 to determine the start of the morse input.
        for example, A: .- , would directly translate to just 01, but to remove
        issues of .- being different from ..-, we have added in a leading 1.
    Hence, A: .- = 101 = 5.
    */
    switch (decimalValue)
    {
    case 5:
        // Handle case for A: .-
        return 'a';
    case 24:
        // Handle case for B: -..
        return 'b';
    case 26:
        // Handle case for C: -.-.
        return 'c';
    case 12:
        // Handle case for D: -..
        return 'd';
    case 2:
        // Handle case for E: .
        return 'e';
    case 18:
        // Handle case for F: ..-.
        return 'f';
    case 14:
        // Handle case for G: --.
        return 'g';
    case 16:
        // Handle case for H: ....
        return 'h';
    case 4:
        // Handle case for I: ..
        return 'i';
    case 23:
        // Handle case for J: .---
        return 'j';
    case 13:
        // Handle case for K: -.-
        return 'k';
    case 20:
        // Handle case for L: .-..
        return 'l';
    case 7:
        // Handle case for M: --
        return 'm';
    case 6:
        // Handle case for N: -.
        return 'n';
    case 15:
        // Handle case for O: ---
        return 'o';
    case 22:
        // Handle case for P: .--.
        return 'p';
    case 29:
        // Handle case for Q: --.-
        return 'q';
    case 10:
        // Handle case for R: .-.
        return 'r';
    case 8:
        // Handle case for S: ...
        return 's';
    case 3:
        // Handle case for T: -
        return 't';
    case 9:
        // Handle case for U: ..-
        return 'u';
    case 17:
        // Handle case for V: ...-
        return 'v';
    case 11:
        // Handle case for W: .--
        return 'w';
    case 25:
        // Handle case for X: -..-
        return 'x';
    case 27:
        // Handle case for Y: -.--
        return 'y';
    case 28:
        // Handle case for Z: --..
        return 'z';
    case 63:
        // Handle case for 0: -----
        return '0';
    case 47:
        // Handle case for 1: .----
        return '1';
    case 39:
        // Handle case for 2: ..---
        return '2';
    case 35:
        // Handle case for 3: ...--
        return '3';
    case 33:
        // Handle case for 4: ....-
        return '4';
    case 64:
        // Handle case for 5: .....
        return '5';
    case 48:
        // Handle case for 6: -....
        return '6';
    case 56:
        // Handle case for 7: --...
        return '7';
    case 60:
        // Handle case for 8: ---..
        return '8';
    case 61:
        // Handle case for 9: ----.
        return '9';
    default:
        // Handle unknown cases
        return '=';
    }
}