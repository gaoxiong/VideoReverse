#include "reverse.h"

#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT PIX_FMT_YUV420P /* default pix_fmt */
int BUFFER_LIST_SIZE = 100;

AVFormatContext *formatContext_src = NULL;
AVFormatContext *formatContext_dst = NULL;
AVStream *st_src = NULL;
AVStream *st_dst = NULL;
AVCodec *codec_dst = NULL;
AVCodec *codec_src = NULL;
AVFrame *frame_src = NULL;
AVFrame* frame_dst = NULL;
struct SwsContext* fooContext = NULL;
int ret = -1;
int stream_index = -1;
int frameCount = 0;
int width, height;
int encodeFramePos = 0;
int got_frame = -1;


typedef struct YUVBufferList{
  uint8_t*       data[4];
  void*          next;
} YUVBufferList;
YUVBufferList *pHeader = NULL;

int CopyYuv(const uint8_t *buf_src, int wrap, int xsize,
            int ysize, uint8_t *buf_dst) {
  int i, pos = 0;
  for(i = 0; i < ysize; i++) {
    memcpy((uint8_t*)(&buf_dst[i * xsize]), (uint8_t*)(&buf_src[i * wrap]), xsize);
    pos = i * xsize;
  }
  return pos;
}

int initDecodeEnvironmentAndGetVideoFrameCount(const char* SRC_FILE) {
  /* open input file, and allocated format context */
  if (avformat_open_input(&formatContext_src, SRC_FILE, NULL, NULL) < 0) {
    LOGI(LOG_LEVEL, "Could not open source file %s\n", SRC_FILE);
    return -1;
  }
  /* retrieve stream information */
  if (avformat_find_stream_info(formatContext_src, NULL) < 0) {
    LOGI(LOG_LEVEL, "Could not find stream information\n");
    return -1;
  }
  /* retrieve video stream index */
  ret = av_find_best_stream(formatContext_src, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (ret < 0) {
    LOGI(LOG_LEVEL, "Could not find video stream information\n");
    return -1;
  }
  stream_index = ret;
  /* retrieve video stream */
  st_src = formatContext_src->streams[ret];
  if (st_src == NULL) {
    LOGI(LOG_LEVEL, "video stream is NULL\n");
    return -1;
  }
  /* retrieve decodec context for video stream */
  if (st_src->codec == NULL) {
    LOGI(LOG_LEVEL, "decodec context is NULL\n");
    return -1;
  }
  codec_src = avcodec_find_decoder(st_src->codec->codec_id);
  if ((ret = avcodec_open2(st_src->codec, codec_src, NULL)) < 0) {
    LOGI(LOG_LEVEL, "Failed to open %s codec\n",
         av_get_media_type_string(st_src->codec->codec_id));
    return -1;
  }

  frame_src = avcodec_alloc_frame();
  AVPacket pt_src;
  int got_frame = -1;
  frameCount = 0;
  while (1) {
    av_init_packet(&pt_src);
    pt_src.data = NULL;
    pt_src.size = 0;
    if (av_read_frame(formatContext_src, &pt_src) < 0) {
      break;
    }
    if (pt_src.stream_index == stream_index) {
      ret = avcodec_decode_video2(st_src->codec, frame_src, &got_frame, &pt_src);
      if (ret < 0) {
        LOGI(LOG_LEVEL, "Error decoding video frame\n");
        continue;
      }
      if (got_frame) {
        frameCount++;
      }
    }
  }
  LOGI(LOG_LEVEL, "initDecodeEnvironmentAndGetVideoFrameCount DONE[frameCount:%d]!\n", frameCount);
  return 0;
}

void resetCodec() {
  avcodec_close(st_src->codec);
  if (avcodec_open2(st_src->codec, codec_src, NULL) < 0) {
    LOGI(LOG_LEVEL, "[reset]Failed to open %s codec\n",
         av_get_media_type_string(st_src->codec->codec_id));
  }
  av_seek_frame(formatContext_src, stream_index, 0, AVSEEK_FLAG_BACKWARD);
}

int initH263EncodeEnvironment(const char* OUT_FMT_FILE) {
  /* init AVFormatContext */
  avformat_alloc_output_context2(&formatContext_dst, NULL, NULL, OUT_FMT_FILE);
  if (!formatContext_dst) {
    LOGI(LOG_LEVEL, "Could not deduce output format from file extension: using MPEG.\n");
    avformat_alloc_output_context2(&formatContext_dst, NULL, "mpeg", OUT_FMT_FILE);
  }
  if (!formatContext_dst) {
    return -1;
  }
  int codec_id = formatContext_dst->oformat->video_codec;
  /* find the mpeg1 video encoder */
  codec_dst = avcodec_find_encoder(codec_id);
  if (!codec_dst) {
      LOGI(LOG_LEVEL, "Codec not found\n");
      return -1;
  }
  /* init AVStream */
  st_dst = avformat_new_stream(formatContext_dst, codec_dst);
  if (!st_dst) {
    LOGI(LOG_LEVEL, "Could not allocate stream\n");
    return -1;
  }
  st_dst->id = stream_index;
  if (formatContext_dst->oformat->flags & AVFMT_GLOBALHEADER) {
    st_dst->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }
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
    if (formatContext_dst->oformat->flags & AVFMT_GLOBALHEADER) {
      st_dst->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
  }
  /* open it */
  if (avcodec_open2(st_dst->codec, codec_dst, NULL) < 0) {
      LOGI(LOG_LEVEL, "Could not open codec\n");
      return -1;
  }
  /* open the output file, if needed */
  if (!(formatContext_dst->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&formatContext_dst->pb, OUT_FMT_FILE, AVIO_FLAG_WRITE) < 0) {
      LOGI(LOG_LEVEL, "[output]Could not open '%s'\n", OUT_FMT_FILE);
      return -1;
    }
  }
  /* Write the stream header, if any. */
  if (avformat_write_header(formatContext_dst, NULL) < 0) {
    LOGI(LOG_LEVEL, "[output]Error occurred when opening output file\n");
    return -1;
  }
  return 0;
}

