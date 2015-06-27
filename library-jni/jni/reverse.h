#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

int reverse(char *file_path_src, char *file_path_desc,
  long positionUsStart, long positionUsEnd,
  int video_stream_no, int audio_stream_no,
  int subtitle_stream_no);

int demuxing(const char *src_filename, const char *video_dst_filename, const char *audio_dst_filename);
int mux(const char *filename);

static void video_decode_example(const char *outfilename, const char *filename);
static void video_encode_example(const char *filename, int codec_id);
static void audio_decode_example(const char *outfilename, const char *filename);
static void audio_encode_example(const char *filename);