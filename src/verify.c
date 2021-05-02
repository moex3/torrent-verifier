#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "verify.h"
#include "sha1.h"
#include "opts.h"

#ifdef MT
#include <sys/sysinfo.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct {
    uint8_t* piece_data;
    int piece_data_size;
    int piece_index;
    const sha1sum_t* expected_result;
    int results_match;
    int done;

    sem_t sem_filled_buffer;
} verify_thread_data_t;

typedef struct {
    pthread_t thread;
    verify_thread_data_t thread_data;
} verify_thread_t;

static int mt_max_thread = 0;
static verify_thread_t* mt_threads = NULL;

/* Which thread data to fill? */
static verify_thread_data_t* mt_td_tofill = NULL;
static pthread_cond_t mt_cond_tofill;
/* Mutex for _tofill */
static pthread_mutex_t mt_mut_tofill;
/* Worker thread signals to main thread to fill it's buffer */
static sem_t mt_sem_needs_fill;
#endif

/*
 * Check if file, or directory exists
 * Return 0 if yes, or errno
 */
static int verify_file_exists(const char* path) {
    if (access(path, F_OK|R_OK) == 0)
        return 0;
    return errno;
}

/*
 * Uselessly complex function to get the file path into a stack
 * buffer if the size is enough, or allocate one and copy it there
 * heap_str needs to be freed, if it's not null
 * Returns a pointer to the path string
 */
static char* verify_get_path(fileinfo_t* finfo, const char* data_dir, \
        size_t data_dir_len, const char* torrent_name, int torrent_name_len, \
        char* stack_str, size_t stack_str_size, \
        char** heap_str, size_t* heap_str_size) {
    int path_len = metainfo_fileinfo_path(finfo, NULL);
    int req_len = path_len + data_dir_len + torrent_name_len + 1 + 1;
    char* path_ptr = stack_str;
    if (req_len > stack_str_size) {
        /* Stack is not large enough, use the heap */
        if (!(*heap_str)) {
            /* Heap is not yet allocated */
            *heap_str_size = req_len;
            path_ptr = *heap_str = malloc(*heap_str_size);
        } else if (path_len > *heap_str_size) {
            /* Heap size is not large enough, reallocate */
            *heap_str_size = req_len;
            path_ptr = *heap_str = realloc(*heap_str, *heap_str_size);
        } else {
            /* Heap is allocated, and is large enough */
            path_ptr = *heap_str;
        }
    }
    char* path_ptr_curr = path_ptr;
    memcpy(path_ptr_curr, data_dir, data_dir_len);
    path_ptr_curr += data_dir_len;
    /* WARNING: Not portable here */
    *path_ptr_curr++ = '/';

    memcpy(path_ptr_curr, torrent_name, torrent_name_len);
    path_ptr_curr += torrent_name_len;
    /* This may include multiple /'s but idc lol */
    *path_ptr_curr++ = '/';

    path_ptr_curr += metainfo_fileinfo_path(finfo, path_ptr_curr);
    *path_ptr_curr = '\0';
    return path_ptr;
}

typedef int (*fullpath_iter_cb)(const char* path, void* data);

/*
 * Call the callback function with every full path
 * in the torrent. If callbacks returns non-zero, terminate the iter
 * If append_torrent_folder is 1 and the torrent is a multi file one,
 * the torrent name will be appended after data_dir
 */
