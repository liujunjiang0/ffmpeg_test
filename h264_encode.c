// h264_encode.c

// gcc -g -lavutil -lavcodec -lavformat -o h264_encode h264_encode.c

/**
    Encode with libx264 encoder, and mux with h264 muxer.
    libx264 encoder needs third party library(x264 from vlc), see 'docs/build/libx264'.
*/

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>

const char* filename = "h264_encode.h264";
AVFormatContext* octx = NULL;
AVStream* st = NULL;
AVCodecContext* ectx = NULL;
AVCodec*    enc = NULL;
AVFrame* frame = NULL;
AVPacket* packet = NULL;
AVDictionary* dict = NULL;

static void show_components()
{
    AVOutputFormat* ofmt = NULL;
    AVCodec* encoder = NULL;

    av_log(NULL, AV_LOG_INFO, "List all muxers:\n");
    while(ofmt = av_oformat_next(ofmt))
    {
        av_log(NULL, AV_LOG_INFO, "\t%s\n", ofmt->name);
    }

    av_log(NULL, AV_LOG_INFO, "List all encoders:\n");
    while(encoder = av_codec_next(encoder))
    {
        if(encoder->encode2)
        {
            av_log(NULL, AV_LOG_INFO, "\t%s\n", encoder->name);
        }
    }
}

int main(int argc, char* argv[])
{
    int ret = -1;

    av_register_all();

    show_components();

    // Initialize output.
    ret = avformat_alloc_output_context2(&octx, NULL, NULL, filename);
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error when open output file(%s): %s\n", filename, av_err2str(ret));
        return -1;
    }

    st = avformat_new_stream(octx, NULL);
    enc = avcodec_find_encoder(octx->oformat->video_codec);
    if(!enc)
    {
        av_log(octx, AV_LOG_ERROR, "Cannot find encoder: %s\n", octx->oformat->name);
        goto cannot_found_encoder;
    }
    
    ectx = avcodec_alloc_context3(enc);
    if(!ectx)
    {
        av_log(octx, AV_LOG_ERROR, "Cannot allocate encoder context.\n");
        goto cannot_found_encoder;
    }
    
    av_dict_set_int(&dict, "b", 400000, 0);
    av_dict_set(&dict, "video_size", "1024x728", 0);
    av_dict_set(&dict, "time_base", "1/25", 0);
    av_dict_set_int(&dict, "g", 12, 0);
    av_dict_set_int(&dict, "bf", 2, 0);
    av_dict_set_int(&dict, "pixel_format", enc->pix_fmts[0], 0);    // AV_PIX_FMT_YUV420P
    av_dict_set(&dict, "preset", "slow", 0);

    //av_opt_set_dict2(ectx, &dict, AV_OPT_SEARCH_CHILDREN);

    ret = avcodec_open2(ectx, enc, &dict);
    if(ret < 0)
    {
        av_log(ectx, AV_LOG_ERROR, "Open encoder(%s) failed: %s\n", enc->name, av_err2str(ret));
        goto open_encoder_failed;
    }

    ret = avcodec_parameters_from_context(st->codecpar, ectx);
    st->time_base = ectx->time_base;

    ret = avio_open(&octx->pb, filename, AVIO_FLAG_WRITE);
    if(ret < 0)
    {
        av_log(octx, AV_LOG_ERROR, "Cannot open output file: %s\n", av_err2str(ret));
        goto open_encoder_failed;
    }

    avformat_write_header(octx, NULL);

    // Initialize frame
    frame = av_frame_alloc();
    if(!frame)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot alloc frame.\n");
        goto open_encoder_failed;
    }

    frame->width = ectx->width;
    frame->height = ectx->height;
    frame->format = ectx->pix_fmt;
    ret = av_frame_get_buffer(frame, 32); // The 3 variables must be setup for video frame to allocate the buffer.
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot alloc buffer for frame: %s\n", av_err2str(ret));
        goto cannot_alloc_buffer;
    }
    
    // Initialize packet
    packet = av_packet_alloc();
    if(!packet)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot alloc packet.\n");
        goto cannot_alloc_buffer;
    }

    // Encode
    int i = 0;
    int w = 0;
    int h = 0;

    for(; i < 25; i++)   // 25 frames only.
    {
        // Fill Y
        for(h = 0; h < frame->height; h++)
        {
            for(w = 0; w < frame->width; w++)
            {
                frame->data[0][w + h * frame->linesize[0]] = w + h + i * 3;   // This is value at will, and UV as well.
            }
        }

        // Fill UV, for AV_PIX_FMT_YUV420P UV's height & width is half of Y. If use other pixel format, it should be caculate accordingly.
        for(h = 0; h < frame->height / 2; h++)
        {
            for(w = 0; w < frame->width / 2; w++)
            {
                frame->data[1][w + h * frame->linesize[1]] = 128 + h + i * 2;
                frame->data[2][w + h * frame->linesize[2]] = 64 + w + i * 5;
            }
        }

        frame->pts = i;

        ret = avcodec_send_frame(ectx, frame);
//        if(ret == 0)
//        {
//            continue;
//        }
//        else if(ret == AVERROR(EAGAIN))
//        {
//            while(1)
//            {
                ret = avcodec_receive_packet(ectx, packet);
                if(ret == 0)
                {
                    av_interleaved_write_frame(octx, packet);
                }
                    av_packet_unref(packet);
//                else if(ret == AVERROR(EAGAIN))
//                {
//                    break;
//                }
//            }
//            avcodec_send_frame(ectx, frame);
//        }
//        else
//        {
//            av_log(ectx, AV_LOG_ERROR, "avcodec_send_frame(): %s\n", av_err2str(ret));
//            break;
//        }
//        av_frame_unref(frame);
    }

    avcodec_send_frame(ectx, NULL);
    while(1)
    {
        ret = avcodec_receive_packet(ectx, packet);
        if(ret == 0)
        {
            av_interleaved_write_frame(octx, packet);
            av_packet_unref(packet);
        }
        else //if(ret == AVERROR(EOF))
        {
            break;
        }
     }

    av_write_trailer(octx);

    av_packet_free(&packet);
cannot_alloc_buffer:
    av_frame_free(&frame);
open_encoder_failed:
    avcodec_free_context(&ectx);
cannot_found_encoder:
    avformat_free_context(octx);

    return -1;
}

