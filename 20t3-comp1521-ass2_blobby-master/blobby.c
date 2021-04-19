// blobby.c
// blob file archiver
// COMP1521 20T3 Assignment 2
// Written by Kristin Smith

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

// the first byte of every blobette has this value
#define BLOBETTE_MAGIC_NUMBER          0x42
// number of bytes in fixed-length blobette fields
#define BLOBETTE_MAGIC_NUMBER_BYTES    1
#define BLOBETTE_MODE_LENGTH_BYTES     3
#define BLOBETTE_PATHNAME_LENGTH_BYTES 2
#define BLOBETTE_CONTENT_LENGTH_BYTES  6
#define BLOBETTE_HASH_BYTES            1

// maximum number of bytes in variable-length blobette fields
#define BLOBETTE_MAX_PATHNAME_LENGTH   65535
#define BLOBETTE_MAX_CONTENT_LENGTH    281474976710655


// ADD YOUR #defines HERE
#define BITS_PER_BYTE 8

typedef struct blobette_components {
    uint32_t magic_number;
    uint32_t mode;
    uint32_t pathname_length;
    char *pathname;
    uint64_t content_length;
    char *content;
    uint8_t hash;

} blobette_components_t;

typedef blobette_components_t* Blobette;

typedef enum action {
    a_invalid,
    a_list,
    a_extract,
    a_create
} action_t;


void usage(char *myname);
action_t process_arguments(int argc, char *argv[], char **blob_pathname,
                           char ***pathnames, int *compress_blob);

void list_blob(char *blob_pathname);
void extract_blob(char *blob_pathname);
void create_blob(char *blob_pathname, char *pathnames[], int compress_blob);

uint8_t blobby_hash(uint8_t hash, uint8_t byte);


// ADD YOUR FUNCTION PROTOTYPES HERE
void errorHandler(char *description);
void magicNumberSet(FILE *file, Blobette blobette);
void modeSet(FILE *file, Blobette blobette);
void pathnameLengthSet(FILE *file, Blobette blobette);
void contentLengthSet(FILE *file, Blobette blobette);
void getContent(FILE *file, Blobette blobette);
void calcAndSetHash(Blobette blobette);
void readAndSetHash(FILE *file, Blobette blobette);
Blobette constructBlobette(FILE *file);
void pathnameSet(FILE *file, Blobette blobette);
void destroyBlobette(Blobette blobette);
uint8_t hashBlobette(Blobette b);
void modeWrite(FILE *file, Blobette blobette);
void pathnameLengthWrite(FILE *file, Blobette blobette);
void pathnameWrite(FILE *file, Blobette blobette);
void contentLengthWrite(FILE *file, Blobette blobette);
void contentWrite(FILE *file, Blobette blobette);
uint64_t stat_file_content_len(char *pathname);
uint32_t stat_file_mode(char *pathname);
Blobette constructBlobetteStat(FILE *file, uint32_t mode, uint64_t content_len, char *filename);


// YOU SHOULD NOT NEED TO CHANGE main, usage or process_arguments

int main(int argc, char *argv[]) {
    char *blob_pathname = NULL;
    char **pathnames = NULL;
    int compress_blob = 0;
    action_t action = process_arguments(argc, argv, &blob_pathname, &pathnames,
                                        &compress_blob);

    switch (action) {
        case a_list:
            list_blob(blob_pathname);
            break;

        case a_extract:
            extract_blob(blob_pathname);
            break;

        case a_create:
            create_blob(blob_pathname, pathnames, compress_blob);
            break;

        default:
            usage(argv[0]);
    }

    return 0;
}

// print a usage message and exit

void usage(char *myname) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\t%s -l <blob-file>\n", myname);
    fprintf(stderr, "\t%s -x <blob-file>\n", myname);
    fprintf(stderr, "\t%s [-z] -c <blob-file> pathnames [...]\n", myname);
    exit(1);
}

// process command-line arguments
// check we have a valid set of arguments
// and return appropriate action
// **blob_pathname set to pathname for blobfile
// ***pathname set to a list of pathnames for the create action
// *compress_blob set to an integer for create action

action_t process_arguments(int argc, char *argv[], char **blob_pathname,
                           char ***pathnames, int *compress_blob) {
    extern char *optarg;
    extern int optind, optopt;
    int create_blob_flag = 0;
    int extract_blob_flag = 0;
    int list_blob_flag = 0;
    int opt;
    while ((opt = getopt(argc, argv, ":l:c:x:z")) != -1) {
        switch (opt) {
            case 'c':
                create_blob_flag++;
                *blob_pathname = optarg;
                break;

            case 'x':
                extract_blob_flag++;
                *blob_pathname = optarg;
                break;

            case 'l':
                list_blob_flag++;
                *blob_pathname = optarg;
                break;

            case 'z':
                (*compress_blob)++;
                break;

            default:
                return a_invalid;
        }
    }

    if (create_blob_flag + extract_blob_flag + list_blob_flag != 1) {
        return a_invalid;
    }

    if (list_blob_flag && argv[optind] == NULL) {
        return a_list;
    } else if (extract_blob_flag && argv[optind] == NULL) {
        return a_extract;
    } else if (create_blob_flag && argv[optind] != NULL) {
        *pathnames = &argv[optind];
        return a_create;
    }

    return a_invalid;
}


