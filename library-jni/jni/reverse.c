#include "reverse.h"

#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

AVFormatContext *fmt_ctx = NULL;
AVFormatContext *fmt_ctx_o = NULL;
AVOutputFormat *fmt_o;
AVStream *video_o;
AVCodec *codec_o;
uint8_t *picture_buf_o;
AVFrame *picture_o;
AVPacket pkt_o;

AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx, *video_enc_ctx = NULL;
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
const char *str_appendix = ".jpg";

#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT PIX_FMT_YUV420P /* default pix_fmt */

#define USE_MEMORY_BUFFER 1;

typedef struct YUVBufferList{
  uint8_t*       data[4];
  int            linesize[4];
  int            width;
  int            height;
  void*          next;
} YUVBufferList;

YUVBufferList* yuvBufferListHeader = NULL;
YUVBufferList* yuvBufferListItem = NULL;

int getFileSize(const char *filename) {
  int size = 0;
  FILE* f = fopen(filename, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);
  }
  return size;
}

int SaveYuv(const char *buf, int wrap, int xsize,
            int ysize, const FILE *f) {  
  int i;
  for(i = 0; i < ysize; i++) {
    fwrite(buf + i * wrap, 1, xsize, f);
  }
  return 1;
}

int CopyYuv(const uint8_t *buf_src, int wrap, int xsize,
            int ysize, uint8_t *buf_dst) {
  int i, pos = 0;
  for(i = 0; i < ysize; i++) {
    memcpy((uint8_t*)(&buf_dst[i * xsize]), (uint8_t*)(&buf_src[i * wrap]), xsize);
    pos = i * xsize;
  }
  return pos;
}

int SaveFrame(int nszBuffer, uint8_t *buffer, const char* cOutFileName) {
  int bRet = 0;

  if( nszBuffer > 0 ) {
    FILE *pFile = fopen(cOutFileName, "wb");
    if(pFile) {
      fwrite(buffer, sizeof(uint8_t), nszBuffer, pFile);
      bRet = 1;
      fclose(pFile);
    }
  }
  return bRet;
}

int ReadFrame(int *nszBuffer, uint8_t **buffer, const char* cOutFileName) {
  int bRet = 0;
  FILE *pFile = fopen(cOutFileName, "rb");
  if (pFile)
  {
    int read_size = 0;
    *nszBuffer = 0;
    while ((read_size = fread(&(*buffer)[*nszBuffer], sizeof(uint8_t), 1024 * 1024, pFile)) > 0) {
      *nszBuffer += read_size;
    }
    bRet = 1;
    (*buffer)[*nszBuffer] = '\0';
    fclose(pFile);
    remove(cOutFileName);
  }
  return bRet;
}

