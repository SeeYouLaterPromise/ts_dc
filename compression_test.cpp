#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <dirent.h>

#include "machete/machete.h"
#include "lfzip/lfzip.h"
#include "SZ3/sz3c.h"
#include "gorilla/gorilla.h"
#include "chimp/chimp.h"
#include "elf/elf.h"

ssize_t zlib_compress   (double* in, ssize_t len, uint8_t** out, double error);
ssize_t zlib_decompress (uint8_t* in, ssize_t len, double* out, double error);
ssize_t zstd_compress   (double* in, ssize_t len, uint8_t** out, double error);
ssize_t zstd_decompress (uint8_t* in, ssize_t len, double* out, double error);

enum ListError {
        SKIP = -2,
        EOL,
};

enum Type {Lossy, Lossless};

typedef struct {
        ssize_t ori_size;
        ssize_t cmp_size;
        ssize_t cmp_time;
        ssize_t dec_time;
} Perf;

Perf empty = {0,0,0,0};

static inline ssize_t machete_decompress_lorenzo1_hybrid(uint8_t* input , ssize_t size, double* output, double error) {
        return machete_decompress<lorenzo1,hybrid>(input, size, output);
}

static inline ssize_t SZ_compress_wrapper(double* input, ssize_t len, uint8_t** output, double error) {
        return SZ_compress(input, len, output, error);
}

static inline ssize_t SZ_decompress_wrapper(uint8_t* input , ssize_t size, double* output, double error) {
        return SZ_decompress(input, size, output, error);
}

/*********************************************************************
 *                      Evaluation Settings 
*********************************************************************/
// Available compressors

struct {
        char name[16];
        Type type;
        ssize_t (*compress) (double* input, ssize_t len, uint8_t** output, double error);
        ssize_t (*decompress) (uint8_t* input, ssize_t size, double* output, double error);
        Perf perf;
} 

compressors[] = {
        { "Machete",    Type::Lossy,    machete_compress<lorenzo1, hybrid>,     machete_decompress_lorenzo1_hybrid,     empty},
        { "LFZip",      Type::Lossy,    lfzip_compress,                         lfzip_decompress,                       empty},
        { "SZ3",        Type::Lossy,    SZ_compress_wrapper,                    SZ_decompress_wrapper,                  empty},
        { "Gorilla",    Type::Lossless, gorilla_encode,                         gorilla_decode,                         empty},
        { "Chimp",      Type::Lossless, chimp_encode,                           chimp_decode,                           empty},
        { "Elf",        Type::Lossless, elf_encode,                             elf_decode,                             empty},
        { "Zlib",       Type::Lossless, zlib_compress,                          zlib_decompress,                        empty},
        { "ZSTD",       Type::Lossless, zstd_compress,                          zstd_decompress,                        empty},
};

// Available datasets
struct {
        char name[16];
        const char* path;
        double error;
} 

datasets[] = {
        // { "GeoLife",    "./example_data/Geolife"   , 1E-4}, 
        // { "GeoLife",    "./example_data/Geolife"   , 5E-5}, 
        // { "GeoLife",    "./example_data/Geolife"   , 1E-5}, 
        // { "GeoLife",    "./example_data/Geolife"   , 5E-6}, 
        // { "GeoLife",    "./example_data/Geolife"   , 1E-6}, 
        // { "GeoLife",    "./example_data/Geolife"   , 5E-7}, 
        { "System",     "./example_data/System"   , 1E-1},
        { "System",     "./example_data/System"   , 5E-2}, 
        { "System",     "./example_data/System"   , 1E-2},
        { "System",     "./example_data/System"   , 5E-3},
        { "System",     "./example_data/System"   , 1E-3},
        // { "REDD",       "./example_data/redd"      , 5E-2}, 
        // { "REDD",       "./example_data/redd"      , 1E-2},
        // { "REDD",       "./example_data/redd"      , 5E-3}, 
        // { "REDD",       "./example_data/redd"      , 1E-3}, 
        // { "Stock",      "./example_data/stock"     , 1E-2}, 
        // { "Stock",      "./example_data/stock"     , 5E-3},
        // { "Stock",      "./example_data/stock"     , 1E-3},
        // { "Stock",      "./example_data/stock"     , 5E-4},
};

// List of compressors to be evaluated (use indices in the "compressors" array above)
int compressor_list[] = {0, 1, 2, 3, 4, 5, 6, 7, EOL};
// List of datasets to be evaluated (use indices in the "datasets" array above)
int dataset_list[] = {0, 2, 4, EOL}; 
// List of slice lengths to be evaluated
int bsize_list[] = {500, 1000, 2000, EOL};

