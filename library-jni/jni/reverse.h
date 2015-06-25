#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

int reverse(const char *file_path_src, const char *file_path_desc,
  long positionUsStart, long positionUsEnd,
  int video_stream_no, int audio_stream_no,
  int subtitle_stream_no);
int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type);
int decode_packet(int *got_frame, int cached);
