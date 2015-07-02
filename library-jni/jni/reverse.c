#include "reverse.h"

#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define OUTFILE_ENABLED 1

int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type);
int decode_packet(int *got_frame, AVFrame*, int cached, int, int);
void encode_frame_to_dst(AVFrame *frame, int frameCount);

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

#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT PIX_FMT_YUV420P /* default pix_fmt */

int SaveFrame(int nszBuffer, uint8_t *buffer, const char* cOutFileName)
{
  int bRet = 0;

  if( nszBuffer > 0 )
  {
    FILE *pFile = fopen(cOutFileName, "wb");
    if(pFile)
    {
      LOGI(LOG_LEVEL, "write to file");
      fwrite(buffer, sizeof(uint8_t), nszBuffer, pFile);
      bRet = 1;
      fclose(pFile);
    }
  }
   return bRet;
}

int WriteJPEG (AVCodecContext *pCodecCtx, AVFrame *pFrame,
                const char* cFileName, enum PixelFormat pix,
                uint8_t *buffer, int numBytes)
{
   int bRet = 0;
//   AVCodec *pMJPEGCodec=NULL;
//   AVCodecContext *pMJPEGCtx = avcodec_alloc_context();
//   if (pMJPEGCtx)
//   {
//      pMJPEGCtx->bit_rate = pCodecCtx->bit_rate;
//      pMJPEGCtx->width = pCodecCtx->width;
//      pMJPEGCtx->height = pCodecCtx->height;
//      pMJPEGCtx->pix_fmt = pix;
//      pMJPEGCtx->codec_id = AV_CODEC_ID_MJPEG;
//      pMJPEGCtx->codec_type = AVMEDIA_TYPE_VIDEO;
//      pMJPEGCtx->time_base.num = pCodecCtx->time_base.num;
//      pMJPEGCtx->time_base.den = pCodecCtx->time_base.den;
//      pMJPEGCodec = avcodec_find_encoder(pMJPEGCtx->codec_id);
//
//      if( pMJPEGCodec && (avcodec_open( pMJPEGCtx, pMJPEGCodec) >= 0) )
//      {
//         pMJPEGCtx->qmin = pMJPEGCtx->qmax = 3;
//         pMJPEGCtx->mb_lmin = pMJPEGCtx->lmin = pMJPEGCtx->qmin * FF_QP2LAMBDA;
//         pMJPEGCtx->mb_lmax = pMJPEGCtx->lmax = pMJPEGCtx->qmax * FF_QP2LAMBDA;
//         pMJPEGCtx->flags |= CODEC_FLAG_QSCALE;
//         pFrame->quality = 10;
//         pFrame->pts = 0;
//         int szBufferActual = avcodec_encode_video(pMJPEGCtx, buffer, numBytes, pFrame);
//         if( SaveFrame(szBufferActual, buffer, cFileName ) )
//            bRet = 1;
//
//         avcodec_close(pMJPEGCtx);
//      } else {
//        LOGI(LOG_LEVEL, "Can not find decoder!\n");
//      }
//   } else {
//      LOGI(LOG_LEVEL, "pMJPEGCtx is 0!");
//   }
   return bRet;
}

