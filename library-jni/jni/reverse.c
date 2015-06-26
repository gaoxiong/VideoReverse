#include "reverse.h"

#include <android/log.h>
#include <libavutil/timestamp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define FFMPEG_LOG_LEVEL AV_LOG_WARNING
#define LOG_LEVEL 2
#define LOG_TAG "reverse.c"
#define LOGI(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#define LOGE(level, ...) if (level <= LOG_LEVEL + 10) {__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__);}
#define LOGW(level, ...) if (level <= LOG_LEVEL + 5) {__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__);}

int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type);
int decode_packet(int *got_frame, AVFrame*, int cached, int);

AVFormatContext *fmt_ctx = NULL;
AVFormatContext *fmt_ctx_o = NULL;
AVOutputFormat *fmt_o;
AVStream *video_o;
AVCodecContext *pCodecCtx_o;
AVCodec *pCodec_o;
uint8_t *picture_buf_o;
AVFrame *picture_o;

AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
AVStream *video_stream = NULL, *audio_stream = NULL;
AVPacket pkt;
int video_frame_count = 0;
int audio_frame_count = 0;
FILE *video_dst_file = NULL;
FILE *audio_dst_file = NULL;
int video_stream_idx = -1, audio_stream_idx = -1;
uint8_t *video_dst_data[4] = {NULL};
int      video_dst_linesize[4];
int      video_dst_bufsize;

uint8_t **audio_dst_data = NULL;
int       audio_dst_linesize;
int       audio_dst_bufsize;

int ret = 0, got_frame;

int reverse(char *file_path_src, char *file_path_desc,
  long positionUsStart, long positionUsEnd,
  int video_stream_no, int audio_stream_no,
  int subtitle_stream_no) {
  
  char *src_filename = NULL;
  char *video_dst_filename = NULL;
  char *audio_dst_filename = NULL;

  src_filename = file_path_src;
  video_dst_filename = file_path_desc;
  //audio_dst_filename = argv[3];

  /* register all formats and codecs */
  av_register_all();

  /* open input file, and allocated format context */
  LOGI(LOG_LEVEL, "source file %s\n", src_filename);
  LOGI(LOG_LEVEL, "destination file %s\n", video_dst_filename);
  if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
      LOGI(LOG_LEVEL, "Could not open source file %s\n", src_filename);
      exit(1);
  }

  /* retrieve stream information */
  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
      LOGI(LOG_LEVEL, "Could not find stream information\n");
      exit(1);
  }

  if (video_stream_no > 0 &&
      open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
      video_stream = fmt_ctx->streams[video_stream_idx];
      video_dec_ctx = video_stream->codec;
#if 1
      video_dst_file = fopen(video_dst_filename, "wb");
      if (!video_dst_file) {
          LOGI(LOG_LEVEL, "Could not open video destination file %s(%d)\n", video_dst_filename, errno);
          ret = 1;
          goto end;
      }

      /* allocate image where the decoded image will be put */
      ret = av_image_alloc(video_dst_data, video_dst_linesize,
                           video_dec_ctx->width, video_dec_ctx->height,
                           video_dec_ctx->pix_fmt, 1);
      if (ret < 0) {
          LOGI(LOG_LEVEL, "Could not allocate raw video buffer\n");
          goto end;
      }
      video_dst_bufsize = ret;
#else
      LOGI(LOG_LEVEL, "deduce output format from file extension\n");
      avformat_alloc_output_context2(&fmt_ctx_o, NULL, NULL, video_dst_filename);
      if (!fmt_ctx_o) {
        LOGI(LOG_LEVEL, "Could not deduce output format from file extension: using default\n");
        avformat_alloc_output_context2(&fmt_ctx_o, NULL, "mpeg", video_dst_filename);
      }
      if (!fmt_ctx_o) {
        LOGI(LOG_LEVEL, "Could not deduce output format from file extension: using MPEG\n");
        goto end;
      }
      /*
      fmt_o = fmt_ctx_o->oformat;
      if (avio_open(&fmt_ctx_o->pb, 
                    video_dst_filename, 
                    AVIO_FLAG_READ_WRITE) < 0) {
        LOGI(LOG_LEVEL, "Could not open video destination file %s(%d)\n", video_dst_filename, errno);
        goto end;
      }
      */
#endif
  }

  /* dump input information to stderr */
  av_dump_format(fmt_ctx, 0, src_filename, 0);