int initReuseBuffer() {
  AVPicture picture_pic;
  frame_dst = avcodec_alloc_frame();
  if (!frame_dst) {
      LOGI(LOG_LEVEL, "Could not allocate video frame\n");
      return -1;
  }
  if (avpicture_alloc(&picture_pic, st_dst->codec->pix_fmt,
                      width, height) < 0) {
    LOGI(LOG_LEVEL, "Could not allocate video picture\n");
    return -1;
  }
  if (st_dst->codec->pix_fmt != PIX_FMT_YUV420P) {
    LOGI(LOG_LEVEL, "pix_fmt: %d\n", st_dst->codec->pix_fmt);
    ret = avpicture_alloc(&picture_pic, PIX_FMT_YUV420P,
                          width, height);
    if (ret < 0) {
      LOGI(LOG_LEVEL, "Could not allocate temporary picture\n");
      return -1;
    }
  }
  /* copy data and linesize picture pointers to frame */
  *((AVPicture *)frame_dst) = picture_pic;
  fooContext = sws_getContext(width, height, PIX_FMT_YUV420P,
                              width, height, PIX_FMT_YUV420P,
                              SWS_BICUBIC, NULL, NULL, NULL);
  return 0;
}

void copyFrame2List() {
  YUVBufferList* pItem = (YUVBufferList*)av_malloc(sizeof(YUVBufferList));
  int widthMultiHeight = width * height;
  pItem->data[0] = (uint8_t*)malloc(widthMultiHeight);
  pItem->data[1] = (uint8_t*)malloc(widthMultiHeight >> 2);
  pItem->data[2] = (uint8_t*)malloc(widthMultiHeight >> 2);
  pItem->data[3] = 0;
  memset(pItem->data[0], 0, widthMultiHeight);
  memset(pItem->data[1], 0, widthMultiHeight >> 2);
  memset(pItem->data[2], 0, widthMultiHeight >> 2);
  CopyYuv(frame_src->data[0], frame_src->linesize[0],
    width, height, pItem->data[0]);
  CopyYuv(frame_src->data[1], frame_src->linesize[1],
    width / 2, height / 2, pItem->data[1]);
  CopyYuv(frame_src->data[2], frame_src->linesize[2],
    width / 2, height / 2, pItem->data[2]);
  pItem->next = (void*)pHeader;
  pHeader = pItem;
}

int freeAVPacketItem(YUVBufferList* pItem) {
  if (pItem) {
    av_free(pItem->data[0]);
    av_free(pItem->data[1]);
    av_free(pItem->data[2]);
    av_free(pItem);
  }
  return 0;
}

int encodeYUVBufferList() {
  YUVBufferList* pItem = NULL;
  AVPacket pkt;
  int linesize[4] = {width, width / 2, width / 2, 0};
  int got_output = -1;
  while (pHeader) {
    pItem = pHeader;
    pHeader = pItem->next;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    sws_scale(fooContext, pItem->data, linesize,
              0, height,
              frame_dst->data, frame_dst->linesize);
    freeAVPacketItem(pItem);
    frame_dst->pts = encodeFramePos;
    /* encode the image */
    ret = avcodec_encode_video2(st_dst->codec, &pkt, frame_dst, &got_output);
    if (ret < 0) {
        LOGI(LOG_LEVEL, "Error encoding frame\n");
        continue;
    }
    if (got_output) {
      LOGI(LOG_LEVEL, "Encoding to video...[%d]", encodeFramePos);
//      if (st_dst->codec->coded_frame->pts != AV_NOPTS_VALUE) {
//        pkt.pts = av_rescale_q(st_dst->codec->coded_frame->pts,
//                               st_dst->codec->time_base,
//                               st_dst->time_base);
//      }
      /* Write the compressed frame to the media file. */
      pkt.stream_index = stream_index;
      ret = av_interleaved_write_frame(formatContext_dst, &pkt);
      if (ret < 0) {
        LOGI(LOG_LEVEL, "[output] write frame failed: %d \n", ret);
      } else {
        encodeFramePos++;
      }
    }
  }
}

