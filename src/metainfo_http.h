#if !defined(METAINFO_HTTP_H) && defined(HTTP_TORRENT)
#define METAINFO_HTTP_H

int metainfo_read_http(const char* url, char** out_contents, int* out_size);

#endif