static int verify_fullpath_iter(metainfo_t* m, const char* data_dir, \
        int append_torrent_folder, fullpath_iter_cb cb, void* cb_data) {
    /* A sensible default on the stack */
    char path_buffer[512];
    /* If the above buffer is too small, malloc one */
    char* path_heap_ptr = NULL;
    size_t path_heap_size;

    const char* torrent_folder = "";
    int torrent_folder_len = 0;

    int result = 0;
    size_t data_dir_len = strlen(data_dir);
    fileinfo_t finfo;

    if (metainfo_is_multi_file(m)) {
        fileiter_t fiter;

        if (append_torrent_folder)
            metainfo_name(m, &torrent_folder, &torrent_folder_len);

        metainfo_fileiter_create(m, &fiter);
        while (result == 0 && metainfo_file_next(&fiter, &finfo) == 0) {
            char* path = verify_get_path(&finfo, data_dir, data_dir_len, \
                    torrent_folder, torrent_folder_len, path_buffer, \
                    sizeof(path_buffer), &path_heap_ptr, &path_heap_size);
            result = cb(path, cb_data);
        }
    } else {
        metainfo_fileinfo(m, &finfo);
        char* path = verify_get_path(&finfo, data_dir, data_dir_len, \
                torrent_folder, torrent_folder_len, path_buffer, \
                sizeof(path_buffer), &path_heap_ptr, &path_heap_size);
        result = cb(path, cb_data);
    }

    if (path_heap_ptr)
        free(path_heap_ptr);
    return result;
}

static int verify_is_files_exists_cb(const char* path, void* data) {
    return verify_file_exists(path);
}

/*
 * Check if the files in the torrent exists, or not
 * If append_torrent_folder is 1 and the torrent is a multi file one,
 * the torrent name will be appended after data_dir
 * Return 0 if yes, and is readable, or an errno
 */
static int verify_is_files_exists(metainfo_t* m, const char* data_dir, \
        int append_torrent_folder) {

    return verify_fullpath_iter(m, data_dir, append_torrent_folder, \
            verify_is_files_exists_cb, NULL);
}

/*
 * Read in 1 piece size amount of data
 * Returns 0 if buffer got filled, -1 if error and 1 if end of the file
 */
static int verify_read_piece(const char* path, FILE** f, int piece_size, \
        uint8_t* out_bytes, int* out_bytes_size) {
    if (!*f) {
        /* If first file, open it */
        *f = fopen(path, "rb");
        if (!*f)
            return -1;
    }

    int read;
    out_bytes += *out_bytes_size;
    while (*out_bytes_size != piece_size && (read = fread(out_bytes, \
                    1, piece_size - *out_bytes_size, *f)) > 0) {
        *out_bytes_size += read;
        out_bytes += read;
    }
    if (ferror(*f)) {
        /* If end because of an error */
        fclose(*f);
        *f = NULL;
        return -1;
    }
    if (feof(*f)) {
        /* If we reached the end of the current file */
        fclose(*f);
        *f = NULL;
        return 1;
    }
    /* If we filled the buffer */
    return 0;
}

typedef struct {
    metainfo_t* metai;
    int piece_size;
    uint8_t* piece_data;
    int piece_data_size;
#ifdef MT
    const sha1sum_t* expected_piece_hash;
#endif
    int piece_index;

    int file_count, file_index;
} verify_files_data_t;

#ifdef MT

static void verify_piece_hash_mt_cond_cleanup(void* arg) {
    pthread_mutex_unlock(&mt_mut_tofill);
}

static void* verify_piece_hash_mt(void* param) {
    verify_thread_data_t* data = (verify_thread_data_t*)param;

    for (;;) {
        /* Wait until we can put out pointer into the tofill pointer */
        pthread_mutex_lock(&mt_mut_tofill);
        while (mt_td_tofill != NULL) {
            /* If we got cancelled when waiting on cond, mutex remains
             * locked and thus, deadlock ensures */
            pthread_cleanup_push(verify_piece_hash_mt_cond_cleanup, NULL);
            pthread_cond_wait(&mt_cond_tofill, &mt_mut_tofill);
            pthread_cleanup_pop(0);
        }
        mt_td_tofill = data;
        /* Ask main to fill out buffer */
        sem_post(&mt_sem_needs_fill);
        pthread_mutex_unlock(&mt_mut_tofill);
        /* Wait for it to be filled */
        if (sem_wait(&data->sem_filled_buffer) == -1) {
            if (errno == EINTR)
                break;
        }

        /* Work on the data */
        sha1sum_t result;
        SHA1_CTX ctx;
        SHA1Init(&ctx);
        SHA1Update(&ctx, data->piece_data, data->piece_data_size);
        SHA1Final(result, &ctx);

        /* Compare the data */
        if (memcmp(result, data->expected_result, sizeof(sha1sum_t)) != 0) {
            data->results_match = 0;
        } else {
            data->results_match = 1;
        }
    }
    return 0;
}

