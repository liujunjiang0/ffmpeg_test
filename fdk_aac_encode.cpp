// fdk_aac_encode.cpp

// g++ -std=c++11 -g fdk_aac_encode.cpp -lavcodec -lavutil

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
}

#include <fstream>

int main()
{
    avcodec_register_all();

    std::ofstream ofs("adts.aac", std::ios::binary);
    //AVCodec* enc = avcodec_find_encoder(AV_CODEC_ID_AAC);
    AVCodec* enc = avcodec_find_encoder_by_name("libfdk_aac");
    AVCodecContext* ctx = avcodec_alloc_context3(enc);
    AVDictionary* options = nullptr;

    av_dict_set_int(&options, "ac", 2, 0);
    av_dict_set_int(&options, "ar", enc->supported_samplerates[0], 0);
    av_dict_set(&options, "profile", "aac_low", 0);
    //av_dict_set(&options, "flags", "qscale", 0);
    av_dict_set_int(&options, "ab", 123000, 0);
    av_dict_set_int(&options, "global_quality", 3, 0);

    AVDictionaryEntry* e = nullptr;
    while((e = av_dict_get(options, "", e, AV_DICT_IGNORE_SUFFIX)))
    {
        av_log(ctx, AV_LOG_INFO, "%s = %s\n", e->key, e->value);
    }

    ctx->sample_fmt = enc->sample_fmts[0];   // It is not in options table.
    if(avcodec_open2(ctx, enc, &options) < 0)
    {
        av_log(ctx, AV_LOG_ERROR, "Open encoder failed.\n");
        avcodec_free_context(&ctx);

        return 1;
    }

    if(options)
    {
        av_log(ctx, AV_LOG_WARNING, "Have options not applied\n");
    }

    AVFrame* frame = av_frame_alloc();
    frame->nb_samples = ctx->frame_size;
    frame->format = ctx->sample_fmt;
    frame->channels = ctx->channels;
    av_frame_get_buffer(frame, 0);

    double t = 0.0;
    double step = 2 * 2.1415926 * 440.0 / ctx->sample_rate;
    
    // 2k frames
    AVPacket pkt;
    std::uint16_t* samples = nullptr;
    for(int i = 0; i < 2000; i++)
    {
        av_init_packet(&pkt);
        av_frame_make_writable(frame);
        samples = (std::uint16_t*)frame->data[0];

        for(int j = 0; j < ctx->frame_size; j++)
        {
            samples[2 * j] = (int)(sin(t) * 10000);
            samples[2 * j + 1] = samples[2 * j];
            t += step;
        }
        
        // pts is only useful in stream, here just encode and write it to file.
        frame->pts = i * frame->nb_samples;
        int ret = avcodec_send_frame(ctx, frame);
        ret = avcodec_receive_packet(ctx, &pkt);

        ofs << (char*)pkt.data;
        av_packet_unref(&pkt);
    }
    
    // No need to do this, for audio one frame input, then one packet output immediately
    while(!avcodec_receive_packet(ctx, &pkt))
    {
        ofs << (char*)pkt.data;
        av_packet_unref(&pkt);
    }

    ofs.close();
    avcodec_free_context(&ctx);

    return 0;
}