#if 0
  if (audio_stream_no > 0 &&
      open_codec_context(&audio_stream_idx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
      int nb_planes;

      audio_stream = fmt_ctx->streams[audio_stream_idx];
      audio_dec_ctx = audio_stream->codec;
      audio_dst_file = fopen(audio_dst_filename, "wb");
      if (!audio_dst_file) {
          LOGI(LOG_LEVEL, "Could not open audio destination file %s(%d)\n", video_dst_filename, errno);
          ret = 1;
          goto end;
      }

      nb_planes = av_sample_fmt_is_planar(audio_dec_ctx->sample_fmt) ?
          audio_dec_ctx->channels : 1;
      audio_dst_data = av_mallocz(sizeof(uint8_t *) * nb_planes);
      if (!audio_dst_data) {
          LOGI(LOG_LEVEL, "Could not allocate audio data buffers\n");
          ret = AVERROR(ENOMEM);
          goto end;
      }
  }
#endif
  if (!audio_stream && !video_stream) {
      LOGI(LOG_LEVEL, "Could not find audio or video stream in the input, aborting\n");
      ret = 1;
      goto end;
  }

  AVFrame *frame = avcodec_alloc_frame();
  if (!frame) {
      LOGI(LOG_LEVEL, "Could not allocate frame\n");
      ret = AVERROR(ENOMEM);
      goto end;
  }

  /* initialize packet, set data to NULL, let the demuxer fill it */
  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;
  // write output file header
  //avformat_write_header(fmt_ctx_o, NULL);

  if (video_stream)
      LOGI(LOG_LEVEL, "Demuxing video from file '%s' into '%s'\n", src_filename, video_dst_filename);
  if (audio_stream)
      LOGI(LOG_LEVEL, "Demuxing video from file '%s' into '%s'\n", src_filename, audio_dst_filename);

  /* read frames from the file */
  int frameCount = 0;
  while (av_read_frame(fmt_ctx, &pkt) >= 0) {
      // decode pkt to frame
      decode_packet(&got_frame, frame, 0, frameCount++);
  }

  /* flush cached frames */
  pkt.data = NULL;
  pkt.size = 0;
#if 0
  do {
      decode_packet(&got_frame, frame, 1);
  } while (got_frame);
#endif
  LOGI(LOG_LEVEL, "Demuxing succeeded.\n");

#if 0
  if (video_stream) {
      LOGI(LOG_LEVEL, "Play the output video file with the command:\n"
             "ffplay -f rawvideo -pix_fmt %s -video_size %d*%d %s\n",
             av_get_pix_fmt_name(video_dec_ctx->pix_fmt),
             video_dec_ctx->width, video_dec_ctx->height,
             video_dst_filename);
  }
  if (audio_stream) {
      const char *fmt;

      if ((ret = get_format_from_sample_fmt(&fmt, audio_dec_ctx->sample_fmt) < 0))
          goto end;
      LOGI(LOG_LEVEL, "Play the output audio file with the command:\n"
             "ffplay -f %s -ac %d -ar %d %s\n",
             fmt, audio_dec_ctx->channels, audio_dec_ctx->sample_rate,
             audio_dst_filename);
  }
#endif
  // write output file tailor
  //av_write_trailer(fmt_ctx_o);
end:
  if (video_dec_ctx)
      avcodec_close(video_dec_ctx);
  if (audio_dec_ctx)
      avcodec_close(audio_dec_ctx);
  avformat_close_input(&fmt_ctx);
  if (video_dst_file)
      fclose(video_dst_file);
  if (audio_dst_file)
      fclose(audio_dst_file);
  av_free(frame);
  av_free(video_dst_data[0]);
  av_free(audio_dst_data);

  return ret < 0;
}

