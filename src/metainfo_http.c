#ifdef HTTP_TORRENT
#include "metainfo_http.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/ioctl.h>

#include <curl/curl.h>

#include "metainfo.h"
#include "opts.h"

struct http_metainfo;

typedef void (*progress_fn)(struct http_metainfo* mi);

struct http_metainfo {
    int64_t c_size, max_size;
    char *data;
    int err;
    struct {
        struct winsize wsize;
        /* If -1, this is a chunked transfer */
        off_t cont_len;
        time_t last_upd_ms;
        int upd_count;
        progress_fn fn;
        bool show;
        struct timespec dlstart_time;
    } progress;
};

static void progress_known(struct http_metainfo* h_meta)
{
    /* "[#####] 14%" */
    unsigned short cols;;
    struct timespec now;
    time_t nowms;
    int bars_count;
    float prcnt;

    if (h_meta == NULL) {
        /* Clear line at end */
        fprintf(stderr, "\033[1G\033[2K");
        fflush(stderr);
        return;
    }

    cols = h_meta->progress.wsize.ws_col;
    if (cols < 8)
        return;
    char line[cols + 1];

    /* 1 ms resoltion on my machine */
    clock_gettime(CLOCK_MONOTONIC_COARSE, &now);

    nowms = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    if (nowms - h_meta->progress.last_upd_ms < 500)
        return;
    h_meta->progress.last_upd_ms = nowms;
    prcnt = h_meta->c_size / (float)h_meta->progress.cont_len;
    bars_count = (cols - 8) * prcnt;

    line[0] = '[';
    memset(&line[1], '#', bars_count);
    sprintf(&line[bars_count + 1], "]% .0f%%", prcnt * 100);
    fprintf(stderr, "\033[2K\033[1G%s", line);
    fflush(stderr);
}

static void progress_unknown(struct http_metainfo* h_meta)
{
    /* "[ / ] Downloading..." */
    const char pchar[] = "/-\\|";
    struct timespec now;
    time_t nowms;
    unsigned short cols;

    if (h_meta == NULL) {
        /* Clear line at end */
        fprintf(stderr, "\033[2K\033[1G");
        fflush(stderr);
        return;
    }

    cols = h_meta->progress.wsize.ws_col;
    if (cols < 20)
        return;

    /* 1 ms resoltion on my machine */
    clock_gettime(CLOCK_MONOTONIC_COARSE, &now);

    nowms = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    if (nowms - h_meta->progress.last_upd_ms < 500)
        return;
    h_meta->progress.last_upd_ms = nowms;

    fprintf(stderr, "\033[2K\033[1G[ %c ] Downloading...",
            pchar[h_meta->progress.upd_count++ % (sizeof(pchar) - 1)]);
    fflush(stderr);
}

static size_t metainfo_read_http_headercb(char *buf, size_t size, size_t n,
        void *data) {
    struct http_metainfo *h_meta = (struct http_metainfo*)data;
    const char *cl_header = "content-length";
    char *sep = memchr(buf, ':', size * n);

    if (!sep || (sep - buf) != strlen(cl_header))
        goto end;

    if (strncmp(cl_header, buf, strlen(cl_header)) == 0) {
        char *endp;
        int64_t len;

        sep += 2;
        errno = 0;
        len = strtoll(sep, &endp, 10);
        if (sep != endp && errno == 0) {
            if (len > MAX_TORRENT_SIZE) {
                h_meta->err = EFBIG;
                return 0;
            }
            h_meta->max_size = len;
            h_meta->progress.cont_len = len;
            if (!opt_silent)
                h_meta->progress.fn = progress_known;
        }
    }
    
end:
    return size * n;
}

