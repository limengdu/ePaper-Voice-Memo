// OfrSpiffs.h -- SPIFFS file hooks for OpenFontRender.
//
// OpenFontRender's FileSupport.h maps its internal ft_fopen/ft_fread/... to the
// free functions OFR_fopen/OFR_fread/... declared below. This file provides the
// SPIFFS-backed implementations (adapted from the vendor demo's
// spiffs_perset.h). It defines globals and external-linkage functions, so it
// must be included in exactly ONE translation unit (TextRenderer.cpp) and only
// in the Chinese build. Include it AFTER OpenFontRender.h, which defines
// FT_FILE.

#ifndef VOICE_MEMO_OFR_SPIFFS_H
#define VOICE_MEMO_OFR_SPIFFS_H

#include <FS.h>
#include <SPIFFS.h>
#include <list>

// Keeps each opened fs::File alive while OpenFontRender holds a FT_FILE* to it.
std::list<fs::File> ofr_file_list;

FT_FILE *OFR_fopen(const char *filename, const char *mode) {
  fs::File f = SPIFFS.open(filename, mode);
  ofr_file_list.push_back(f);
  return &ofr_file_list.back();
}

void OFR_fclose(FT_FILE *stream) {
  reinterpret_cast<fs::File *>(stream)->close();
}

size_t OFR_fread(void *ptr, size_t size, size_t nmemb, FT_FILE *stream) {
  return reinterpret_cast<fs::File *>(stream)->read(
      reinterpret_cast<uint8_t *>(ptr), size * nmemb);
}

int OFR_fseek(FT_FILE *stream, long int offset, int whence) {
  return reinterpret_cast<fs::File *>(stream)->seek(
      offset, static_cast<fs::SeekMode>(whence));
}

long int OFR_ftell(FT_FILE *stream) {
  return reinterpret_cast<fs::File *>(stream)->position();
}

#endif  // VOICE_MEMO_OFR_SPIFFS_H
