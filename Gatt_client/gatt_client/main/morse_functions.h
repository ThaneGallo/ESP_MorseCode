#ifndef MORSE_FUNCTIONS_H
#define MORSE_FUNCTIONS_H


/**
 * Converts decimal value of Morse code to char.
 * @param decimalValue the decimal interpretation of the morse input. Example 'a' = .- = 5.
 *  Note that there is a leading 1 on the binary input of decimalValue.
 * @return the character corresponding to the morse code decimalValue.
 */
char getLetterMorseCode(int decimalValue);


#endif