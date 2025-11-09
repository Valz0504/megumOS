#ifndef _STRING_H
#define _STRING_H

#include <stdint.h>
#include <stddef.h>

/**
 * C standard memset, check man memset or
 * https://man7.org/linux/man-pages/man3/memset.3.html for more details
 * 
 * @param s Pointer to memory area to set
 * @param c Constant byte value for filling memory area
 * @param n Memory area size in byte 
 * 
 * @return Pointer s
*/
void* memset(void *s, int c, size_t n);

/**
 * C standard memcpy, check man memcpy or
 * https://man7.org/linux/man-pages/man3/memcpy.3.html for more details
 * 
 * @param dest Starting location for memory area to set
 * @param src Pointer to source memory
 * @param n Memory area size in byte 
 * 
 * @return Pointer dest
*/
void* memcpy(void* restrict dest, const void* restrict src, size_t n);

/**
 * C standard memcmp, check man memcmp or
 * https://man7.org/linux/man-pages/man3/memcmp.3.html for more details
 * 
 * @param s1 Pointer to first memory area
 * @param s2 Pointer to second memory area
 * @param n Memory area size in byte 
 * 
 * @return Integer as error code, zero for equality, non-zero for inequality
*/
int memcmp(const void *s1, const void *s2, size_t n);

/**
 * C standard memmove, check man memmove or
 * https://man7.org/linux/man-pages/man3/memmove.3.html for more details
 * 
 * @param dest Pointer to destination memory
 * @param src Pointer to source memory
 * @param n Memory area size in byte 
 * 
 * @return Pointer dest
*/
void *memmove(void *dest, const void *src, size_t n);

/**
 * Calculate the length of a string
 * 
 * @param str Pointer to null-terminated string
 * @return Length of string (excluding null terminator)
 */
size_t strlen(const char *str);

/**
 * Compare two strings
 * 
 * @param s1 Pointer to first string
 * @param s2 Pointer to second string
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strcmp(const char *s1, const char *s2);

/**
 * Compare two strings up to n characters
 * 
 * @param s1 Pointer to first string
 * @param s2 Pointer to second string
 * @param n Maximum number of characters to compare
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strncmp(const char *s1, const char *s2, size_t n);

/**
 * Copy string from source to destination
 * 
 * @param dest Pointer to destination buffer
 * @param src Pointer to source string
 * @return Pointer to destination
 */
char *strcpy(char *dest, const char *src);

/**
 * Copy up to n characters from source to destination
 * 
 * @param dest Pointer to destination buffer
 * @param src Pointer to source string
 * @param n Maximum number of characters to copy
 * @return Pointer to destination
 */
char *strncpy(char *dest, const char *src, size_t n);

/**
 * Concatenate source string to destination string
 * 
 * @param dest Pointer to destination string (must have enough space)
 * @param src Pointer to source string
 * @return Pointer to destination
 */
char *strcat(char *dest, const char *src);

/**
 * Concatenate up to n characters from source to destination
 * 
 * @param dest Pointer to destination string
 * @param src Pointer to source string
 * @param n Maximum number of characters to concatenate
 * @return Pointer to destination
 */
char *strncat(char *dest, const char *src, size_t n);

/**
 * Find first occurrence of character in string
 * 
 * @param str Pointer to string to search
 * @param c Character to find
 * @return Pointer to first occurrence, or NULL if not found
 */
char *strchr(const char *str, int c);

/**
 * Find last occurrence of character in string
 * 
 * @param str Pointer to string to search
 * @param c Character to find
 * @return Pointer to last occurrence, or NULL if not found
 */
char *strrchr(const char *str, int c);

/**
 * Find first occurrence of substring in string
 * 
 * @param haystack Pointer to string to search in
 * @param needle Pointer to substring to find
 * @return Pointer to first occurrence, or NULL if not found
 */
char *strstr(const char *haystack, const char *needle);

/**
 * Split string into tokens using delimiter
 * Note: This function modifies the input string
 * 
 * @param str Pointer to string to tokenize (NULL to continue previous tokenization)
 * @param delim Pointer to delimiter characters
 * @return Pointer to next token, or NULL if no more tokens
 */
char *strtok(char *str, const char *delim);

#endif