// list the contents of blob_pathname

void list_blob(char *blob_pathname) {

    // Go through each blobette until you're at the end of the blob file
    FILE *blob= fopen(blob_pathname, "rb");
    if(blob == NULL) {

    }
    char c;
    while((c = fgetc(blob)) != EOF) {
        //Invalid blog check
        if(c != BLOBETTE_MAGIC_NUMBER) {
            errorHandler("ERROR: Magic byte of blobette incorrect");
        }
        //Parse blob into blobettes
        Blobette blobette = constructBlobette(blob);
        printf("%06lo %5lu %s\n", (unsigned long)blobette->mode, blobette->content_length, blobette->pathname);
        destroyBlobette(blobette);
    }

}

// extract the contents of blob_pathname

void extract_blob(char *blob_pathname) {
    // Go through each blobette until you're at the end of the blob file
    FILE *blob= fopen(blob_pathname, "rb");
    char c;
    while((c = fgetc(blob)) != EOF) {
        if(c != BLOBETTE_MAGIC_NUMBER) {
            errorHandler("ERROR: Magic byte of blobette incorrect");
        }
        Blobette blobette = constructBlobette(blob);

        //Copy pathname to null terminated array
        char *nameArray = calloc((blobette->pathname_length + 1),sizeof(char));
        strncpy(nameArray,blobette->pathname,blobette->pathname_length);
        //make file with filepath from struct
        FILE *writing_stream = fopen(nameArray, "wb");
        printf("Extracting: %s\n",nameArray);
        //chmod method taken from lecture example
                
        //check hash value matches hash field of struct
        if(blobette->hash != hashBlobette(blobette)) {
            errorHandler("ERROR: blob hash incorrect");

        }
        
        //write content field to new file
        for (long i = 0; i < blobette->content_length; i++) {
            fputc(blobette->content[i], writing_stream);
        }       

        fclose(writing_stream);
        //Modification check
        if (chmod(nameArray, blobette->mode) != 0) {
            errorHandler("Mode modification failed");
        }
        destroyBlobette(blobette);
        free(nameArray);
    }
}

// create blob_pathname from NULL-terminated array pathnames
// compress with xz if compress_blob non-zero (subset 4)

void create_blob(char *blob_pathname, char *pathnames[], int compress_blob) {

    //Determine number of files to be added to blob
    int num_files = 0;
    for (int p = 0; pathnames[p]; p++) {
        num_files++;
    }
    //Allocate memory for one blobette per file 
    Blobette *blob = malloc((sizeof(Blobette))*num_files);

    //Create a blobette for each file
    for (int p = 0; p<num_files; p++) {
        FILE *file = fopen(pathnames[p],"r");
        //Store blobette in blob array
        uint32_t mode = stat_file_mode(pathnames[p]);
        uint64_t cont_len = stat_file_content_len(pathnames[p]);
        blob[p] = constructBlobetteStat(file, mode, cont_len, pathnames[p]);
        printf("Adding: %s\n", pathnames[p]);
        fclose(file);
    }
    //create blob file
    FILE *blob_file = fopen(blob_pathname,"w");
    if (blob_file == NULL) {
        perror(blob_pathname);
        errorHandler(blob_pathname);
    }
    //traverse array of blobette structs
    for (int p = 0; p<num_files; p++) {
        //dereference each pointer and write the contents to the file bob_pathname
        //pass in stat val where required
        fputc(blob[p]->magic_number,blob_file);
        modeWrite(blob_file,blob[p]);
        pathnameLengthWrite(blob_file,blob[p]);
        contentLengthWrite(blob_file,blob[p]);
        pathnameWrite(blob_file,blob[p]);
        contentWrite(blob_file,blob[p]);
        int hash = hashBlobette(blob[p]);
        fputc(hash,blob_file);
        
    }
    for( int i = 0 ;i < num_files; i++) {
        free(blob[i]);
    }  
   free(blob);
}


// ADD YOUR FUNCTIONS HERE