int decode2YUV(const char* SRC_FILE, const char* OUT_FMT_FILE,
               AVCodecContext *codecContext,
               int *stream_index) {
  AVFormatContext *formatContext_src = NULL;
  AVStream *st_src = NULL;
  AVCodec *codec_src = NULL;
  AVFrame *frame_src = NULL;

  AVPacketList *pktListHeader = NULL;
  AVPacketList *pktListItem = NULL;

  int ret = -1;
  int frameCount = 0;
  int video_outbuf_size_dst;
  static uint8_t *video_outbuf_dst;

  /* open input file, and allocated format context */
  if (avformat_open_input(&formatContext_src, SRC_FILE, NULL, NULL) < 0) {
    LOGI(LOG_LEVEL, "Could not open source file %s\n", SRC_FILE);
    goto end;
  }
  /* retrieve stream information */
  if (avformat_find_stream_info(formatContext_src, NULL) < 0) {
    LOGI(LOG_LEVEL, "Could not find stream information\n");
    goto end;
  }

  /* retrieve video stream index */
  ret = av_find_best_stream(formatContext_src, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (ret < 0) {
    LOGI(LOG_LEVEL, "Could not find video stream information\n");
    goto end;
  }
  *stream_index = ret;
  LOGI(LOG_LEVEL, "stream_index:%d\n", ret);
  if (formatContext_src == NULL) {
    LOGI(LOG_LEVEL, "format context is NULL\n");
    goto end;
  }
  /* retrieve video stream */
  st_src = formatContext_src->streams[ret];
  if (st_src == NULL) {
    LOGI(LOG_LEVEL, "video stream is NULL\n");
    goto end;
  }
  /* retrieve decodec context for video stream */
  if (st_src->codec == NULL) {
    LOGI(LOG_LEVEL, "decodec context is NULL\n");
    goto end;
  }

  codec_src = avcodec_find_decoder(st_src->codec->codec_id);
  if ((ret = avcodec_open2(st_src->codec, codec_src, NULL)) < 0) {
    LOGI(LOG_LEVEL, "Failed to open %s codec\n",
         av_get_media_type_string(st_src->codec->codec_id));
    goto end;
  }

  codecContext->width = st_src->codec->width;
  codecContext->height = st_src->codec->height;

  /* initialize packet, set data to NULL, let the demuxer fill it */
  AVPacket pt_src;
  frame_src = avcodec_alloc_frame();

  int got_frame = -1, got_output = -1, numBytes = 0;
  float start_timestamp;
  float timestamp_interval;
  // first step: get the frame count, start timestamp and the timestamp interval
  while (1) {
    av_init_packet(&pt_src);
    pt_src.data = NULL;
    pt_src.size = 0;
    if (av_read_frame(formatContext_src, &pt_src) < 0) {
      break;
    }
    if (pt_src.stream_index == (*stream_index)) {
      ret = avcodec_decode_video2(st_src->codec, frame_src, &got_frame, &pt_src);
      if (ret < 0) {
        LOGI(LOG_LEVEL, "Error decoding video frame\n");
        goto end;
      }
      if (got_frame) {
        LOGI(LOG_LEVEL, "video_frame n:%d coded_n:%d pts:%s\n",
             frameCount, frame_src->coded_picture_number,
             av_ts2timestr(frame_src->pts, &st_src->codec->time_base));
        if (frameCount == 1) {
          start_timestamp = st_src->start_time;
          timestamp_interval = av_q2d(st_src->codec->time_base) * frame_src->pts;
          LOGI(LOG_LEVEL, "start_time:%f, timestamp_interval:%f\n",
               start_timestamp, timestamp_interval);
        }
        frameCount++;
      }
    }
  }
  LOGI(LOG_LEVEL, "Decoding video frame DONE!\n");
  avcodec_close(st_src->codec);
  if ((ret = avcodec_open2(st_src->codec, codec_src, NULL)) < 0) {
    LOGI(LOG_LEVEL, "Failed to open %s codec\n",
         av_get_media_type_string(st_src->codec->codec_id));
    goto end;
  }
  // second step: get the seeked frame and encode it to video
  LOGI(LOG_LEVEL, "Start encoding ...\n");
  int i = 0;
  frameCount -= 1;
  AVFormatContext *formatContext_dst = NULL;
  AVCodec *codec_dst;
  AVStream *st_dst;
  AVFrame *frame_pic;
  AVPicture picture_pic;
  int width, height;
  AVPacket pkt;

  width = codecContext->width;
  height = codecContext->height;
  
  /* init AVFormatContext */
  avformat_alloc_output_context2(&formatContext_dst, NULL, NULL, OUT_FMT_FILE);
  if (!formatContext_dst) {
    LOGI(LOG_LEVEL, "[output]Could not deduce output format from file extension: using MPEG.\n");
    avformat_alloc_output_context2(&formatContext_dst, NULL, "mpeg", OUT_FMT_FILE);
  }
  if (!formatContext_dst) {
    return 0;
  }

  LOGI(LOG_LEVEL, "Encode video file %s\n", OUT_FMT_FILE);
  int codec_id = formatContext_dst->oformat->video_codec;

  /* find the mpeg1 video encoder */
  codec_dst = avcodec_find_encoder(codec_id);
  if (!codec_dst) {
      LOGI(LOG_LEVEL, "Codec not found\n");
      return 0;
  }
  /* init AVStream */
  st_dst = avformat_new_stream(formatContext_dst, codec_dst);
  if (!st_dst) {
    LOGI(LOG_LEVEL, "[output]Could not allocate stream\n");
    return 0;
  }
  st_dst->id = 1;
  if (formatContext_dst->oformat->flags & AVFMT_GLOBALHEADER)
    st_dst->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

  /* init AVCodecContext */
  avcodec_get_context_defaults3(st_dst->codec, codec_dst);
  {
    /* init AVCodecContext for open */
    st_dst->codec->codec_id = codec_id;

    /* Put sample parameters. */
    st_dst->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    st_dst->codec->bit_rate = 400000;//codecContext->bit_rate;
    /* Resolution must be a multiple of two. */
    st_dst->codec->width    = width;
    st_dst->codec->height   = height;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1. */
    st_dst->codec->time_base.den = 25;//STREAM_FRAME_RATE;//st_src->codec->time_base.den;
    st_dst->codec->time_base.num = 1;//st_src->codec->time_base.num;
    st_dst->codec->gop_size      = 12;//st_src->codec->gop_size;
    st_dst->codec->pix_fmt       = PIX_FMT_YUV420P;//st_src->codec->pix_fmt;
    st_dst->codec->qmin          = 10;//st_src->codec->qmin;
    st_dst->codec->qmax          = 51;//st_src->codec->qmax;
    if (st_dst->codec->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        st_dst->codec->max_b_frames = 2;
    }
    if (st_dst->codec->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
        /* Needed to avoid using macroblocks in which some coeffs overflow.
         * This does not happen with normal video, it just happens here as
         * the motion of the chroma plane does not match the luma plane. */
        st_dst->codec->mb_decision = 2;
    }
    /* Some formats want stream headers to be separate. */
    if (formatContext_dst->oformat->flags & AVFMT_GLOBALHEADER)
        st_dst->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  /* open it */
  if (avcodec_open2(st_dst->codec, codec_dst, NULL) < 0) {
      LOGI(LOG_LEVEL, "Could not open codec\n");
      return 0;
  }

  frame_pic = avcodec_alloc_frame();
  if (!frame_pic) {
      LOGI(LOG_LEVEL, "Could not allocate video frame\n");
      return 0;
  }
  if (avpicture_alloc(&picture_pic, st_dst->codec->pix_fmt,
                      width, height) < 0) {
    LOGI(LOG_LEVEL, "[output]Could not allocate video picture\n");
    return 0;
  }
  if (st_dst->codec->pix_fmt != PIX_FMT_YUV420P) {
    LOGI(LOG_LEVEL, "[output]pix_fmt: %d\n", st_dst->codec->pix_fmt);
    ret = avpicture_alloc(&picture_pic, PIX_FMT_YUV420P,
                          width, height);
    if (ret < 0) {
      LOGI(LOG_LEVEL, "[output]Could not allocate temporary picture\n");
      return 0;
    }
  }
  /* copy data and linesize picture pointers to frame */
  *((AVPicture *)frame_pic) = picture_pic;

  /* open the output file, if needed */
  if (!(formatContext_dst->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&formatContext_dst->pb, OUT_FMT_FILE, AVIO_FLAG_WRITE) < 0) {
      LOGI(LOG_LEVEL, "[output]Could not open '%s'\n", OUT_FMT_FILE);
      return 0;
    }
  }
  /* Write the stream header, if any. */
  if (avformat_write_header(formatContext_dst, NULL) < 0) {
    LOGI(LOG_LEVEL, "[output]Error occurred when opening output file\n");
    return 0;
  }

  static struct SwsContext *sws_ctx;
  struct SwsContext* fooContext = sws_getContext(width, height,
                                                 PIX_FMT_YUV420P,
                                                 width, height,
                                                 PIX_FMT_YUV420P,
                                                 SWS_BICUBIC,
                                                 NULL, NULL, NULL);
  /************ end of init decoder ******************/
  
  for(; frameCount >= 0; frameCount--) {
    av_init_packet(&pt_src);
    pt_src.data = NULL;
    pt_src.size = 0;
    int64_t seek_target = (frameCount * timestamp_interval - start_timestamp) * 1000 * 1000;
    LOGI(LOG_LEVEL, "timestamp_interval:%f\n", timestamp_interval);
    LOGI(LOG_LEVEL, "seek_target: %d\n", seek_target);
    int64_t seeked_ts = av_rescale_q(seek_target,
                                     AV_TIME_BASE_Q,
                                     st_src->time_base);
    LOGI(LOG_LEVEL, "Seek.... ts[%d]\n", seeked_ts);
    if (av_seek_frame(formatContext_src, *stream_index,
                      seeked_ts, AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD) < 0) {
      LOGI(LOG_LEVEL, "Seek error.\n");
      continue;
    }
loop:
    if (av_read_frame(formatContext_src, &pt_src) < 0) {
      LOGI(LOG_LEVEL, "No frame to read.\n");
      break;
    } else if (pt_src.stream_index != (*stream_index)) {
      LOGI(LOG_LEVEL, "pt_src.stream_index:%d, stream_index:%d\n", pt_src.stream_index, *stream_index);
      goto loop;
    }
    LOGI(LOG_LEVEL, "pt_src.stream_index:%d, stream_index:%d\n", pt_src.stream_index, *stream_index);
    if (pt_src.stream_index == (*stream_index)) {
      LOGI(LOG_LEVEL, "Start decode the seeked frame.\n");
      ret = avcodec_decode_video2(st_src->codec, frame_src, &got_frame, &pt_src);
      if (ret < 0) {
        LOGI(LOG_LEVEL, "Error decoding video frame\n");
        goto end;
      }
      if (got_frame) {
        LOGI(LOG_LEVEL, "Decoding...[%d]\n", frameCount);
        LOGI(LOG_LEVEL, "video_frame n:%d coded_n:%d pts:%s\n",
             frameCount, frame_src->coded_picture_number,
             av_ts2timestr(frame_src->pts, &st_src->codec->time_base));
        sws_scale(fooContext, frame_src->data, frame_src->linesize,
                  0, height,
                  frame_pic->data, frame_pic->linesize);

        av_init_packet(&pkt);
        pkt.data = NULL;    // packet data will be allocated by the encoder
        pkt.size = 0;

        frame_pic->pts = i;
        //((AVFrame*)&picture_pic)->pts = i;
        /* encode the image */
        ret = avcodec_encode_video2(st_dst->codec, &pkt, frame_pic, &got_output);
        if (ret < 0) {
            LOGI(LOG_LEVEL, "Error encoding frame\n");
            break;
        }

        if (got_output) {
          LOGI(LOG_LEVEL, "Encoding to video...[%d]", i);
//          if (st_dst->codec->coded_frame->pts != AV_NOPTS_VALUE) {
//            pkt.pts = av_rescale_q(st_dst->codec->coded_frame->pts,
//                                      st_dst->codec->time_base,
//                                      st_dst->time_base);
//          }
          /* Write the compressed frame to the media file. */
          pkt.stream_index = (*stream_index);
          ret = av_interleaved_write_frame(formatContext_dst, &pkt);
          if (ret < 0) {
            LOGI(LOG_LEVEL, "[output] write frame failed: %d \n", ret);
          } else {
            i++;
          }
        }
      }
    }
  }
  av_write_trailer(formatContext_dst);
  LOGI(LOG_LEVEL, "Encoding video frame DONE!\n");
end:
  if (formatContext_src) {
    avformat_close_input(&formatContext_src);
  }
  if (frame_src) {
    av_free(frame_src);
  }
  sws_freeContext(fooContext);
  return frameCount;
}

int encodeYUV2Video(const char* TMP_FOLDER, int frameCount, int stream_index,
                    const char* OUT_FMT_FILE, AVCodecContext *codecContext) {
  LOGI(LOG_LEVEL, "encodeYUV2Video: %d(w:%d,h:%d)\n", frameCount,
       codecContext->width, codecContext->height);

  AVFormatContext *formatContext_dst = NULL;
  AVCodec *codec_dst;
  AVStream *st_dst;
  AVFrame *frame_pic;
  AVPicture picture_pic;
  int i, ret, got_output, width, height;
  FILE *pictureF;
  AVPacket pkt;
  char DST_FILE[100];

  width = codecContext->width;
  height = codecContext->height;

  /****************** start of encoder init ************************/
  /* allocate the output media context */
  /* init AVFormatContext */
  avformat_alloc_output_context2(&formatContext_dst, NULL, NULL, OUT_FMT_FILE);
  if (!formatContext_dst) {
    LOGI(LOG_LEVEL, "[output]Could not deduce output format from file extension: using MPEG.\n");
    avformat_alloc_output_context2(&formatContext_dst, NULL, "mpeg", OUT_FMT_FILE);
  }
  if (!formatContext_dst) {
    return 0;
  }

  LOGI(LOG_LEVEL, "Encode video file %s\n", OUT_FMT_FILE);
  int codec_id = formatContext_dst->oformat->video_codec;

  /* find the mpeg1 video encoder */
  codec_dst = avcodec_find_encoder(codec_id);
  if (!codec_dst) {
      LOGI(LOG_LEVEL, "Codec not found\n");
      return 0;
  }
  /* init AVStream */
  st_dst = avformat_new_stream(formatContext_dst, codec_dst);
  if (!st_dst) {
    LOGI(LOG_LEVEL, "[output]Could not allocate stream\n");
    return 0;
  }
  st_dst->id = 1;
  if (formatContext_dst->oformat->flags & AVFMT_GLOBALHEADER)
    st_dst->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

  /* init AVCodecContext */
  avcodec_get_context_defaults3(st_dst->codec, codec_dst);
  {
    /* init AVCodecContext for open */
    st_dst->codec->codec_id = codec_id;

    /* Put sample parameters. */
    st_dst->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    st_dst->codec->bit_rate = 400000;//codecContext->bit_rate;
    /* Resolution must be a multiple of two. */
    st_dst->codec->width    = width;
    st_dst->codec->height   = height;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1. */
    st_dst->codec->time_base.den = 25;//STREAM_FRAME_RATE;//st_src->codec->time_base.den;
    st_dst->codec->time_base.num = 1;//st_src->codec->time_base.num;
    st_dst->codec->gop_size      = 12;//st_src->codec->gop_size;
    st_dst->codec->pix_fmt       = PIX_FMT_YUV420P;//st_src->codec->pix_fmt;
    st_dst->codec->qmin          = 10;//st_src->codec->qmin;
    st_dst->codec->qmax          = 51;//st_src->codec->qmax;
    if (st_dst->codec->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        st_dst->codec->max_b_frames = 2;
    }
    if (st_dst->codec->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
        /* Needed to avoid using macroblocks in which some coeffs overflow.
         * This does not happen with normal video, it just happens here as
         * the motion of the chroma plane does not match the luma plane. */
        st_dst->codec->mb_decision = 2;
    }
    /* Some formats want stream headers to be separate. */
    if (formatContext_dst->oformat->flags & AVFMT_GLOBALHEADER)
        st_dst->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  /* open it */
  if (avcodec_open2(st_dst->codec, codec_dst, NULL) < 0) {
      LOGI(LOG_LEVEL, "Could not open codec\n");
      return 0;
  }

  frame_pic = avcodec_alloc_frame();
  if (!frame_pic) {
      LOGI(LOG_LEVEL, "Could not allocate video frame\n");
      return 0;
  }
  if (avpicture_alloc(&picture_pic, st_dst->codec->pix_fmt,
                      width, height) < 0) {
    LOGI(LOG_LEVEL, "[output]Could not allocate video picture\n");
    return 0;
  }
  if (st_dst->codec->pix_fmt != PIX_FMT_YUV420P) {
    LOGI(LOG_LEVEL, "[output]pix_fmt: %d\n", st_dst->codec->pix_fmt);
    ret = avpicture_alloc(&picture_pic, PIX_FMT_YUV420P,
                          width, height);
    if (ret < 0) {
      LOGI(LOG_LEVEL, "[output]Could not allocate temporary picture\n");
      return 0;
    }
  }
  /* copy data and linesize picture pointers to frame */
  *((AVPicture *)frame_pic) = picture_pic;

  /* open the output file, if needed */
  if (!(formatContext_dst->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&formatContext_dst->pb, OUT_FMT_FILE, AVIO_FLAG_WRITE) < 0) {
      LOGI(LOG_LEVEL, "[output]Could not open '%s'\n", OUT_FMT_FILE);
      return 0;
    }
  }
  /* Write the stream header, if any. */
  if (avformat_write_header(formatContext_dst, NULL) < 0) {
    LOGI(LOG_LEVEL, "[output]Error occurred when opening output file\n");
    return 0;
  }
  static struct SwsContext *sws_ctx;
  struct SwsContext* fooContext = sws_getContext(width, height,
                                                 PIX_FMT_YUV420P,
                                                 width, height,
                                                 PIX_FMT_YUV420P,
                                                 SWS_BICUBIC,
                                                 NULL, NULL, NULL);
  /************ end of init decoder ******************/

  /* allocate and init a re-usable frame */
#ifndef USE_MEMORY_BUFFER
  int outbuf_size = 1000000;
  uint8_t *outbuf = malloc(outbuf_size);
#endif
  int nbytes = avpicture_get_size(PIX_FMT_YUV420P, width, height);
  uint8_t *buffer = (uint8_t *)av_malloc(nbytes * sizeof(uint8_t));
  int file_size;
  uint8_t *inbuf[4];
  int widthMultiHeight = width * height;
  inbuf[0] = (uint8_t*)malloc(widthMultiHeight);
  inbuf[1] = (uint8_t*)malloc(widthMultiHeight >> 2);
  inbuf[2] = (uint8_t*)malloc(widthMultiHeight >> 2);
  inbuf[3] = NULL;

  /* encode 1 second of video */
  i = frameCount;
  frameCount = 0;
  for(; i >= 0; i--) {
#ifdef USE_MEMORY_BUFFER
      if (yuvBufferListHeader == NULL) {
        LOGI(LOG_LEVEL, "yuvBufferListHeader is NULL!\n");
        break;
      }
      yuvBufferListItem = yuvBufferListHeader;
      yuvBufferListHeader = (YUVBufferList *)yuvBufferListHeader->next;

      memset(inbuf[0], 0, widthMultiHeight);
      memset(inbuf[1], 0, widthMultiHeight >> 2);
      memset(inbuf[2], 0, widthMultiHeight >> 2);
      memcpy(inbuf[0], yuvBufferListItem->data[0], widthMultiHeight);
      av_free(yuvBufferListItem->data[0]);
      memcpy(inbuf[1], yuvBufferListItem->data[1], widthMultiHeight >> 2);
      av_free(yuvBufferListItem->data[1]);
      memcpy(inbuf[2], yuvBufferListItem->data[2], widthMultiHeight >> 2);
      av_free(yuvBufferListItem->data[2]);
      av_free(yuvBufferListItem);
#else
      memset(DST_FILE, 0, strlen(DST_FILE));
      strcpy(DST_FILE, TMP_FOLDER);
      strcat(DST_FILE, "/");
      char frame_count_str[8];
      sprintf(frame_count_str, "%d", i);
      strcat(DST_FILE, frame_count_str);
      strcat(DST_FILE, ".yuv");
//      if (!ReadFrame(&file_size, &outbuf, DST_FILE)) {
//        LOGI(LOG_LEVEL, "read file error: %s\n", DST_FILE);
//        break;
//      }
      FILE *pFile = fopen(DST_FILE, "rb");
      if (pFile)
      {
        LOGI(LOG_LEVEL, "reading from file\n");
        int read_size = 0;
        file_size = 0;
        while ((read_size = fread(&(outbuf[file_size]), sizeof(uint8_t), 1024 * 1024, pFile)) > 0) {
          file_size += read_size;
          LOGI(LOG_LEVEL, "read size: %d", read_size);
        }
        outbuf[file_size] = '\0';
        fclose(pFile);
        remove(DST_FILE);
      }
      memcpy(inbuf[0], outbuf, widthMultiHeight);
      memcpy(inbuf[1], outbuf + widthMultiHeight, widthMultiHeight >> 2);
      memcpy(inbuf[2], outbuf + (widthMultiHeight * 5 >> 2), widthMultiHeight >> 2);
#endif
      int inlinesize[4] = {width, width / 2, width / 2, 0};
      avpicture_fill(frame_pic, buffer, PIX_FMT_YUV420P,
                     width, height);
      sws_scale(fooContext, inbuf, inlinesize,
                0, height,
                frame_pic->data, frame_pic->linesize);

      av_init_packet(&pkt);
      pkt.data = NULL;    // packet data will be allocated by the encoder
      pkt.size = 0;

      frame_pic->pts = frameCount;
      //((AVFrame*)&picture_pic)->pts = frameCount;
      /* encode the image */
      ret = avcodec_encode_video2(st_dst->codec, &pkt, frame_pic, &got_output);
      if (ret < 0) {
          LOGI(LOG_LEVEL, "Error encoding frame\n");
          break;
      }

      if (got_output) {
          LOGI(LOG_LEVEL, "Encoding to video...[%d]", frameCount);
          if (st_dst->codec->coded_frame->pts != AV_NOPTS_VALUE) {
            pkt.pts = av_rescale_q(st_dst->codec->coded_frame->pts,
                                      st_dst->codec->time_base,
                                      st_dst->time_base);
          }
          /* Write the compressed frame to the media file. */
          pkt.stream_index = stream_index;
          ret = av_interleaved_write_frame(formatContext_dst, &pkt);
          if (ret < 0) {
            LOGI(LOG_LEVEL, "[output] write frame failed: %d \n", ret);
          } else {
            frameCount++;
          }
      }
  }
  av_write_trailer(formatContext_dst);;
  sws_freeContext(fooContext);
  LOGI(LOG_LEVEL, "Decoding video frame DONE!\n");
  return 1;
}

int doReverseViaBmp(const char* SRC_FILE, const char* OUT_FMT_FILE) {
  AVCodecContext *codecContext = NULL;
  AVCodec *codec = NULL;
  
  av_register_all();
  enum AVCodecID codec_id = AV_CODEC_ID_MPEG4;
  codec = avcodec_find_encoder(codec_id);
  if (!codec) {
    LOGI(LOG_LEVEL, "Cannot find codec.\n");
    return 0;
  }
  codecContext = avcodec_alloc_context3(codec);
  if (!codecContext) {
    LOGI(LOG_LEVEL, "Cannot find codecContext.\n");
    return 0;
  }
  int stream_index = 0;
  return decode2YUV(SRC_FILE, OUT_FMT_FILE, codecContext, &stream_index);
}

int reverse(char *file_path_src, char *file_path_desc,
            long positionUsStart, long positionUsEnd,
            int video_stream_no, int audio_stream_no,
            int subtitle_stream_no) {
  const char *TMP_FOLDER = "/sdcard/tmp";
  LOGI(LOG_LEVEL, "reversing...");
  return doReverseViaBmp(file_path_src, file_path_desc);
}


