#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <android/log.h>

#define FFMPEG_LOG_LEVEL AV_LOG_WARNING
#define LOG_LEVEL 2
#define LOG_TAG "reverse.c"
#define LOGI(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#define LOGE(level, ...) if (level <= LOG_LEVEL + 10) {__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__);}
#define LOGW(level, ...) if (level <= LOG_LEVEL + 5) {__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__);}

typedef struct tagBITMAPFILEHEADER {
  WORD  bfType;
  DWORD bfSize;
  WORD  bfReserved1;
  WORD  bfReserved2;
  DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
  DWORD biSize;
  LONG  biWidth;
  LONG  biHeight;
  WORD  biPlanes;
  WORD  biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG  biXPelsPerMeter;
  LONG  biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

int reverse(char *file_path_src, char *file_path_desc,
  long positionUsStart, long positionUsEnd,
  int video_stream_no, int audio_stream_no,
  int subtitle_stream_no);

int demuxing(const char *src_filename, const char *video_dst_filename, const char *audio_dst_filename);
int mux(const char *filename);

void video_decode_example(const char *outfilename, const char *filename);
void video_encode_example(const char *filename, int codec_id);
void audio_decode_example(const char *outfilename, const char *filename);
void audio_encode_example(const char *filename);

void write_video_frame(AVFormatContext *oc, AVStream *st);
AVStream *add_video_stream(AVFormatContext *oc, AVCodec **codec,
                           enum AVCodecID codec_id);
void open_video(AVFormatContext *oc, AVCodec *codec, AVStream *st);