//Populate the component fields of a blobette struct
Blobette constructBlobette(FILE *file) {
    Blobette blobette = malloc(sizeof(blobette_components_t));
    magicNumberSet(file,blobette);
    modeSet(file,blobette);
    pathnameLengthSet(file,blobette);
    contentLengthSet(file,blobette);
    pathnameSet(file,blobette);
    getContent(file, blobette);
    readAndSetHash(file, blobette);

    return blobette;
}

//Blobette variation for stat acquired information
Blobette constructBlobetteStat(FILE *file, uint32_t mode, uint64_t content_len, char *filename) {
    Blobette blobette = malloc(sizeof(blobette_components_t));
    magicNumberSet(file,blobette);
    blobette->mode = mode;
    blobette->pathname_length = strlen(filename);
    blobette->pathname = calloc((sizeof(char)),(blobette->pathname_length)+1);
    assert(blobette->pathname != NULL);
    strncpy(blobette->pathname, filename, blobette->pathname_length);
    blobette->content_length = content_len;    
    getContent(file, blobette);
    readAndSetHash(file, blobette);
    

    return blobette;
}

//Memory managment
void destroyBlobette(Blobette blobette) {
    free(blobette->content);
    blobette->content = NULL;
    free(blobette->pathname);
    blobette->pathname = NULL;
    free(blobette);
}

//Go through each component hashing as we go
uint8_t hashBlobette(Blobette b) {
    
    //set hash for each component byte by byte
    uint8_t hash = blobby_hash(0, b->magic_number);

    for(int i = 0; i < BLOBETTE_MODE_LENGTH_BYTES; i++) {
        hash = blobby_hash(hash, b->mode >> (BLOBETTE_MODE_LENGTH_BYTES-1-i)*BITS_PER_BYTE);
    }

    for(int i = 0; i < BLOBETTE_PATHNAME_LENGTH_BYTES; i++) {
        hash = blobby_hash(hash, b->pathname_length >> (BLOBETTE_PATHNAME_LENGTH_BYTES-1-i)*BITS_PER_BYTE);
    }

    for(int i = 0; i < BLOBETTE_CONTENT_LENGTH_BYTES; i++) {
        hash = blobby_hash(hash, b->content_length >> (BLOBETTE_CONTENT_LENGTH_BYTES-1-i)*BITS_PER_BYTE);
    }

    for(long i = 0; i < b->pathname_length; i++) {
        hash = blobby_hash(hash, b->pathname[i]);
    }

    for(long i = 0; i < b->content_length; i++) {
        hash = blobby_hash(hash, b->content[i]);
    }

    return hash;

}

/*
Constructor, getter and setter helper functions
*/

//Function modified from stat.c lecture example
//Acquire file mode via stat
uint32_t stat_file_mode(char *pathname) {
    //printf("stat(\"%s\", &s)\n", pathname);
    struct stat s;
    if (stat(pathname, &s) != 0) {
        perror(pathname);
        exit(1);
    }
    return s.st_mode;

}

//Acquire content length of file via stat
uint64_t stat_file_content_len(char *pathname) {
    struct stat s;
    if (stat(pathname, &s) != 0) {
        perror(pathname);
        exit(1);
    }
    return s.st_size;

}

//Parse file content into relevant blobette field
void getContent(FILE *file, Blobette blobette) {
    //Calloc space for name + null terminator
    blobette->content = calloc(sizeof(char),blobette->content_length+1);
    for(long i = 0; i < blobette->content_length; i++) {
        blobette->content[i] = fgetc(file);
    }
}

//Set magic number field to constant
void magicNumberSet(FILE *file, Blobette blobette) {
    blobette->magic_number = BLOBETTE_MAGIC_NUMBER;
}

//Set mode field
void modeSet(FILE *file, Blobette blobette) {
    blobette->mode = 0;
    //Shift mode into mode field
    for (int offset = BLOBETTE_MODE_LENGTH_BYTES-1; offset >= 0; offset--) {
        blobette->mode |= (fgetc(file) << (offset*8));
    }
}

//Set pathname length
void pathnameLengthSet(FILE *file, Blobette blobette) {

    blobette->pathname_length = 0;
    for (int offset = BLOBETTE_PATHNAME_LENGTH_BYTES-1; offset >= 0; offset--) {
        blobette->pathname_length |= (fgetc(file) << (offset*8));
    }

}

void pathnameSet(FILE *file, Blobette blobette) {
    //calloc extra byte for null terminator 
    blobette->pathname = calloc(sizeof(char), blobette->pathname_length+1);
    for (int i = 0; i < blobette->pathname_length; i++) {
        blobette->pathname[i] = fgetc(file);
    }
}

void contentLengthSet(FILE *file, Blobette blobette) {
    
    blobette->content_length = 0;
    for (long offset = BLOBETTE_CONTENT_LENGTH_BYTES-1; offset >= 0; offset--) {
        blobette->content_length |= ((long)(fgetc(file)) << (offset*8));
    }
}