int decode2JPG(const char* SRC_FILE, const char* TMP_FOLDER, AVCodecContext *codecContext) {
  AVFormatContext *formatContext_src = NULL;
  AVStream *st_src = NULL;
  AVCodec *codec_src = NULL;
  AVFrame *frame_src = NULL;
  char DST_FILE[100];

  AVPacketList *pktListHeader = NULL;
  AVPacketList *pktListItem = NULL;

  int ret = -1;
  int vst_idx = -1;
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
  vst_idx = ret;
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
  LOGI(LOG_LEVEL, "codec_is is %d\n", st_src->codec->codec_id);
  
  codec_src = avcodec_find_decoder(st_src->codec->codec_id);
  if ((ret = avcodec_open2(st_src->codec, codec_src, NULL)) < 0) {
    LOGI(LOG_LEVEL, "Failed to open %s codec\n", 
         av_get_media_type_string(st_src->codec->codec_id));
    goto end;
  }
  LOGI(LOG_LEVEL, "codec name is %s\n", codec_src->name);

  /* initialize packet, set data to NULL, let the demuxer fill it */
  AVPacket pt_src;
  frame_src = avcodec_alloc_frame();

  /* for encoder */
  AVCodec *pMJPEGCodec=NULL;
  AVCodecContext *pMJPEGCtx = avcodec_alloc_context();
  if (pMJPEGCtx)
  {
    codecContext->bit_rate = pMJPEGCtx->bit_rate = st_src->codec->bit_rate;
    codecContext->width = pMJPEGCtx->width = st_src->codec->width;
    codecContext->height = pMJPEGCtx->height = st_src->codec->height;
    pMJPEGCtx->pix_fmt = PIX_FMT_YUVJ420P;
    pMJPEGCtx->codec_id = AV_CODEC_ID_MJPEG;
    pMJPEGCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    codecContext->time_base.num = pMJPEGCtx->time_base.num = st_src->codec->time_base.num;
    codecContext->time_base.den = pMJPEGCtx->time_base.den = st_src->codec->time_base.den;
    pMJPEGCodec = avcodec_find_encoder(pMJPEGCtx->codec_id);

    if( pMJPEGCodec && (avcodec_open( pMJPEGCtx, pMJPEGCodec) >= 0) )
    {
      pMJPEGCtx->qmin = pMJPEGCtx->qmax = 3;
      pMJPEGCtx->mb_lmin = pMJPEGCtx->lmin = pMJPEGCtx->qmin * FF_QP2LAMBDA;
      pMJPEGCtx->mb_lmax = pMJPEGCtx->lmax = pMJPEGCtx->qmax * FF_QP2LAMBDA;
      pMJPEGCtx->flags |= CODEC_FLAG_QSCALE;
    } else {
      LOGI(LOG_LEVEL, "Can not find decoder!\n");
      goto end;
    }
  } else {
    LOGI(LOG_LEVEL, "pMJPEGCtx is 0!");
    goto end;
  }
  
  int got_frame = -1, got_output = -1;
  LOGI(LOG_LEVEL, "Start decoding video frame\n");
  while (1) {
    av_init_packet(&pt_src);
    pt_src.data = NULL;
    pt_src.size = 0;

    AVPacket pt_dst;
    av_init_packet(&pt_dst);
    pt_dst.data = NULL;
    pt_dst.size = 0;
    if (av_read_frame(formatContext_src, &pt_src) < 0) {
      break;
    }
    if (pt_src.stream_index == vst_idx) {
      ret = avcodec_decode_video2(st_src->codec, frame_src, &got_frame, &pt_src);
      if (ret < 0) {
        LOGI(LOG_LEVEL, "Error decoding video frame\n");
        return frameCount;
      }
      if (got_frame) {
        LOGI(LOG_LEVEL, "video_frame n:%d coded_n:%d pts:%s\n",
             frameCount, frame_src->coded_picture_number,
             av_ts2timestr(frame_src->pts, &st_src->codec->time_base));

        memset(DST_FILE, 0, strlen(DST_FILE));
        strcpy(DST_FILE, TMP_FOLDER);
        strcat(DST_FILE, "//");
        char frame_count_str[8];
        LOGI(LOG_LEVEL, frame_count_str, "%d", frameCount++);
        strcat(DST_FILE, frame_count_str);
        strcat(DST_FILE, ".jpg");
        LOGI(LOG_LEVEL, "output file name: %s\n", DST_FILE);
        int numBytes = avpicture_get_size(PIX_FMT_YUVJ420P, 
                                          st_src->codec->width, 
                                          st_src->codec->height);
        uint8_t *buffer=(uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

        frame_src->quality = 10;
        frame_src->pts = 0;
        int szBufferActual = avcodec_encode_video(pMJPEGCtx, buffer, numBytes, frame_src);
        if (SaveFrame(szBufferActual, buffer, DST_FILE))
          ret = 1;

        av_free_packet(&pt_src);
      }
    }
  }
  avcodec_close(pMJPEGCtx);
  LOGI(LOG_LEVEL, "Decoding video frame DONE!\n");
end:
  if (formatContext_src) {
    avformat_close_input(&formatContext_src);
  }
  if (frame_src) {
    av_free(frame_src);
  }
  return frameCount;
}

int encodeJPG2Video(const char* TMP_FOLDER, int frameCount, 
                     const char* OUT_FMT_FILE, AVCodecContext *codecContext) {
  LOGI(LOG_LEVEL, "encodeJPG2Video: %d\n", frameCount);
  
  AVCodec *codec;
  AVCodecContext *c= NULL;
  int i, ret, x, y, got_output;
  FILE *f, *tmpF;
  FILE *pictureF;
  AVFrame *frame;
  AVPacket pkt;
  char DST_FILE[100];
  uint8_t endcode[] = { 0, 0, 1, 0xb7 };

  LOGI(LOG_LEVEL, "Encode video file %s\n", OUT_FMT_FILE);
  int codec_id = AV_CODEC_ID_MPEG4;

  /* find the mpeg1 video encoder */
  codec = avcodec_find_encoder(codec_id);
  if (!codec) {
      LOGI(LOG_LEVEL, "Codec not found\n");
      exit(1);
  }

  c = avcodec_alloc_context3(codec);

  /* put sample parameters */
  c->bit_rate = codecContext->bit_rate;
  /* resolution must be a multiple of two */
  c->width = codecContext->width;
  c->height = codecContext->height;
  /* frames per second */
  c->time_base= (AVRational){1,25};
  c->gop_size = 10; /* emit one intra frame every ten frames */
  c->max_b_frames=1;
  c->pix_fmt = PIX_FMT_YUV420P;

  if(codec_id == AV_CODEC_ID_H264)
      av_opt_set(c->priv_data, "preset", "slow", 0);

  /* open it */
  if (avcodec_open2(c, codec, NULL) < 0) {
      LOGI(LOG_LEVEL, "Could not open codec\n");
      exit(1);
  }

  f = fopen(OUT_FMT_FILE, "wb");
  if (!f) {
      LOGI(LOG_LEVEL, "Could not open %s\n", OUT_FMT_FILE);
      exit(1);
  }

  frame = avcodec_alloc_frame();
  if (!frame) {
      LOGI(LOG_LEVEL, "Could not allocate video frame\n");
      exit(1);
  }
  frame->format = c->pix_fmt;
  frame->width  = c->width;
  frame->height = c->height;

  /* the image can be allocated by any means and av_image_alloc() is
   * just the most convenient way if av_malloc() is to be used */
  ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
                       c->pix_fmt, 32);
  if (ret < 0) {
      LOGI(LOG_LEVEL, "Could not allocate raw picture buffer\n");
      exit(1);
  }
  
  /* allocate and init a re-usable frame */
  AVFrame* frame_pic = avcodec_alloc_frame();
  AVPicture picture_pic;
  if (!frame_pic) {
    LOGI(LOG_LEVEL, "[output]Could not allocate video frame\n");
    return 1;
  }
  if (avpicture_alloc(&picture_pic, c->pix_fmt,
                      c->width, c->height) < 0) {
    LOGI(LOG_LEVEL, "[output]Could not allocate video picture\n");
    return 1;
  }
  if (c->pix_fmt != PIX_FMT_YUV420P) {
    LOGI(LOG_LEVEL, "[output]pix_fmt: %d\n", c->pix_fmt);
    ret = avpicture_alloc(&picture_pic, PIX_FMT_YUV420P,
                          c->width,
                          c->height);
    if (ret < 0) {
      LOGI(LOG_LEVEL, "[output]Could not allocate temporary picture\n");
      return 1;
    }
  }
  /* copy data and linesize picture pointers to frame */
  *((AVPicture *)frame_pic) = picture_pic;

  int outbuf_size = 100000;
  uint8_t *outbuf = malloc(outbuf_size);
  int nbytes = avpicture_get_size(PIX_FMT_YUV420P, c->width, c->height);
  uint8_t* outbuffer = (uint8_t*)av_malloc(nbytes);
  uint8_t *buffer = (uint8_t *)av_malloc(nbytes * sizeof(uint8_t));

  /* encode 1 second of video */
  int i;
  frameCount = 0;
  for(i = frameCount; i > 0; i--) {
      memset(DST_FILE, 0, strlen(DST_FILE));
      strcpy(DST_FILE, TMP_FOLDER);
      strcat(DST_FILE, "//");
      char frame_count_str[8];
      LOGI(LOG_LEVEL, frame_count_str, "%d", i);
      strcat(DST_FILE, frame_count_str);
      strcat(DST_FILE, ".jpg");
      pictureF = fopen(DST_FILE, "wb");
      if (pictureF == NULL) {
        LOGI(LOG_LEVEL, "file is not exist.\n", DST_FILE);
        continue;
      }
      if (fread(buffer, sizeof(char), nbytes, pictureF) <= 0) {
        LOGI(LOG_LEVEL, "file is empty.\n", DST_FILE);
        continue;
      }
      avpicture_fill(&picture_pic, buffer, PIX_FMT_RGB8, c->width, c->height);
      //avpicture_fill((AVPicture*)outpic, outbuffer, PIX_FMT_YUV420P, c->width, c->height);
      fclose(DST_FILE);
      //fdelete(DST_FILE);
      struct SwsContext* fooContext = sws_getContext(c->width, c->height, 
                                                     PIX_FMT_RGB8, 
                                                     c->width, c->height, 
                                                     PIX_FMT_YUV420P, 
                                                     SWS_FAST_BILINEAR, 
                                                     NULL, NULL, NULL);

      //perform the conversion
      sws_scale(fooContext, picture_pic.data, picture_pic.linesize, 
                0, c->height, frame->data, frame->linesize);
      // Here is where I try to convert to YUV
      
      av_init_packet(&pkt);
      pkt.data = NULL;    // packet data will be allocated by the encoder
      pkt.size = 0;

      fflush(stdout);

      frame->pts = frameCount;
      /* encode the image */
      ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
      if (ret < 0) {
          LOGI(LOG_LEVEL, "Error encoding frame\n");
          continue;
      }

      if (got_output) {
          LOGI(LOG_LEVEL, "Write frame %3d (size=%5d)\n", frameCount, pkt.size);
          fwrite(pkt.data, 1, pkt.size, f);
          av_free_packet(&pkt);
      }
  }
  /* add sequence end code to have a real mpeg file */
  fwrite(endcode, 1, sizeof(endcode), f);
  fclose(f);

  avcodec_close(c);
  av_free(c);
  av_freep(&frame->data[0]);
  avcodec_free_frame(&frame);
  av_freep(&frame_pic->data[0]);
  avcodec_free_frame(&frame_pic);
  LOGI(LOG_LEVEL, "Encoding success.\n");
}