static int metainfo_http_progress(struct http_metainfo* h_meta) {

    struct timespec now;

    if (h_meta->progress.fn) {
        if (!h_meta->progress.show) {
            time_t diffms;

            clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
            diffms = (now.tv_sec - h_meta->progress.dlstart_time.tv_sec) * 1000 +
                (now.tv_nsec - h_meta->progress.dlstart_time.tv_nsec) / 1000000;
            if (diffms >= 1000) /* Show progress after 1 second of downloading */
                h_meta->progress.show = true;
        }

        if (h_meta->progress.show)
            h_meta->progress.fn(h_meta);
    }
    return CURL_PROGRESSFUNC_CONTINUE;
}

static int connect_cb(void* data, char*, char*, int, int) {
    struct http_metainfo* h_meta = data;
    if (!opt_silent)
        clock_gettime(CLOCK_MONOTONIC_COARSE, &h_meta->progress.dlstart_time);
    return CURL_PREREQFUNC_OK;
}

static size_t metainfo_read_http_writecb(char *ptr, size_t size, size_t n,
        void *data) {
    struct http_metainfo *h_meta = (struct http_metainfo*)data;
    size_t bytes = size * n;

    if (h_meta->max_size > MAX_TORRENT_SIZE) {
        h_meta->err = EFBIG;
        goto fail; /* Stop processing if too large */
    }

    if (h_meta->max_size == -1) {
        /* If no content-length, make a dinamic array */
        h_meta->max_size = 0;
    } else if (!h_meta->data) {
        /* We have content-size, and we haven't alloced yet */
        h_meta->data = malloc(h_meta->max_size);
    }

    size_t free_space = h_meta->max_size - h_meta->c_size;

    if (bytes > free_space) {
        while (bytes > free_space) {
            if (h_meta->max_size == 0)
                h_meta->max_size = 2048;

            h_meta->max_size *= 2;
            free_space = h_meta->max_size - h_meta->c_size;
        }

        void *n_data = realloc(h_meta->data, h_meta->max_size);
        if (!n_data) {
            h_meta->err = ENOMEM;
            goto fail;
        }

        h_meta->data = n_data;
    }

    if (h_meta->c_size + bytes > MAX_TORRENT_SIZE) {
        h_meta->err = EFBIG;
        goto fail;
    }

    memcpy(&h_meta->data[h_meta->c_size], ptr, bytes);
    h_meta->c_size += bytes;

    metainfo_http_progress(h_meta);

    return bytes;

fail:
    if (h_meta->data)
        free(h_meta->data);
    return 0;
}

/* 
 * Download the file in memory, and return the pointer to it (which needs to be
 * freed) in out_contents and the size in out_size. If the file is too big,
 * fail. Returns 0 on success and an errno on fail.
 */
int metainfo_read_http(const char* url, char** out_contents, int* out_size) {
    char errbuf[CURL_ERROR_SIZE];
    CURL *curl = curl_easy_init();
    CURLcode res;
    struct http_metainfo h_meta = {
        .c_size = 0,
        .max_size = -1,
        .data = NULL,
        .progress.cont_len = -1,
        .progress.fn = opt_silent ? NULL : progress_unknown,
    };

    if (!curl) {
        return ENOMEM;
    }

    if (!opt_silent) {
        ioctl(0, TIOCGWINSZ, &h_meta.progress.wsize);
    }

    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &h_meta);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &h_meta);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, metainfo_read_http_headercb);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, metainfo_read_http_writecb);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_PREREQFUNCTION, connect_cb);
    curl_easy_setopt(curl, CURLOPT_PREREQDATA, &h_meta);
    curl_easy_setopt(curl, CURLOPT_URL, url);

    //curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, 1024);

    res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    if (h_meta.progress.fn && h_meta.progress.show) {
        h_meta.progress.fn(NULL);
    }

    if (res) {
        fprintf(stderr, "libCurl error: %s\n", errbuf);
        return h_meta.err;
    }

    *out_contents = h_meta.data;
    *out_size = h_meta.c_size; /* caller will free */

    return 0;
}

#endif