#else

static void verify_piece_hash(uint8_t* piece_data, int piece_size, sha1sum_t result) {
    SHA1_CTX ctx;
    SHA1Init(&ctx);
    SHA1Update(&ctx, piece_data, piece_size);
    SHA1Final(result, &ctx);
}

#endif

#ifdef MT
#include <time.h>
/* MT magic oOoOOoOOoOoo */
static int verify_files_cb(const char* path, void* data) {
    verify_files_data_t* vfi = (verify_files_data_t*)data;
    FILE* f = NULL;
    int read_piece_result = 0;
    int result = 0;

    if (!opt_silent) {
        vfi->file_index++;
        printf("[%d/%d] Verifying file: %s\n", vfi->file_index, vfi->file_count, path);
    }
    for (;;) {
        /* If we don't have enough data to give to the threads, read it here */
        if (vfi->piece_data_size != vfi->piece_size) {
            read_piece_result = verify_read_piece(path, &f, vfi->piece_size, \
                    vfi->piece_data, &vfi->piece_data_size);
            if (read_piece_result == 1) {
                /* End of file, try the next one */
                break;
            } else if (read_piece_result == -1) {
                /* Something failed */
                result = -1;
                fprintf(stderr, "Reading piece: %d failed\n", vfi->piece_index);
                break;
            }
            /* Else, buffer got filled, read target piece hash and continue */
            if (metainfo_piece_index(vfi->metai, vfi->piece_index, &vfi->expected_piece_hash) == -1) {
                fprintf(stderr, "Piece meta hash reading failed at %d\n", vfi->piece_index);
                break;
            }
            /* BUT don't increment piece_index here, because it will be copied into a thread */
        }
        
        /* Wait until a thread signals us to fill it's buffer, and check result */
        sem_wait(&mt_sem_needs_fill);
        pthread_mutex_lock(&mt_mut_tofill);

        if (mt_td_tofill->piece_data_size == vfi->piece_size && \
                !mt_td_tofill->results_match) {
            /* If there was a hash at least once and vertif failed */
            fprintf(stderr, "Error at piece: %d\n", \
                    mt_td_tofill->piece_index);

            pthread_mutex_unlock(&mt_mut_tofill);
            result = -1;
            goto end;
        }

        mt_td_tofill->piece_index = vfi->piece_index;
        mt_td_tofill->piece_data_size = vfi->piece_data_size;
        mt_td_tofill->expected_result = vfi->expected_piece_hash;
        vfi->piece_index++;

        /* Reset variable so we will read next piece */
        vfi->piece_data_size = 0;

        /* Swap buffers */
        uint8_t* tmp = vfi->piece_data;
        vfi->piece_data = mt_td_tofill->piece_data;
        mt_td_tofill->piece_data = tmp;

        /* Send thread to work */
        sem_post(&mt_td_tofill->sem_filled_buffer);
        mt_td_tofill = NULL;

        /* Tell threads its okay to fill the tofill pointer */
        pthread_cond_signal(&mt_cond_tofill);
        pthread_mutex_unlock(&mt_mut_tofill);
    }

end:
    if (f)
        fclose(f);
    if (read_piece_result == -1) {
        return -1;
    }
    return result;
}

#else

