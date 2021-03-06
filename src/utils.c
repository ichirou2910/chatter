#include "utils.h"

// Generate a random string of given length
char* rand_string(size_t size) {
    char* str = (char*)malloc(size * sizeof(char) + 1);
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if (str && size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int)(sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

// Split a string STR from delimiter C
char** str_split(char* str, const char c) {
    char** result = 0;
    size_t count = 0;
    char* tmp = str;
    char* last_delim = 0;
    char delim[2];
    delim[0] = c;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp) {
        if (c == *tmp) {
            count++;
            last_delim = tmp;
        }
        tmp++;
    }
    if (count) {
        /* Add space for trailing token. */
        count += last_delim < (str + strlen(str) - 1);

        /* Add space for terminating null string so caller
           knows where the list of returned strings ends. */
        count++;

        result = malloc(sizeof(char*) * count);

        if (result) {
            size_t idx = 0;
            char* token = strtok(str, delim);

            while (token) {
                assert(idx < count);
                *(result + idx++) = strdup(token);
                token = strtok(0, delim);
            }
            assert(idx == count - 1);
            *(result + idx) = 0;
        }
    }
    else {
        result = malloc(sizeof(char*));
        result[0] = "";
    }

    return result;
}

// Truncate to the first newline
void str_trim_lf(char* arr, int length) {
    for (int i = 0; i < length; i++) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}