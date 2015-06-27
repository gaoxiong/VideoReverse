#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#define FFMPEG_LOG_LEVEL AV_LOG_WARNING
#define LOG_LEVEL 2
#define LOG_TAG "reverse.c"
#define LOGI(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#define LOGE(level, ...) if (level <= LOG_LEVEL + 10) {__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__);}
#define LOGW(level, ...) if (level <= LOG_LEVEL + 5) {__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__);}

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