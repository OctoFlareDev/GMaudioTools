#ifdef _WIN32
#define gml extern "C" __declspec(dllexport) double
#else
#define gml extern "C" double
#endif
#define _CRT_SECURE_NO_WARNINGS

extern "C" double RegisterCallbacks(
    void* async_fn,
    void* ds_map_create,
    void* ds_map_add_double,
    void* ds_map_add_string
) {
    // store these pointers for your cross-platform code…
    return 0;
}

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// GameMaker passes extension strings as UTF-8. Windows file APIs use UTF-16
// for paths that must work independently of the active ANSI code page.
static FILE* fopen_utf8(const char* filename, const wchar_t* mode) {
    if (!filename || !mode) return NULL;

    int wide_length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        filename,
        -1,
        NULL,
        0
    );
    if (wide_length <= 0) return NULL;

    wchar_t* wide_filename = (wchar_t*)malloc((size_t)wide_length * sizeof(wchar_t));
    if (!wide_filename) return NULL;

    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            filename,
            -1,
            wide_filename,
            wide_length
        ) != wide_length) {
        free(wide_filename);
        return NULL;
    }

    FILE* file = NULL;
    errno_t open_error = _wfopen_s(&file, wide_filename, mode);
    free(wide_filename);
    return open_error == 0 ? file : NULL;
}

static int decode_vorbis_file(
    FILE* file,
    int* channels,
    int* sample_rate,
    short** output
) {
    int decoder_error = 0;
    stb_vorbis* decoder = stb_vorbis_open_file(file, 1, &decoder_error, NULL);
    if (!decoder) return -1;

    stb_vorbis_info info = stb_vorbis_get_info(decoder);
    if (info.channels <= 0 || info.channels > INT_MAX / 4096) {
        stb_vorbis_close(decoder);
        return -1;
    }

    int chunk_size = info.channels * 4096;
    int capacity = chunk_size;
    int sample_count = 0;
    int sample_offset = 0;
    short* data = (short*)malloc((size_t)capacity * sizeof(short));
    if (!data) {
        stb_vorbis_close(decoder);
        return -2;
    }

    for (;;) {
        int decoded = stb_vorbis_get_samples_short_interleaved(
            decoder,
            info.channels,
            data + sample_offset,
            capacity - sample_offset
        );
        if (decoded == 0) break;

        sample_count += decoded;
        sample_offset += decoded * info.channels;

        if (capacity - sample_offset < chunk_size) {
            if (capacity > INT_MAX / 2) {
                free(data);
                stb_vorbis_close(decoder);
                return -2;
            }

            int new_capacity = capacity * 2;
            short* expanded = (short*)realloc(
                data,
                (size_t)new_capacity * sizeof(short)
            );
            if (!expanded) {
                free(data);
                stb_vorbis_close(decoder);
                return -2;
            }

            data = expanded;
            capacity = new_capacity;
        }
    }

    *channels = info.channels;
    if (sample_rate) *sample_rate = (int)info.sample_rate;
    *output = data;
    stb_vorbis_close(decoder);
    return sample_count;
}
#endif

/**
 * Decode & resample an Ogg/Vorbis file into 16-bit interleaved stereo PCM @ 44100 Hz.
 *
 * @param filename   Path to input .ogg
 * @param out_samples_per_chan  Returns # samples per channel in the output
 * @return A malloc’d buffer of size out_samples_per_chan*2*sizeof(short), or NULL on error.
 */
