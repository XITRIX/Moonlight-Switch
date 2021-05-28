#include "DKVideoRenderer.hpp"
#include <borealis.hpp>
//#include <borealis/platforms/switch/switch_video.hpp>

//int OperationFrame_EncodeAndWrite_Inner_SaveJpg(AVFrame *pFrame, const char *out_file) {
//    int width = pFrame->width;
//    int height = pFrame->height;
//
//    AVFormatContext* pFormatCtx = avformat_alloc_context();
//
//    pFormatCtx->oformat = av_guess_format("mjpeg", NULL, NULL);
//    if( avio_open(&pFormatCtx->pb, out_file, AVIO_FLAG_READ_WRITE) < 0) {
//        printf("Couldn't open output file.");
//        return -1;
//    }
//
//    AVStream* pAVStream = avformat_new_stream(pFormatCtx, 0);
//    if( pAVStream == NULL ) {
//        return -1;
//    }
//
//    AVCodecContext* pCodecCtx = pAVStream->codec;
//
//    pCodecCtx->codec_id = pFormatCtx->oformat->video_codec;
//    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
//    pCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
//    pCodecCtx->width = width;
//    pCodecCtx->height = height;
//    pCodecCtx->time_base.num = 1;
//    pCodecCtx->time_base.den = 25;
//
//    // Begin Output some information
//    av_dump_format(pFormatCtx, 0, out_file, 1);
//    // End Output some information
//
//    AVCodec* pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
//    if( !pCodec ) {
//        printf("Codec not found.");
//        return -1;
//    }
//    if( avcodec_open2(pCodecCtx, pCodec, NULL) < 0 ) {
//        printf("Could not open codec.");
//        return -1;
//    }
//
//    //Write Header
//    avformat_write_header(pFormatCtx, NULL);
//
//    int y_size = pCodecCtx->width * pCodecCtx->height;
//
//    AVPacket pkt;
//    av_new_packet(&pkt, y_size * 3);
//
//    //
//    int got_picture = 0;
//    int ret = avcodec_encode_video2(pCodecCtx, &pkt, pFrame, &got_picture);
//    if( ret < 0 ) {
//        printf("Encode Error.\n");
//        return -1;
//    }
//    if( got_picture == 1 ) {
//        //pkt.stream_index = pAVStream->index;
//        ret = av_write_frame(pFormatCtx, &pkt);
//    }
//
//    av_free_packet(&pkt);
//
//    //Write Trailer
//    av_write_trailer(pFormatCtx);
//
//    if( pAVStream ) {
//        avcodec_close(pAVStream->codec);
//    }
//    avio_close(pFormatCtx->pb);
//    avformat_free_context(pFormatCtx);
//
//    return 0;
//}

int save_frame_as_jpeg(AVFrame *pFrame, int FrameNo) {
//    avcodec_init();
    AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!jpegCodec) {
        brls::Logger::error("DKVideoRenderer: error opening this shit 1");
        return -1;
    }
    AVCodecContext *jpegContext = avcodec_alloc_context3(jpegCodec);
    if (!jpegContext) {
        brls::Logger::error("DKVideoRenderer: error opening this shit 2");
        return -1;
    }

    jpegContext->codec_type = AVMEDIA_TYPE_VIDEO;
    jpegContext->pix_fmt = AV_PIX_FMT_RGB24;
    jpegContext->width = pFrame->width;
    jpegContext->height = pFrame->height;
    jpegContext->time_base.num = 1;
    jpegContext->time_base.den = 25;

    if (avcodec_open2(jpegContext, jpegCodec, NULL) < 0) {
        brls::Logger::error("DKVideoRenderer: error opening this shit 3");
        return -1;
    }
    FILE *JPEGFile;
    char JPEGFName[256];

    AVPacket packet = {.data = NULL, .size = 0};
    av_init_packet(&packet);
    int gotFrame;

    if (avcodec_encode_video2(jpegContext, &packet, pFrame, &gotFrame) < 0) {
        brls::Logger::error("DKVideoRenderer: error opening this shit 4");
        return -1;
    }

    
#ifdef __SWITCH__
    sprintf(JPEGFName, "sdmc:/switch/moonlight/test.jpg");
#else
    sprintf(JPEGFName, "moonlight-nx/test.jpg");
#endif
    JPEGFile = fopen(JPEGFName, "wb");
    fwrite(packet.data, 1, packet.size, JPEGFile);
    fclose(JPEGFile);

    av_free_packet(&packet);
    avcodec_close(jpegContext);
    return 0;
}

void DKVideoRenderer::draw(NVGcontext* vg, int width, int height, AVFrame *frame)
{

//    OperationFrame_EncodeAndWrite_Inner_SaveJpg(frame, "test.jpeg");
    save_frame_as_jpeg(frame, 0);

//    brls::async([width, height, frame] {
        
#ifdef __SWITCH__
    int image = nvgCreateImage(vg, "sdmc:/switch/moonlight/test.jpg", 0);
#else
    int image = nvgCreateImage(vg, "moonlight-nx/test.jpg", 0);
#endif
     NVGpaint paint = nvgImagePattern(vg, 0, 0, width, height, 0, image, 1.0f);

     nvgBeginPath(vg);
     nvgRect(vg, 0, 0, width, height);
     nvgFillPaint(vg, paint);
     nvgFill(vg);
//    });
}

VideoRenderStats* DKVideoRenderer::video_render_stats()
{
    m_video_render_stats.rendered_fps = (float)m_video_render_stats.rendered_frames / ((float)(LiGetMillis() - m_video_render_stats.measurement_start_timestamp) / 1000);
    return (VideoRenderStats*)&m_video_render_stats;
}

DKVideoRenderer::~DKVideoRenderer()
{

}