//Allocate hash value to hash field (based on the rest of the content in blobette)
void calcAndSetHash(Blobette blobette) {
    blobette->hash = hashBlobette(blobette);
}

void readAndSetHash(FILE *file, Blobette blobette) {
    blobette->hash = fgetc(file);
}

/*
Write to disk helpers
*/

//Write blobette mode to file
void modeWrite(FILE *file, Blobette blobette) {
  
    char c = 0;
    for (int offset = BLOBETTE_MODE_LENGTH_BYTES-1; offset >= 0; offset--) {
        c = (blobette->mode >> (offset*8)) & 0xff;
        fputc(c,file);
    }
}
//Write PNL to file
void pathnameLengthWrite(FILE *file, Blobette blobette) {

    char c = 0;
    for (int offset = BLOBETTE_PATHNAME_LENGTH_BYTES-1; offset >= 0; offset--) {
        c = (blobette->pathname_length >> (offset*8)) & 0xff;
        fputc(c,file);
    }
}

//Write blobette pathname to file
void pathnameWrite(FILE *file, Blobette blobette) {

    for (int i = 0; i < blobette->pathname_length; i++) {
        fputc(blobette->pathname[i],file);
    }
}

//Write blobette length to file
void contentLengthWrite(FILE *file, Blobette blobette) {

    char c = 0;
    for (int offset = BLOBETTE_CONTENT_LENGTH_BYTES-1; offset >= 0; offset--) {
        c = (blobette->content_length >> (offset*8)) & 0xff;
        fputc(c,file);
    }
}

//Write blobette content to file
void contentWrite(FILE *file, Blobette blobette) {

    for (int i = 0; i < blobette->content_length; i++) {
        fputc(blobette->content[i],file);
    }
}

//Generic error handler, pass in desire description
void errorHandler(char *description) {
    fprintf(stderr, "%s\n", description);
    exit(1);
}
// YOU SHOULD NOT CHANGE CODE BELOW HERE

// Lookup table for a simple Pearson hash
// https://en.wikipedia.org/wiki/Pearson_hashing
// This table contains an arbitrary permutation of integers 0..255

const uint8_t blobby_hash_table[256] = {
        241, 18,  181, 164, 92,  237, 100, 216, 183, 107, 2,   12,  43,  246, 90,
        143, 251, 49,  228, 134, 215, 20,  193, 172, 140, 227, 148, 118, 57,  72,
        119, 174, 78,  14,  97,  3,   208, 252, 11,  195, 31,  28,  121, 206, 149,
        23,  83,  154, 223, 109, 89,  10,  178, 243, 42,  194, 221, 131, 212, 94,
        205, 240, 161, 7,   62,  214, 222, 219, 1,   84,  95,  58,  103, 60,  33,
        111, 188, 218, 186, 166, 146, 189, 201, 155, 68,  145, 44,  163, 69,  196,
        115, 231, 61,  157, 165, 213, 139, 112, 173, 191, 142, 88,  106, 250, 8,
        127, 26,  126, 0,   96,  52,  182, 113, 38,  242, 48,  204, 160, 15,  54,
        158, 192, 81,  125, 245, 239, 101, 17,  136, 110, 24,  53,  132, 117, 102,
        153, 226, 4,   203, 199, 16,  249, 211, 167, 55,  255, 254, 116, 122, 13,
        236, 93,  144, 86,  59,  76,  150, 162, 207, 77,  176, 32,  124, 171, 29,
        45,  30,  67,  184, 51,  22,  105, 170, 253, 180, 187, 130, 156, 98,  159,
        220, 40,  133, 135, 114, 147, 75,  73,  210, 21,  129, 39,  138, 91,  41,
        235, 47,  185, 9,   82,  64,  87,  244, 50,  74,  233, 175, 247, 120, 6,
        169, 85,  66,  104, 80,  71,  230, 152, 225, 34,  248, 198, 63,  168, 179,
        141, 137, 5,   19,  79,  232, 128, 202, 46,  70,  37,  209, 217, 123, 27,
        177, 25,  56,  65,  229, 36,  197, 234, 108, 35,  151, 238, 200, 224, 99,
        190
};

// Given the current hash value and a byte
// blobby_hash returns the new hash value
//
// Call repeatedly to hash a sequence of bytes, e.g.:
// uint8_t hash = 0;
// hash = blobby_hash(hash, byte0);
// hash = blobby_hash(hash, byte1);
// hash = blobby_hash(hash, byte2);
// ...

uint8_t blobby_hash(uint8_t hash, uint8_t byte) {
    return blobby_hash_table[hash ^ byte];
}