///////////////////////// Setting End ////////////////////////////


int check(double *d_org, double *d_cmp, size_t len, double error) {
        if (error == 0) {
                for (int i = 0; i < len; i++) {
                        if (d_org[i] != d_cmp[i]) {
                                printf("%d: %.16lf(%lx) vs %.16lf(%lx)\n", i, d_org[i], ((uint64_t*)d_org)[i], d_cmp[i], ((uint64_t*)d_cmp)[i]);
                                return -1;
                        }
                }
        } else {
                for (int i = 0; i < len; i++) {
                        if (std::abs(d_org[i] - d_cmp[i]) > error) {
                                printf("%d(%lf): %lf vs %lf\n", i, d_org[i] - d_cmp[i], d_org[i], d_cmp[i]);
                                return -1;
                        }
                }
        }
        return 0;
}


/**
 * Test a single file with a specific compressor.
 * @param file The file to be tested.
 * @param c The index of the compressor in the compressors array.
 * @return If collation fails, return -1; otherwise, return 0.
 */
int test_file(FILE* file, int c, int chunk_size, double error) {
        // d_org is the original data read from the file
        // d_cmp is the compressed data
        // d_dcmp is the decompressed data
        double* d_org = (double*) malloc(chunk_size * sizeof(double));
        uint8_t *d_cmp;

        // why d_dcmp is twice the size of chunk_size?
        double *d_dcmp = (double*) malloc(2 * chunk_size * sizeof(double));
        int64_t start;

        
        // Ensure the "cmp_product" directory exists
        system("mkdir -p cmp_product");

        // Save each compressor's compressed data to a unique file
        char cmp_path[257];
        sprintf(cmp_path, "cmp_product/tmp_%s.cmp", compressors[c].name);

        FILE* fc = fopen(cmp_path, "w");
        int block = 0;
        // compress
        while(!feof(file)) {
                // len0 usually equals chunk_size, but can be less at the end of the file
                // This line reads a chunk of data from the file into d_org
                ssize_t len0 = fread(d_org, sizeof(double), chunk_size, file);
                if (len0 == 0) break;

                // real encoding happens here
                start = clock();
                ssize_t len1 = compressors[c].compress(d_org, len0, &d_cmp, error);
                compressors[c].perf.cmp_time += clock() - start;
                compressors[c].perf.cmp_size += len1;
                
                // `fwrite` writes the data that the pointer points to
                // so you should pass the pointer, the size of the elem of data
                // and the number of elements to write

                fwrite(&len1, sizeof(len1), 1, fc);
                fwrite(d_cmp, 1, len1, fc);
                free(d_cmp);
                block++;
        }
        fclose(fc);

        // rewind the file pointer to the beginning of the data file for collation
        rewind(file);
        fc = fopen(cmp_path, "r");
        block = 0;
        // decompress
        while(!feof(file)) {
                size_t len0 = fread(d_org, sizeof(double), chunk_size, file);
                if (len0 == 0) break;
                
                size_t len1;

                // (void) is C idiomatic for intetionally ignoring the return value
                // let reader know that this is not a careless ignore, but intentional.
                (void)!fread(&len1, sizeof(len1), 1, fc);
                d_cmp = (uint8_t*) malloc(len1);
                (void)!fread(d_cmp, 1, len1, fc);

                // real decoding happens here
                start = clock();
                ssize_t len2 = compressors[c].decompress(d_cmp, len1, d_dcmp, error);
                compressors[c].perf.dec_time += clock() - start;
                compressors[c].perf.ori_size += len2 * sizeof(double);

                double terror;
                switch (compressors[c].type) {
                        case Lossy: terror = error; break;
                        case Lossless: terror = 0; break;
                }

                if (len0 != len2 || check(d_org, d_dcmp, len0, terror)) {
                        // we give more specific name to the dump file
                        // so that we can identify which compressor and block caused the error
                        system("mkdir -p dump");
                        char dump_path[257];
                        sprintf(dump_path, "dump/problem_%s_%d.data", compressors[c].name, block);
                        printf("Check failed, dumping data to dump_path\n");

                        FILE* dump = fopen(dump_path, "w");
                        fwrite(d_org, sizeof(double), len0, dump);
                        fclose(dump);

                        free(d_cmp); free(d_dcmp);
                        free(d_org); fclose(fc);
                        return -1;
                }
                free(d_cmp);
                block++;
        }
        fclose(fc);

        free(d_org);
        free(d_dcmp);
        return 0;
}