short* decode_ogg_44100_stereo(const char* filename, int* out_samples_per_chan) {
    int  src_chan, src_rate;
    short* src_pcm = NULL;

    // NOTE: we capture the returned samples_per_channel,
#ifdef _WIN32
    FILE* source = fopen_utf8(filename, L"rb");
    if (!source) return NULL;
    int src_samps_per_chan = decode_vorbis_file(
        source,
        &src_chan,
        &src_rate,
        &src_pcm
    );
#else
    int src_samps_per_chan = stb_vorbis_decode_filename(
        filename,
        &src_chan,          // int*
        &src_rate,          // int*
        &src_pcm            // short**  <-- malloc’d by stb_vorbis
    );
#endif
    if (src_samps_per_chan <= 0 || !src_pcm) {
        // decode error
        free(src_pcm);
        return NULL;
    }

    // Target
    const int dst_rate = 44100;
    const int dst_chan = 2;

    // Compute resample ratio & output length
    double ratio = (double)dst_rate / (double)src_rate;
    int dst_samps = (int)floor(src_samps_per_chan * ratio);
    if (dst_samps < 1) {
        free(src_pcm);
        return NULL;
    }

    // Allocate per-channel src buffers
    short* srcL = (short*)malloc(src_samps_per_chan * sizeof(short));
    short* srcR = (short*)malloc(src_samps_per_chan * sizeof(short));
    if (!srcL || !srcR) {
        free(src_pcm); free(srcL); free(srcR);
        return NULL;
    }

    // Fill srcL/srcR
    if (src_chan == 1) {
        // Mono → duplicate
        for (int i = 0; i < src_samps_per_chan; ++i)
            srcL[i] = srcR[i] = src_pcm[i];
    } else {
        // Stereo or multi → pick/average
        for (int i = 0; i < src_samps_per_chan; ++i) {
            if (src_chan >= 2) {
                // take first two channels
                srcL[i] = src_pcm[i*src_chan + 0];
                srcR[i] = src_pcm[i*src_chan + 1];
            } else {
                // fallback (shouldn't happen)
                srcL[i] = srcR[i] = src_pcm[i*src_chan];
            }
        }
    }
    free(src_pcm);

    // Allocate dest buffers per channel
    short* dstL = (short*)malloc(dst_samps * sizeof(short));
    short* dstR = (short*)malloc(dst_samps * sizeof(short));
    if (!dstL || !dstR) {
        free(srcL); free(srcR); free(dstL); free(dstR);
        return NULL;
    }

    // Linear resampling per channel
    for (int ch = 0; ch < dst_chan; ++ch) {
        short* S = (ch==0 ? srcL : srcR);
        short* D = (ch==0 ? dstL : dstR);
        for (int i = 0; i < dst_samps; ++i) {
            double pos = i / ratio;
            int idx = (int)floor(pos);
            double frac = pos - idx;
            if (idx >= src_samps_per_chan - 1) {
                D[i] = S[src_samps_per_chan - 1];
            } else {
                short s0 = S[idx];
                short s1 = S[idx + 1];
                D[i] = (short)lround(s0*(1-frac) + s1*frac);
            }
        }
    }
    free(srcL);
    free(srcR);

    // Interleave into single buffer
    short* out = (short*)malloc((size_t)dst_samps * dst_chan * sizeof(short));
    if (!out) { free(dstL); free(dstR); return NULL; }
    for (int i = 0; i < dst_samps; ++i) {
        out[2*i + 0] = dstL[i];
        out[2*i + 1] = dstR[i];
    }
    free(dstL);
    free(dstR);

    *out_samples_per_chan = dst_samps;
    return out;
}

gml audio_file_decode_ogg(const char* filename, const char* dest) {
    int samples;
    short* pcm = decode_ogg_44100_stereo(filename, &samples);
    if (!pcm) {
        return -1;  // decode failed
    }
#ifdef _WIN32
    FILE* f = fopen_utf8(dest, L"wb");
#else
    FILE* f = fopen(dest, "wb");
#endif
    if (!f) {
        free(pcm);
        return -2;  // file open error
    }
    size_t want = (size_t)samples * 2;
    if (fwrite(pcm, sizeof(short), want, f) != want) {
        fclose(f);
        free(pcm);
        return -3;  // write error
    }
    fclose(f);
    free(pcm);
    return samples;  // success, return #samples per channel
}