static int verify_files_cb(const char* path, void* data) {
    verify_files_data_t* vfi = (verify_files_data_t*)data;
    FILE* f = NULL;
    int ver_res;

    if (!opt_silent) {
        vfi->file_index++;
        printf("[%d/%d] Verifying file: %s\n", vfi->file_index, vfi->file_count, path);
    }
    while ((ver_res = verify_read_piece(path, &f, vfi->piece_size, \
                vfi->piece_data, &vfi->piece_data_size)) == 0) {
        if (vfi->piece_size != vfi->piece_data_size) {
            fprintf(stderr, "piece_size != piece_data_size at hash\n");
        }

        sha1sum_t curr_piece_sum;
        verify_piece_hash(vfi->piece_data, vfi->piece_data_size, curr_piece_sum);

        const sha1sum_t* target_piece_sum;
        if (metainfo_piece_index(vfi->metai, vfi->piece_index, &target_piece_sum) == -1)
            goto error;

        if (memcmp(curr_piece_sum, target_piece_sum, sizeof(sha1sum_t)) != 0)
            goto error;

        vfi->piece_index++;
        vfi->piece_data_size = 0;
    }

    if (ver_res == -1)
        return -1;
    return 0;
error:
    fprintf(stderr, "Error at piece: %d\n", vfi->piece_index);
    if (f)
        fclose(f);
    return -1;
}
#endif

/*
 * Returns 0 if all files match
 */
static int verify_files(metainfo_t* m, const char* data_dir, \
        int append_torrent_folder) {
    int result = 0;

#if MT
    mt_max_thread = get_nprocs_conf();
    pthread_mutex_init(&mt_mut_tofill, 0);
    sem_init(&mt_sem_needs_fill, 0, 0);
    pthread_cond_init(&mt_cond_tofill, 0);
    mt_threads = calloc(mt_max_thread, sizeof(verify_thread_t));
    for (int i = 0; i < mt_max_thread; i++) {
        mt_threads[i].thread_data.piece_data = malloc(metainfo_piece_size(m));
        sem_init(&mt_threads[i].thread_data.sem_filled_buffer, 0, 0);
        if (pthread_create(&mt_threads[i].thread, NULL, verify_piece_hash_mt, &mt_threads[i].thread_data) != 0) {
            perror("Thread creation failed: ");
            exit(EXIT_FAILURE);
        }
    }
#endif

    verify_files_data_t data;
    data.piece_size = metainfo_piece_size(m);
    data.piece_data = malloc(data.piece_size);
    data.piece_data_size = 0;
    data.piece_index = 0;
    data.metai = m;

    if (!opt_silent) {
        data.file_count = metainfo_file_count(m);
        data.file_index = 0;
    }
    
    int vres = verify_fullpath_iter(m, data_dir, append_torrent_folder, \
            verify_files_cb, &data);
    if (vres != 0) {
        result = vres;
        goto end;
    }

    /* Here, we still have one piece left */
    sha1sum_t curr_piece_sum;
    SHA1_CTX ctx;
    SHA1Init(&ctx);
    SHA1Update(&ctx, data.piece_data, data.piece_data_size);
    SHA1Final(curr_piece_sum, &ctx);

    const sha1sum_t* target_piece_sum;
    if (metainfo_piece_index(m, data.piece_index, &target_piece_sum) == -1) {
        result = -1;
        goto end;
    }

    if (memcmp(curr_piece_sum, target_piece_sum, sizeof(sha1sum_t)) != 0) {
        result = -1;
        goto end;
    }

end:
#ifdef MT
    for (int i = 0; i < mt_max_thread; i++) {
        pthread_cancel(mt_threads[i].thread);
        pthread_join(mt_threads[i].thread, NULL);

        sem_destroy(&mt_threads[i].thread_data.sem_filled_buffer);

        free(mt_threads[i].thread_data.piece_data);
    }
    free(mt_threads);
    pthread_cond_destroy(&mt_cond_tofill);
    pthread_mutex_destroy(&mt_mut_tofill);
    sem_destroy(&mt_sem_needs_fill);
#endif
    free(data.piece_data);
    return result;
}

int verify(metainfo_t* metai, const char* data_dir, int append_folder) {
    int files_no_exists = verify_is_files_exists(metai, data_dir, append_folder);
    if (files_no_exists)
        return files_no_exists;
    
    return verify_files(metai, data_dir, append_folder);
}