int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type) {
  int ret;
  AVStream *st;
  AVCodecContext *dec_ctx = NULL;
  AVCodec *dec = NULL;

  ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
  if (ret < 0) {
    LOGI(LOG_LEVEL, "Could not find %s stream in inout file '%s'\n",
         av_get_media_type_string(type), "src_filename");
    return ret;
  } else {
    *stream_idx = ret;
    st = fmt_ctx->streams[ret];

    /* find decoder for the stream */
    dec_ctx = st->codec;
    dec = avcodec_find_decoder(dec_ctx->codec_id);
    if (!dec) {
      LOGI(LOG_LEVEL, "Failed to find %s codec\n", av_get_media_type_string(type));
      return ret;
    }
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
      LOGI(LOG_LEVEL, "Failed to open %s codec\n", av_get_media_type_string(type));
      return ret;
    }
    return 0;
  }
}

int decode_packet(int *got_frame, AVFrame *frame, int cached, int frameCount) {
  int ret = 0;
  if (pkt.stream_index == video_stream_idx) {
    ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
    if (ret < 0) {
      LOGI(LOG_LEVEL, "Error decoding video frame\n");
      return ret;
    }
    if (*got_frame) {
      LOGI(LOG_LEVEL, "video_frame%s n:%d coded_n:%d pts:%s\n",
           cached ? "(cached)" : "",
           video_frame_count++, frame->coded_picture_number,
           av_ts2timestr(frame->pts, &video_dec_ctx->time_base));
#if 1
      /* copy decoded frame to dest buffer */
      av_image_copy(video_dst_data, video_dst_linesize,
                    (const uint8_t **)(frame->data),
                    frame->linesize,
                    video_dec_ctx->pix_fmt,
                    video_dec_ctx->width,
                    video_dec_ctx->height);
      /* write to rawvideo file */
      fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
#else
      /* write to another file */
      AVFrame *frame_o = avcodec_alloc_frame();
      uint8_t* frame_buf;
      int size;
      AVPacket pkt_o;
      int y_size = video_dec_ctx->width * video_dec_ctx->height;
      av_new_packet(&pkt_o, y_size * 3);
      
      size = avpicture_get_size(video_dec_ctx->pix_fmt, 
                                video_dec_ctx->width, 
                                video_dec_ctx->height);
      frame_buf = (uint8_t *)av_malloc(size);
      avpicture_fill((AVPicture *)frame_o, frame_buf, 
                      video_dec_ctx->pix_fmt, 
                      video_dec_ctx->width, 
                      video_dec_ctx->height);

      frame_o->data[0] = frame_buf;  // ����Y  
      frame_o->data[1] = frame_buf + y_size;  // U   
      frame_o->data[2] = frame_buf + y_size * 5 / 4; // V
      frame_o->pts = frameCount;

      int got_picture = 0;
      int ret = avcodec_encode_video2(video_dec_ctx, &pkt_o,
        frame_o, &got_picture);
      if (ret < 0){  
        LOGI(LOG_LEVEL, "Failed to encode!\n");
        return -1;  
      }  
      if (got_picture==1){  
        LOGI(LOG_LEVEL, "Succeed to encode 1 frame!\n");
        pkt_o.stream_index = video_stream->index;  
        ret = av_write_frame(fmt_ctx_o, &pkt);  
        av_free_packet(&pkt_o);
      }
#endif
    }
  } else if (pkt.stream_index == audio_stream_idx) {
    // TODO
  }
}

int get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt) {
  int i;
  struct sample_fmt_entry {
    enum AVSampleFormat sample_fmt;
    const char *fmt_be, *fmt_le;
  } sample_fmt_entries[] = {
    { AV_SAMPLE_FMT_U8,  "u8",     "u8"    },
    { AV_SAMPLE_FMT_S16, "s16be",  "s16le" },
    { AV_SAMPLE_FMT_S32, "s32be",  "s32le" },
    { AV_SAMPLE_FMT_FLT, "f32be",  "f32le" },
    { AV_SAMPLE_FMT_DBL, "f64be",  "f64le" },
  };
  *fmt = NULL;

  for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
    struct sample_fmt_entry *entry = &sample_fmt_entries[i];
    if (sample_fmt == entry->sample_fmt) {
      *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
      return 0;
    }
  }

  LOGI(LOG_LEVEL, "sample format %s is not supported as output format\n",
       av_get_sample_fmt_name(sample_fmt));
  return -1;
}