void draw_progress(int now, int total, int len) {
        int count = now * len / total;
        int i;
        fprintf(stderr, "Progress: ");
        for (i = 0; i < count; i++) {
                fputc('#', stderr);
        }
        for (; i < len; i++) {
                fputc('-', stderr);
        }
        // In linux, \r moves the cursor to the beginning of the line (carriage return)
        // Different from Windows, carriage return `Enter` (like `\r\n`) does not create a new line.
        // It just moves the cursor to the beginning of the current line.
        fputc('\r', stderr);
}

int test_dataset(int ds, int chunk_size) {
        printf("**************************************\n");
        printf("      Testing on %s(%.8lf)\n", datasets[ds].name, datasets[ds].error);
        printf("**************************************\n");
        fflush(stdout); // Ensure the print message appears immediately.

        DIR* dir = opendir(datasets[ds].path);
        char filepath[257];
        struct dirent *ent;
        
        // Count the number of files in the directory
        int file_cnt = 0;
        while ((ent = readdir(dir)) != NULL) {
                // DT_REG indicates that a directory entry is a regular file.
                if (ent->d_type == DT_REG) {
                        file_cnt++;
                }
        }
        
        // readdir(dir) will move the directory pointer, so we need to rewind it.
        rewinddir(dir);
        
        int cur_file = 0;
        draw_progress(cur_file, file_cnt, 80);
        while ((ent = readdir(dir)) != NULL) {
                // prevent processing hidden files and directories ('.' and '..')
                if (ent->d_name[0] == '.') {
                        continue;
                }
                // This line creates a file path string by combining the dataset path and the file name.
                sprintf(filepath, "%s/%s", datasets[ds].path, ent->d_name);
                
                // the result is stored in `filepath` buffer for later use. (e.g., opening the file)
                FILE* file = fopen(filepath, "rb");
                for (int i = 0; compressor_list[i] != EOL; i++) {
                        if (compressor_list[i] == SKIP) {
                                continue;
                        }
                        // ensure the file pointer is at the beginning of the file
                        fseek(file, 0, SEEK_SET);

                        // In C, any non-zero (e.g. -1 -> true) value is considered true in an if condition.
                        if (test_file(file, compressor_list[i], chunk_size, datasets[ds].error)) {
                                printf("Error Occurred while testing %s, skipping\n", compressors[compressor_list[i]].name);
                                compressor_list[i] = SKIP;
                        }
                }
                fclose(file);
                cur_file++;
                draw_progress(cur_file, file_cnt, 80);
        }
        closedir(dir);
        printf("\n");
        fflush(stdout);
        return 0;
}

void report(int c) {
        printf("========= %s ==========\n", compressors[c].name);
        printf("Compression ratio: %lf\n",      
                (double)compressors[c].perf.ori_size / compressors[c].perf.cmp_size);
        printf("Compression speed: %lf MB/s\n", 
                (double)compressors[c].perf.ori_size/1024/1024 / 
                ((double)compressors[c].perf.cmp_time/CLOCKS_PER_SEC));
        printf("Decompression speed: %lf MB/s\n", 
                (double)compressors[c].perf.ori_size/1024/1024 / 
                ((double)compressors[c].perf.dec_time/CLOCKS_PER_SEC));
        printf("\n");
        fflush(stdout);
}


int main() {
        lfzip_init();

// #define DEBUG_LAST_FAILED
#ifdef DEBUG_LAST_FAILED
        // according to the dump file, debug
        FILE* fp = fopen("dump.data", "r");
        if (fp == NULL) {
                printf("Failed to open dump.data\n");
        }
        test_file(fp, 0, 1000, 1E-3);
        printf("Test finished\n");
#else 
        for (int i = 0; bsize_list[i] != EOL; i++) {
                printf("Current slice length: %d\n", bsize_list[i]);
                fflush(stdout);
                for (int j = 0; dataset_list[j] != EOL; j++) {
                        test_dataset(dataset_list[j], bsize_list[i]);
                        for (int k = 0; compressor_list[k] != EOL; k++) {
                                if (compressor_list[k] != SKIP) {
                                        report(compressor_list[k]);
                                }
                                // reset (clear) the performance structure (Perf) after each a test/report cycle.
                                __builtin_memset(&compressors[compressor_list[k]].perf, 0, sizeof(Perf));
                        }
                }
        }
        printf("Test finished\n");
#endif
        return 0;
}