YUVBufferList* getYUVBufferList(int startFramePos, int endFramePos) {
  int framePos = 0;
  AVPacket pt_src;
  LOGI(LOG_LEVEL, "start pos: %d, end pos: %d\n", startFramePos, endFramePos);
  pHeader = NULL;
  while (framePos <= endFramePos) {
    av_init_packet(&pt_src);
    pt_src.data = NULL;
    pt_src.size = 0;
    if (av_read_frame(formatContext_src, &pt_src) < 0) {
      LOGI(LOG_LEVEL, "No frame to read. frameCount:%d\n", framePos);
      break;
    }
    if (pt_src.stream_index == stream_index) {
      avcodec_decode_video2(st_src->codec, frame_src, &got_frame, &pt_src);
      if (got_frame) {
        framePos++;
        if (framePos > endFramePos) {
          break;
        } else if (framePos >= startFramePos) {
          LOGI(LOG_LEVEL, "video_frame n:%d coded_n:%d pts:%s\n",
               framePos, frame_src->coded_picture_number,
               av_ts2timestr(frame_src->pts, &st_src->codec->time_base));
          copyFrame2List();
        }
      }
    }
  }
  return pHeader;
}

int writeTrailer() {
  av_write_trailer(formatContext_dst);
  LOGI(LOG_LEVEL, "Encoding video frame DONE!\n");
  return 0;
}

int decode2YUV2Video(const char* SRC_FILE, const char* OUT_FMT_FILE) {
  ret = initDecodeEnvironmentAndGetVideoFrameCount(SRC_FILE);
  if (ret < 0) {
    LOGI(LOG_LEVEL, "initDecodeEnvironmentAndGetVideoFrameCount error.\n");
    goto end;
  }
  if (frameCount <= 0) {
    goto end;
  }
  width = st_src->codec->width;
  height = st_src->codec->height;
  
  //initYUVEncodeEnvironment();
  ret = initH263EncodeEnvironment(OUT_FMT_FILE);
  if (ret < 0) {
    LOGI(LOG_LEVEL, "initH263EncodeEnvironment error.\n");
    goto end;
  }
  // initReuseBuffer must be after initH263EncodeEnvironment
  ret = initReuseBuffer();
  if (ret < 0) {
    LOGI(LOG_LEVEL, "initReuseBuffer error.\n");
    goto end;
  }
  int loopTimes = (int) (frameCount / BUFFER_LIST_SIZE);
  int i, encodeFramePos = 0;
  YUVBufferList *pktListHeader = NULL;

  for (i = loopTimes; i >= 0; i--) {
    int startFramePos = BUFFER_LIST_SIZE * i;
    int endFramePos = startFramePos + BUFFER_LIST_SIZE - 1;

    resetCodec();
    pktListHeader = getYUVBufferList(startFramePos, endFramePos);
    if (pktListHeader == NULL) {
      LOGI(LOG_LEVEL, "pktListHeader is null.\n");
      break;
    }
    encodeYUVBufferList(encodeFramePos);
  }
  ret = writeTrailer(formatContext_dst);
end:
  freeReuseBuffer();
  //closeEncodeEnvironment();
  //closeDecodeEnvironment();
  return ret;
}

void closeEncodeEnvironment() {
  if (formatContext_dst) {
    avformat_close_input(&formatContext_dst);
  }
  if (frame_dst) {
    av_free(frame_dst);
  }
  if (st_dst && st_dst->codec) {
    avcodec_close(st_dst->codec);
  }
}

void closeDecodeEnvironment() {
  if (formatContext_src) {
    avformat_close_input(&formatContext_src);
  }
  if (frame_src) {
    av_free(frame_src);
  }
  if (st_src && st_src->codec) {
    avcodec_close(st_src->codec);
  }
}

void freeReuseBuffer() {
  if (fooContext) {
    sws_freeContext(fooContext);
  }
}

int reverse(char *file_path_src, char *file_path_desc,
            long positionUsStart, long positionUsEnd,
            int video_stream_no, int audio_stream_no,
            int subtitle_stream_no) {
  const char *TMP_FOLDER = "/sdcard/Movies/localfile.mp4";
  const char *TMP_FOLDER_DST = "/sdcard/Movies/r_localfile.mp4";
  LOGI(LOG_LEVEL, "reversing...");
  av_register_all();
  return decode2YUV2Video(file_path_src, file_path_desc);
}



