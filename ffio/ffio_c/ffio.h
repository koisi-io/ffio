/**
 * Library : ffio
 * Author : koisi, dongrixinyu
 * License : MIT
 * Email : dongrixinyu.66@gmail.com
 * Github : https://github.com/dongrixinyu/ffio
 * Description : An easy-to-use Python wrapper for FFmpeg-C-API.
 * Website : http://www.jionlp.com
 */

#ifndef FFIO_C_FFIO_H
#define FFIO_C_FFIO_H

#include "ffio_util.h"
#ifdef CHECK_IF_CUDA_IS_AVAILABLE
#include "pix_fmt_conversion.cuh"
#endif

#define MAX_URL_LENGTH                  256
#define FFIO_COLOR_DEPTH                3
#define FFIO_PTS_GAP_TOLERANCE_EVEN     6
#define FFIO_TIME_BASE_MILLIS          (AVRational){1, 1000}

typedef enum FFIOMode {
  FFIO_MODE_DECODE = 0,
  FFIO_MODE_ENCODE
} FFIOMode;

typedef enum FFIOState {
  FFIO_STATE_INIT = 0,         //  Just reset all ffio contents to NULL.
  FFIO_STATE_READY,            //  Succeeded to call initFFIO(). Available for decoding or encoding.
  FFIO_STATE_RUNNING,          //  Normally running. Available for decoding or encoding.
  FFIO_STATE_END,              //  Reached the end of video.
  FFIO_STATE_CLOSED            //  Set by finalizeFFIO().
} FFIOState;

typedef enum FFIOError {
  FFIO_ERROR_FFIO_NOT_AVAILABLE = -100,
  FFIO_ERROR_RECV_FROM_CODEC,
  FFIO_ERROR_SEND_TO_CODEC,
  FFIO_ERROR_READ_OR_WRITE_TARGET,
  FFIO_ERROR_STREAM_EOF,
  FFIO_ERROR_AVFRAME_ALLOCATION,
  FFIO_ERROR_AVFORMAT_FAILURE,
  FFIO_ERROR_AVCODEC_FAILURE,
  FFIO_ERROR_SHM_FAILURE,
  FFIO_ERROR_SWS_FAILURE,
  FFIO_ERROR_HARDWARE_ACCELERATION,
  FFIO_ERROR_WRONG_CODEC_PARAMS,
  FFIO_ERROR_SUCCESS = 0
} FFIOError;

typedef enum FFIOPTSTrick {
  FFIO_PTS_TRICK_EVEN = 0,                   // For     live-streaming scenarios.
  FFIO_PTS_TRICK_INCREASE,                   // For non-live-streaming scenarios.
  FFIO_PTS_TRICK_RELATIVE,                   // If you are calling encodeOneFrame() at a stable rate.
  FFIO_PTS_TRICK_DIRECT                      // Manually set `ffio->pts_anchor` every time before encodeOneFrame().
} FFIOPTSTrick;

typedef struct CodecParams {
  int      width;
  int      height;
  int      bitrate;
  int      max_bitrate;
  int      fps;
  int      gop;
  int      b_frames;
  int      pts_trick;                        // see: enum FFIOPTSTrick & FFIO.get_current_pts()

  char     flags   [24];
  char     flags2  [24];
  char     profile [24];
  char     preset  [24];
  char     tune    [24];
  char     pix_fmt [24];
  char     format  [24];
  char     codec   [24];
  uint8_t  sei_uuid[16];
  bool     use_h264_AnnexB_sei;              // whether to use AnnexB as h.264 NALU format when creating sei frame.
} CodecParams;

typedef enum FFIOFrameType {
  FFIO_FRAME_TYPE_ERROR = -1,
  FFIO_FRAME_TYPE_RGB,
  FFIO_FRAME_TYPE_EOF
} FFIOFrameType;

typedef struct FFIOFrame {
  FFIOFrameType   type;
  FFIOError       err;
  int             width;
  int             height;
  char           *sei_msg;
  int sei_msg_size;
  unsigned char *data;
} FFIOFrame;

#ifdef CHECK_IF_CUDA_IS_AVAILABLE
typedef struct FFIOCudaFrame {

  // image size
  int width;
  int height;
  int *d_width; // for cuda use

  // for yuv2rgb and rgb2yuv use
  unsigned char *d_rgb;
  unsigned char *d_yuv_y;
  unsigned char *d_yuv_uv;

} FFIOCudaFrame;

#endif

typedef struct FFIO FFIO;
struct FFIO{
  FFIOState ffioState;                       // to indicate that if the stream has been opened successfully
  FFIOMode  ffioMode;                        // encode or decode
  int       frameSeq;                        // the sequence number of the current video frame
  bool      hw_enabled;                      // indicate if using the hardware acceleration
  bool      pix_fmt_hw_enabled;              // indicate if using the hardware to accelerate pixel format conversion

  bool      shmEnabled;
  int       shmFd;
  int       shmSize;

  int       videoStreamIndex;                // which stream index to parse in the video
  int       imageWidth;
  int       imageHeight;
  int       imageByteSize;
  double    framerate;

  int64_t   pts_anchor;

  char      targetUrl[MAX_URL_LENGTH];       // the path of mp4 or rtmp, rtsp

  AVFormatContext     *avFormatContext;
  AVCodecContext      *avCodecContext;
  AVCodec             *avCodec;
  AVPacket            *avPacket;
  AVFrame             *avFrame;              // Decode:  codec    -> avFrame -> (hw_enabled? hwFrame) -> rgbFrame
  AVFrame             *hwFrame;              // Encode:  rgbFrame -> avFrame -> (hw_enabled? hwFrame) -> codec
  AVFrame             *rgbFrame;
  struct SwsContext   *swsContext;

  unsigned char       *rawFrame;
  unsigned char       *rawFrameShm;
  unsigned char        sei_buf[MAX_SEI_LENGTH];
  FFIOFrame            frame;
#ifdef CHECK_IF_CUDA_IS_AVAILABLE
  FFIOCudaFrame       *cudaFrame;
#endif

  AVBufferRef         *hwContext;
  enum AVPixelFormat   hw_pix_fmt;
  enum AVPixelFormat   sw_pix_fmt;

  CodecParams         *codecParams;
  int64_t              time_start_at;
  // struct Clicker      *clicker;

  int64_t (*get_current_pts)(FFIO *);
};

// Functions of FFIO lifecycle.
FFIO* newFFIO();
int initFFIO(
    FFIO *ffio, FFIOMode mode, const char *streamUrl,
    bool hw_enabled, bool pix_fmt_hw_enabled, const char *hw_device,
    bool enableShm, const char *shmName, int shmSize, int shmOffset,
    CodecParams *codecParams);
FFIO* finalizeFFIO(FFIO* ffio);

/** decode one frame from the online video
 *
 * 1 means failed, 0 means success.
 * the result is stored at ffio->rawFrame or rawFrameShm.
 */
FFIOFrame* decodeOneFrame(FFIO* ffio, const char* sei_filter);
FFIOFrame* decodeOneFrameToShm(FFIO* ffio, int shmOffset, const char* sei_filter);

int encodeOneFrame(FFIO* ffio, unsigned char *RGBImage, const char* seiMsg, uint32_t seiMsgSize);
bool encodeOneFrameFromShm(FFIO* ffio, int shmOffset,   const char* seiMsg, uint32_t seiMsgSize);

#endif //FFIO_C_FFIO_H