int doReverseViaBmp(const char* SRC_FILE, const char* TMP_FOLDER, const char* OUT_FMT_FILE) {
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
  int frameCount = decode2JPG(SRC_FILE, TMP_FOLDER, codecContext);
  return encodeJPG2Video(TMP_FOLDER, frameCount, OUT_FMT_FILE, codecContext);
}

int doReverse2(const char* SRC_FILE, const char* OUT_FILE, const char* OUT_FMT_FILE) {
  AVFormatContext *formatContext_src = NULL;
  AVFormatContext *formatContext_dst = NULL;
  AVStream *st_src = NULL;
  AVStream *st_dst = NULL;
  AVCodec *codec_src = NULL;
  AVCodec *codec_dst = NULL;
  AVFrame *frame_src = NULL;
  AVFrame *frame_dst = NULL;
  AVOutputFormat *outputFormat_dst = NULL;
  AVPicture picture_dst;

  AVPacketList *pktListHeader = NULL;
  AVPacketList *pktListItem = NULL;

  int ret = -1;
  int vst_idx = -1;
  int video_outbuf_size_dst;
  static uint8_t *video_outbuf_dst;
  
  av_register_all();
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
  vst_idx = ret;
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
  LOGI(LOG_LEVEL, "codec_is is %d\n", st_src->codec->codec_id);
  
  codec_src = avcodec_find_decoder(st_src->codec->codec_id);
  if ((ret = avcodec_open2(st_src->codec, codec_src, NULL)) < 0) {
    LOGI(LOG_LEVEL, "Failed to open %s codec\n", 
         av_get_media_type_string(st_src->codec->codec_id));
    goto end;
  }
  LOGI(LOG_LEVEL, "codec name is %s\n", codec_src->name);

  /****************** start of encoder init ************************/
  /* allocate the output media context */
  /* init AVFormatContext */
  avformat_alloc_output_context2(&formatContext_dst, NULL, NULL, OUT_FMT_FILE);
  if (!formatContext_dst) {
    LOGI(LOG_LEVEL, "[output]Could not deduce output format from file extension: using MPEG.\n");
    avformat_alloc_output_context2(&formatContext_dst, NULL, "mpeg", OUT_FMT_FILE);
  }
  if (!formatContext_dst) {
    return 1;
  }
  outputFormat_dst = formatContext_dst->oformat;
  /* init AVCodec */
  enum AVCodecID codec_id = outputFormat_dst->video_codec;
  //enum AVCodecID codec_id = AV_CODEC_ID_MPEG4;
  codec_dst = avcodec_find_encoder(codec_id);
  LOGI(LOG_LEVEL, "[output]Codec is %d\n", codec_id);
  if (!codec_dst) {
    LOGI(LOG_LEVEL, "[output]Could not find codec\n");
    return 1;
  }
  /* init AVStream */
  st_dst = avformat_new_stream(formatContext_dst, codec_dst);
  if (!st_dst) {
    LOGI(LOG_LEVEL, "[output]Could not allocate stream\n");
    return 1;
  }
  st_dst->id = 1;
  //st_dst->codec->bit_rate =
  if (outputFormat_dst->flags & AVFMT_GLOBALHEADER)
    st_dst->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

  /* init AVCodecContext */
  avcodec_get_context_defaults3(st_dst->codec, codec_dst);
  {
    /* init AVCodecContext for open */
    st_dst->codec->codec_id = codec_id;

    /* Put sample parameters. */
    st_dst->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    st_dst->codec->bit_rate = 400000;//st_src->codec->bit_rate;
    /* Resolution must be a multiple of two. */
    st_dst->codec->width    = st_src->codec->width;
    st_dst->codec->height   = st_src->codec->height;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1. */
    st_dst->codec->time_base.den = STREAM_FRAME_RATE;//st_src->codec->time_base.den;
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

  /* open the codec */
  if (avcodec_open2(st_dst->codec, codec_dst, NULL) < 0) {
    LOGI(LOG_LEVEL, "[output]Could not open codec\n");
    return 1;
  }
  /* allocate and init a re-usable frame */
  frame_dst = avcodec_alloc_frame();
  if (!frame_dst) {
    LOGI(LOG_LEVEL, "[output]Could not allocate video frame\n");
    return 1;
  }
  if (avpicture_alloc(&picture_dst, st_dst->codec->pix_fmt,
                      st_dst->codec->width, st_dst->codec->height) < 0) {
    LOGI(LOG_LEVEL, "[output]Could not allocate video picture\n");
    return 1;
  }
  if (st_dst->codec->pix_fmt != PIX_FMT_YUV420P) {
    LOGI(LOG_LEVEL, "[output]pix_fmt: %d\n", st_dst->codec->pix_fmt);
    ret = avpicture_alloc(&picture_dst, PIX_FMT_YUV420P,
                          st_dst->codec->width,
                          st_dst->codec->height);
    if (ret < 0) {
      LOGI(LOG_LEVEL, "[output]Could not allocate temporary picture\n");
      return 1;
    }
  }
  /* copy data and linesize picture pointers to frame */
  *((AVPicture *)frame_dst) = picture_dst;

  //av_dump_format(formatContext_dst, 0, OUT_FMT_FILE, 1);
  /* open the output file, if needed */
  if (!(outputFormat_dst->flags & AVFMT_NOFILE)) {
    if (avio_open(&formatContext_dst->pb, OUT_FMT_FILE, AVIO_FLAG_WRITE) < 0) {
      LOGI(LOG_LEVEL, "[output]Could not open '%s'\n", OUT_FMT_FILE);
      return 1;
    }
  }
  /* Write the stream header, if any. */
  if (avformat_write_header(formatContext_dst, NULL) < 0) {
    LOGI(LOG_LEVEL, "[output]Error occurred when opening output file\n");
    return 1;
  }
  static struct SwsContext *sws_ctx;
  /************ end of init encoder ******************/
  
  /* dump input information to stderr */
  //av_dump_format(formatContext_src, 0, SRC_FILE, 0);
  /* initialize packet, set data to NULL, let the demuxer fill it */
  AVPacket pt_src;
  frame_src = avcodec_alloc_frame();
  
  int frameCount = 0;
  int got_frame = -1, got_output = -1;
  LOGI(LOG_LEVEL, "Start decoding video frame\n");
  while (1) {
    av_init_packet(&pt_src);
    pt_src.data = NULL;
    pt_src.size = 0;

    AVPacket pt_dst;
    av_init_packet(&pt_dst);
    pt_dst.data = NULL;
    pt_dst.size = 0;
    if (av_read_frame(formatContext_src, &pt_src) < 0) {
      break;
    }
    if (pt_src.stream_index == vst_idx) {
      ret = avcodec_decode_video2(st_src->codec, frame_src, &got_frame, &pt_src);
      if (ret < 0) {
        LOGI(LOG_LEVEL, "Error decoding video frame\n");
        return 1;
      }
      if (got_frame) {
        LOGI(LOG_LEVEL, "video_frame n:%d coded_n:%d pts:%s\n",
             frameCount, frame_src->coded_picture_number,
             av_ts2timestr(frame_src->pts, &st_src->codec->time_base));
        /* encode the image */
        {
          /* as we only generate a YUV420P picture, we must convert it
          * to the codec pixel format if needed */
          if (!sws_ctx) {
            sws_ctx = sws_getContext(st_src->codec->width,
                                     st_src->codec->height,
                                     st_src->codec->pix_fmt,
                                     st_dst->codec->width,
                                     st_dst->codec->height,
                                     st_dst->codec->pix_fmt,
                                     SWS_BICUBIC, NULL, NULL, NULL);
            if (!sws_ctx) {
              LOGI(LOG_LEVEL, "Could not initialize the conversion context\n");
              return 1;
            }
          }
          sws_scale(sws_ctx,
                    (const uint8_t * const *)frame_src->data,
                    frame_src->linesize,
                    0,
                    st_dst->codec->height,
                    picture_dst.data,
                    picture_dst.linesize);
        }
        av_free_packet(&pt_src);
        ret = avcodec_encode_video2(st_dst->codec, &pt_dst, frame_dst, &got_output);
        if (ret < 0) {
          LOGI(LOG_LEVEL, "[output]Error encoding frame: %s\n", av_err2str(ret));
          return 1;
        }
        if (got_output) {
          LOGI(LOG_LEVEL, "[output]Write frame %3d (size=%5d)\n", frameCount, pt_dst.size);
          if (st_dst->codec->coded_frame->key_frame) {
            LOGI(LOG_LEVEL, "[output] key_frame \n");
            pt_dst.flags |= AV_PKT_FLAG_KEY;
          }

          LOGI(LOG_LEVEL, "[output] index: %d \n", st_dst->index);
          pt_dst.stream_index = st_dst->index;
#if 1
          /* reverse */
          av_dup_packet(&pt_dst);
          pktListItem = av_malloc(sizeof(AVPacketList));
          pktListItem->pkt = pt_dst;
          pktListItem->next = pktListHeader;
          pktListHeader = pktListItem;
          frameCount++;
#else
          if (st_dst->codec->coded_frame->pts != AV_NOPTS_VALUE) {
            LOGI(LOG_LEVEL, "[output]pts_src: %d\n",
                 st_dst->codec->coded_frame->pts);
            pt_dst.pts = av_rescale_q(st_dst->codec->coded_frame->pts,
                                      st_dst->codec->time_base,
                                      st_dst->time_base);
            LOGI(LOG_LEVEL, "[output]pts_dst: %d, dts: %d\n", pt_dst.pts, pt_dst.dts);
          }
          /* Write the compressed frame to the media file. */
          ret = av_interleaved_write_frame(formatContext_dst, &pt_dst);
          if (ret < 0) {
            LOGI(LOG_LEVEL, "[output] write frame failed: %d \n", ret);
          } else {
            frameCount++;
          }
#endif
        }
      } else {
        LOGI(LOG_LEVEL, "got_frame:%d, n:%d\n", got_frame, frameCount);
      }
      //frame_dst->pts = frameCount;
    } else {
      LOGI(LOG_LEVEL, "Not video frame.\n");
    }
  }
#if 1
  LOGI(LOG_LEVEL, "...start write to file...");
  frameCount = 1;
  while (pktListHeader) {
    pktListHeader->pkt.pts = frameCount;
    pktListHeader->pkt.dts = frameCount++;
    ret = av_interleaved_write_frame(formatContext_dst, &pktListHeader->pkt);
    if (ret < 0) {
      LOGI(LOG_LEVEL, "[output] write frame failed: %d \n", ret);
    }
    av_free_packet(&pktListHeader->pkt);
    pktListHeader = pktListHeader->next;
  }
#endif
  av_write_trailer(formatContext_dst);
  LOGI(LOG_LEVEL, "Decoding video frame DONE!\n");
end:
  if (st_dst && st_src->codec) {
    avcodec_close(st_dst->codec);
  }
  if (formatContext_src) {
    avformat_close_input(&formatContext_src);
  }
  if (frame_src) {
    av_free(frame_src);
  }
  if (frame_dst) {
    av_free(frame_dst);
  }
  return 0;
}


int doReverse(const char* SRC_FILE, const char* OUT_FILE, const char* OUT_FMT_FILE) {
  av_register_all();
  AVFormatContext* pFormat = NULL;
  LOGI(LOG_LEVEL, "1. avformat_open_input: %s\n", SRC_FILE);
  if (avformat_open_input(&pFormat, SRC_FILE, NULL, NULL) < 0)
  {
      return 0;
  }
  AVCodecContext* video_dec_ctx = NULL;
  AVCodec* video_dec = NULL;
  LOGI(LOG_LEVEL, "2. avformat_find_stream_info\n");
  if (avformat_find_stream_info(pFormat, NULL) < 0)
  {
      return 0;
  }
  LOGI(LOG_LEVEL, "3. av_dump_format\n");
  av_dump_format(pFormat, 0, SRC_FILE, 0);
  int video_stream_idx = -1;
  LOGI(LOG_LEVEL, "4. open_codec_context for input VIDEO\n");
  if (open_codec_context(&video_stream_idx, pFormat, AVMEDIA_TYPE_VIDEO) < 0)
  {
    return 0;
  }
  video_dec_ctx = pFormat->streams[video_stream_idx]->codec;
  LOGI(LOG_LEVEL, "5. avcodec_find_decoder\n");
  video_dec = avcodec_find_decoder(video_dec_ctx->codec_id);
#if OUTFILE_ENABLED
  AVFormatContext* pOFormat = NULL;
  AVOutputFormat* ofmt = NULL;
  LOGI(LOG_LEVEL, "6. avformat_alloc_output_context2: %s\n", OUT_FILE);
  if (avformat_alloc_output_context2(&pOFormat, NULL, NULL, OUT_FILE) < 0)
  {
      return 0;
  }
  ofmt = pOFormat->oformat;
  LOGI(LOG_LEVEL, "7. avio_open: %s\n", OUT_FILE);
  if (avio_open(&(pOFormat->pb), OUT_FILE, AVIO_FLAG_READ_WRITE) < 0)
  {
      return 0;
  }
  AVCodecContext *video_enc_ctx = NULL;
  AVCodec *video_enc = NULL;
  LOGI(LOG_LEVEL, "8. avcodec_find_encoder: [%d]", ofmt->video_codec);
  video_enc = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
  LOGI(LOG_LEVEL, "%s\n9. avformat_new_stream for H264\n", video_enc == NULL ? "null" : "OK");
  AVStream *video_st = avformat_new_stream(pOFormat, video_enc);
  if (!video_st)
      return 0;
  video_enc_ctx = video_st->codec;
  video_enc_ctx->width = video_dec_ctx->width;
  video_enc_ctx->height = video_dec_ctx->height;
  video_enc_ctx->pix_fmt = video_dec_ctx->pix_fmt;//PIX_FMT_YUV420P;
  video_enc_ctx->time_base.num = video_dec_ctx->time_base.num;//1;
  video_enc_ctx->time_base.den = video_dec_ctx->time_base.den;//25;
  video_enc_ctx->bit_rate = video_dec_ctx->bit_rate;
  video_enc_ctx->gop_size = video_dec_ctx->gop_size;//250;
  video_enc_ctx->max_b_frames = video_dec_ctx->max_b_frames;//10;
  //H264 
  //pCodecCtx->me_range = video_dec_ctx->pix_fmt;//16; 
  //pCodecCtx->max_qdiff = video_dec_ctx->pix_fmt;//4; 
  video_enc_ctx->qmin = video_dec_ctx->qmin;//10;
  video_enc_ctx->qmax = video_dec_ctx->qmax;//51;
  LOGI(LOG_LEVEL, "10. avcodec_open2: H264\n");
  if (avcodec_open2(video_enc_ctx, video_enc, NULL) < 0)
  {
      LOGI(LOG_LEVEL, "11. open encode error\n");
      return 0;
  }
  LOGI(LOG_LEVEL, "12. Output264video Information====================\n");
  av_dump_format(pOFormat, 0, OUT_FILE, 1);
#endif

  //mp4 file
  AVFormatContext* pMp4Format = NULL;
  AVOutputFormat* pMp4OFormat = NULL;
  LOGI(LOG_LEVEL, "13. avformat_alloc_output_context2: %s\n", OUT_FMT_FILE);
  if (avformat_alloc_output_context2(&pMp4Format, NULL, NULL, OUT_FMT_FILE) < 0)
  {
      return 0;
  }
  pMp4OFormat = pMp4Format->oformat;
#if 0
  pMp4OFormat->video_codec = AV_CODEC_ID_MPEG4;
  AVCodec *video_enc_mp4 = NULL;
  AVStream *video_st_mp4;
  if (pMp4OFormat->video_codec != AV_CODEC_ID_NONE) {
    video_st_mp4 = add_stream(pMp4Format, &video_enc_mp4, 
                              pMp4OFormat->video_codec,
                              video_dec_ctx);
  }
  if (video_st_mp4) {
    //open_video(pMp4Format, video_enc_mp4, video_st_mp4);
    /* open the codec */
    LOGI(LOG_LEVEL, "13.5. avcodec_open2: mp4\n");
    if (avcodec_open2(video_st_mp4->codec, video_enc_mp4, NULL) < 0) {
      LOGI(LOG_LEVEL, "Could not open video codec\n");
      return 0;
    }
  }
#endif
  LOGI(LOG_LEVEL, "14. avio_open: mp4\n");
  if (avio_open(&(pMp4Format->pb), OUT_FMT_FILE, AVIO_FLAG_READ_WRITE) < 0)
  {
      return 0;
  }

  int i;
  for (i = 0; i < pFormat->nb_streams; i++) {
      AVStream *in_stream = pFormat->streams[i];
      LOGI(LOG_LEVEL, "15. avformat_new_stream: %d\n", i);
      AVStream *out_stream = avformat_new_stream(pMp4Format, in_stream->codec->codec);
      if (!out_stream) {
          return 0;
      }
      int ret = 0;
      LOGI(LOG_LEVEL, "16. avcodec_copy_context\n");
      ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
      if (ret < 0) {
          LOGI(LOG_LEVEL, "Failed to copy context from input to output stream codec context\n");
          return 0;
      }
      out_stream->codec->codec_tag = 0;
      if (pMp4Format->oformat->flags & AVFMT_GLOBALHEADER)
          out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  LOGI(LOG_LEVEL, "17. av_dump_format:%s\n", OUT_FMT_FILE);
  av_dump_format(pMp4Format, 0, OUT_FMT_FILE, 1);

  LOGI(LOG_LEVEL, "18. avformat_write_header for mp4\n");
  if (avformat_write_header(pMp4Format, NULL) < 0)
  {
    return 0;
  }

  av_opt_set(video_enc_ctx->priv_data, "preset", "superfast", 0);
  av_opt_set(video_enc_ctx->priv_data, "tune", "zerolatency", 0);
#if OUTFILE_ENABLED
  LOGI(LOG_LEVEL, "19. avformat_write_header for H264\n");
  avformat_write_header(pOFormat, NULL);
#endif
  AVPacket pkt;
  AVPacket tmppkt;
  av_init_packet(&tmppkt);
  av_init_packet(&pkt);
  AVFrame *pFrame = avcodec_alloc_frame();
  int ts = 0;
  while (1)
  {
    LOGI(LOG_LEVEL, "20. av_read_frame\n");
    if (av_read_frame(pFormat, &pkt) < 0)
    {
#if OUTFILE_ENABLED
      avio_close(pOFormat->pb);
#endif
      av_write_trailer(pMp4Format);
      avio_close(pMp4Format->pb);
      return 0;
    }
    if (pkt.stream_index == 0)
    {
      LOGI(LOG_LEVEL, "21. stream_index : %d\n", pkt.stream_index);
      int got_picture = 0, ret = 0;
      LOGI(LOG_LEVEL, "22. avcodec_decode_video2\n");
      ret = avcodec_decode_video2(video_dec_ctx, pFrame, &got_picture, &pkt);
      if (ret < 0)
      {
        return 0;
      }
      LOGI(LOG_LEVEL, "23. frame pts: %d\n", pFrame->pts);
      pFrame->pts = pFrame->pkt_pts;//ts++;
      if (got_picture)
      {
        int size = video_enc_ctx->width * video_enc_ctx->height * 3 / 2;
        char* buf = av_malloc(size);//new char[size];
        memset(buf, 0, size);
        tmppkt.data = (uint8_t*)buf;
        tmppkt.size = size;
        LOGI(LOG_LEVEL, "24. avcodec_encode_video2\n");
        ret = avcodec_encode_video2(video_enc_ctx, &tmppkt, pFrame, &got_picture);
        if (ret < 0)
        {
#if OUTFILE_ENABLED
          avio_close(pOFormat->pb);
#endif
          av_free(buf);
          return 0;
        }
        if (got_picture)
        {
          LOGI(LOG_LEVEL, "25. av_interleaved_write_frame\n");
          //ret = av_interleaved_write_frame(pOFormat, tmppkt);
          AVStream *in_stream = pFormat->streams[pkt.stream_index];
          AVStream *out_stream = pMp4Format->streams[pkt.stream_index];

          tmppkt.pts = av_rescale_q_rnd(tmppkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
          tmppkt.dts = av_rescale_q_rnd(tmppkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
          tmppkt.duration = av_rescale_q(tmppkt.duration, in_stream->time_base, out_stream->time_base);
          tmppkt.pos = -1;
          ret = av_interleaved_write_frame(pMp4Format, &tmppkt);
          if (ret < 0)
              return 0;
          av_free(buf);
        }
      }
      //avcodec_free_frame(&pFrame);
    }
    else if (pkt.stream_index == 1)
    {
      LOGI(LOG_LEVEL, "26. stream_index : %d\n", pkt.stream_index);
      AVStream *in_stream = pFormat->streams[pkt.stream_index];
      AVStream *out_stream = pMp4Format->streams[pkt.stream_index];

      pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
      pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
      pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
      pkt.pos = -1;
      LOGI(LOG_LEVEL, "27. av_interleaved_write_frame\n");
      if (av_interleaved_write_frame(pMp4Format, &tmppkt) < 0)
        return 0;
    }
  }
  LOGI(LOG_LEVEL, "28. avcodec_free_frame\n");
  avcodec_free_frame(&pFrame);
  LOGI(LOG_LEVEL, "29. DONE...............\n");
  return 0;
}

int reverse(char *file_path_src, char *file_path_desc,
            long positionUsStart, long positionUsEnd,
            int video_stream_no, int audio_stream_no,
            int subtitle_stream_no) {
  const char *OUT_FILE = "/sdcard/outfile.h264";
  const char *OUT_FMT_FILE = "/sdcard/outfmtfile.mp4";
  const char *TMP_FOLDER = "/sdcard/test";
  const char *MUX_TEST_FILE = "/sdcard/mux_test_file.mp4";
  const char *VIDEO_ENCODING_TEST_FILE = "/sdcard/video_encoding.mp4";
  //doReverse2(file_path_src, OUT_FILE, OUT_FMT_FILE);
  //mux(MUX_TEST_FILE);
  //video_encode_example(VIDEO_ENCODING_TEST_FILE, AV_CODEC_ID_MPEG1VIDEO);
  doReverseViaBmp(file_path_src, TMP_FOLDER, OUT_FMT_FILE);
  return;
  
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
  }

  /* dump input information to stderr */
  av_dump_format(fmt_ctx, 0, src_filename, 0);
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
  
  av_init_packet(&pkt_o);
  pkt_o.data = NULL;
  pkt_o.size = 0;

  if (video_stream)
      LOGI(LOG_LEVEL, "Demuxing video from file '%s' into '%s'\n", src_filename, video_dst_filename);
  if (audio_stream)
      LOGI(LOG_LEVEL, "Demuxing video from file '%s' into '%s'\n", src_filename, audio_dst_filename);

  codec_o = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!codec_o) {
    LOGI(LOG_LEVEL, "Codec not found\n");
    exit(1);
  }

  video_enc_ctx = avcodec_alloc_context3(codec_o);
  video_enc_ctx->bit_rate = video_dec_ctx->bit_rate;
  video_enc_ctx->width = video_dec_ctx->width;
  video_enc_ctx->height = video_dec_ctx->height;
  video_enc_ctx->time_base = video_dec_ctx->time_base;
  video_enc_ctx->gop_size = video_dec_ctx->gop_size;
  video_enc_ctx->max_b_frames = video_dec_ctx->max_b_frames;
  video_enc_ctx->pix_fmt = video_dec_ctx->pix_fmt;
  video_enc_ctx->priv_data = video_dec_ctx->priv_data;
  /* open it */
  if (avcodec_open2(video_enc_ctx, codec_o, NULL) < 0) {
    LOGI(LOG_LEVEL, "Could not open codec\n");
    exit(1);
  }

  /* read frames from the file */
  int frameCount = 0;
  while (av_read_frame(fmt_ctx, &pkt) >= 0) {
    // decode pkt to frame
    decode_packet(&got_frame, frame, 0, frameCount, 0);
    if (got_frame) {
      encode_frame_to_dst(frame, frameCount);
    }
    frameCount++;
  }

  /* flush cached frames */
  pkt.data = NULL;
  pkt.size = 0;
  do {
      decode_packet(&got_frame, frame, 1, frameCount, 0);
      if (got_frame) {
      encode_frame_to_dst(frame, frameCount);
    }
    frameCount++;
  } while (got_frame);
  LOGI(LOG_LEVEL, "Demuxing succeeded.\n");


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

void encode_frame_to_dst(AVFrame *frame, int frameCount) {
  /* write to another file */
  av_init_packet(&pkt_o);
  pkt_o.data = NULL;    // packet data will be allocated by the encoder
  pkt_o.size = 0;
  fflush(stdout);

  int x = 0, y = 0;
  
  /* prepare a dummy image */
  /* Y */
  for(y=0;y<video_enc_ctx->height;y++) {
      for(x=0;x<video_enc_ctx->width;x++) {
          frame->data[0][y * frame->linesize[0] + x] = x + y + frameCount * 3;
      }
  }

  /* Cb and Cr */
  x = y = 0;
  for(y=0;y<video_enc_ctx->height/2;y++) {
      for(x=0;x<video_enc_ctx->width/2;x++) {
          frame->data[1][y * frame->linesize[1] + x] = 128 + y + frameCount * 2;
          frame->data[2][y * frame->linesize[2] + x] = 64 + x + frameCount * 5;
      }
  }

  frame->pts = frameCount;
  /* encode the image */
  int got_output;
  ret = avcodec_encode_video2(video_enc_ctx, &pkt_o, frame, &got_output);
  if (ret < 0) {
      LOGI(LOG_LEVEL, "Error encoding frame\n");
      exit(1);
  }

  if (got_output) {
      LOGI(LOG_LEVEL, "Write frame %3d (size=%5d)\n", frameCount, pkt_o.size);
      fwrite(pkt_o.data, 1, pkt_o.size, video_dst_file);
      av_free_packet(&pkt);
  }
}

int open_codec_context(int *stream_idx, AVFormatContext *fmt_ctx, enum AVMediaType type) {
  int ret;
  AVStream *st;
  AVCodecContext *dec_ctx = NULL;
  AVCodec *dec = NULL;

  LOGI(LOG_LEVEL, "av_find_best_stream\n");
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
    LOGI(LOG_LEVEL, "avcodec_find_decoder\n");
    dec = avcodec_find_decoder(dec_ctx->codec_id);
    if (!dec) {
      LOGI(LOG_LEVEL, "Failed to find %s codec\n", av_get_media_type_string(type));
      return ret;
    }
    LOGI(LOG_LEVEL, "avcodec_open2\n");
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
      LOGI(LOG_LEVEL, "Failed to open %s codec\n", av_get_media_type_string(type));
      return ret;
    }
    return 0;
  }
}

int decode_packet(int *got_frame, AVFrame *frame, 
                  int cached, int frameCount,
                  int writePictureToFile) {
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

      if (writePictureToFile) {
        /* copy decoded frame to dest buffer */
        av_image_copy(video_dst_data, video_dst_linesize,
                      (const uint8_t **)(frame->data),
                      frame->linesize,
                      video_dec_ctx->pix_fmt,
                      video_dec_ctx->width,
                      video_dec_ctx->height);
        /* write to rawvideo file */
        fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
      }
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

