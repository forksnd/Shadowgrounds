#include "treader.h"

#include "../../system/Logger.h"
#include "../../system/Miscellaneous.h"

#include <istorm3d_streambuffer.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

static const unsigned int MAX_BUFFERED_FRAMES = 50;

VideoBackgroundLoader::VideoBackgroundLoader()
    : mContext(0),
    mState(VideoBackgroundLoader::STOPPED),
    quitRequested(false),
    backgroundReaderWaiting(false),
    toRGB_convert_ctx(NULL),
    sws_flags(SWS_BICUBIC),
    mThread(0),
    eof(0)
{
    backgroundMutex = SDL_CreateMutex();
    framesFullCVar  = SDL_CreateCond();
    static bool avinit = false;
    if (!avinit) {
        av_register_all();
        av_log_set_level(AV_LOG_QUIET);
        avinit = true;
    }
    mContext = new avctx;
}

VideoBackgroundLoader::~VideoBackgroundLoader()
{
    stop();
    if (mContext->resamplerctx) swr_free(&mContext->resamplerctx);
    if (mContext->audioframe) av_frame_free(&mContext->audioframe);
    if (mContext->audiobuffer) delete[] mContext->audiobuffer;
    if (mContext->audioopen) avcodec_close(mContext->audiocodecctx);
    if (mContext->readframe) av_frame_free(&mContext->readframe);
    if (mContext->drawframe) av_frame_free(&mContext->drawframe);
    if (mContext->drawbuffer) delete[] mContext->drawbuffer;
    mContext->frames.clear();
    if (mContext->videoopen) avcodec_close(mContext->videocodecctx);
    if (mContext->fileopen) avformat_close_input(&mContext->formatctx);

    if (toRGB_convert_ctx != NULL) {
        sws_freeContext(toRGB_convert_ctx);
        toRGB_convert_ctx = NULL;
    }

    SDL_DestroyCond(framesFullCVar);
    SDL_DestroyMutex(backgroundMutex);
    delete mContext;
    mContext = NULL;
}

bool VideoBackgroundLoader::init(const char *filename, IStorm3D_StreamBuilder *builder)
{
    if (avformat_open_input(&mContext->formatctx, filename, 0, 0) != 0) {
        //LOG_WARNING( strPrintf("Failed to load video '%s'.", filename).c_str() );
        //assert(0);
        return false;
    } else { mContext->fileopen = true; }

    if (avformat_find_stream_info(mContext->formatctx, 0) < 0) {
        //LOG_WARNING("Failed to find stream information.");
        assert(0);
        return false;
    }

    mContext->videoindex = mContext->audioindex = -1;
    for (unsigned int i = 0; i < mContext->formatctx->nb_streams; ++i) {
        if (mContext->formatctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && mContext->videoindex ==
            -1) mContext->videoindex = i;
        else if (mContext->formatctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO && mContext->audioindex ==
                 -1) mContext->audioindex = i;
    }

    if (mContext->videoindex == -1) {
        //LOG_WARNING("No video streams detected.");
        assert(0);
        return false;
    } else { mContext->videocodecctx = mContext->formatctx->streams[mContext->videoindex]->codec; }

    mContext->videocodec = avcodec_find_decoder(mContext->videocodecctx->codec_id);
    if (!mContext->videocodec) {
        //LOG_WARNING("Unable to find suitable video codec.");
        assert(0);
        return false;
    }

    if (mContext->videocodec->capabilities & CODEC_CAP_TRUNCATED)
        mContext->videocodecctx->flags |= CODEC_FLAG_TRUNCATED;

    if (avcodec_open2(mContext->videocodecctx, mContext->videocodec, 0) < 0) {
        //LOG_WARNING("Unable to open video codec.");
        assert(0);
        return false;
    } else { mContext->videoopen = true; }

    if (mContext->videocodecctx->time_base.num > 1000 && mContext->videocodecctx->time_base.den ==
        1) mContext->videocodecctx->time_base.den = 1000;
    mContext->fpsnumerator = mContext->videocodecctx->time_base.num;
    mContext->fpsdenominator = mContext->videocodecctx->time_base.den;

    mContext->videowidth = mContext->videocodecctx->width;
    mContext->videoheight = mContext->videocodecctx->height;

    if ( !( mContext->readframe = av_frame_alloc() ) ) {
        //LOG_WARNING("Unable to allocate read frame.");
        assert(0);
        return false;
    }

    if ( !( mContext->drawframe = av_frame_alloc() ) ) {
        //LOG_WARNING("Unable to allocate draw frame.");
        assert(0);
        return false;
    }
    unsigned int bytes = avpicture_get_size(PIX_FMT_RGB24, mContext->videowidth, mContext->videoheight);
    mContext->drawbuffer = new unsigned char[bytes * 4 + 1];
    if (!mContext->drawbuffer) {
        //LOG_WARNING("Unable to allocate draw buffer.");
        assert(0);
        return false;
    }
    avpicture_fill( (AVPicture *)mContext->drawframe, mContext->drawbuffer, PIX_FMT_RGB24, mContext->videowidth,
                    mContext->videoheight );

    bool stereo = false;
    if (mContext->audioindex != -1 && builder) {
        mContext->audiocodecctx = mContext->formatctx->streams[mContext->audioindex]->codec;
        mContext->audiocodec = avcodec_find_decoder(mContext->audiocodecctx->codec_id);
        if (!mContext->audiocodec) {
            //LOG_WARNING("Unable to find suitable audio codec.");
            assert(0);
        } else {
            if (avcodec_open2(mContext->audiocodecctx, mContext->audiocodec, 0) < 0) {
                //LOG_WARNING("Unable to open audio codec.");
                assert(0);
            } else {
                mContext->audioopen = true;
                mContext->audiobuffersize = (192000 * 3) / 2;
                mContext->audiobuffer = new int16_t[mContext->audiobuffersize];
                stereo = mContext->audiocodecctx->channels>1;
                if (!mContext->audiobuffer) {
                    //LOG_WARNING("Unable to allocate audio buffer.");
                    assert(0);
                } else {
                    builder->setStereo(stereo);
                    builder->setFrequency(mContext->audiocodecctx->sample_rate);
                    builder->setBits(16);
                    mContext->audiostream = builder->getStream();
                    mContext->audiostream->activate();
                }

                int inLayout = (int)av_get_default_channel_layout(mContext->audiocodecctx->channels);
                mContext->resamplerctx = swr_alloc_set_opts(
                    NULL, stereo?AV_CH_LAYOUT_STEREO:AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_S16, mContext->audiocodecctx->sample_rate,
                    inLayout, mContext->audiocodecctx->sample_fmt, mContext->audiocodecctx->sample_rate,
                    0, NULL
                );
                if (mContext->resamplerctx == NULL)
                {
                    //LOG_WARNING("Unable to allocate resampler context.");
                    return false;
                }

                int ret = swr_init(mContext->resamplerctx);
                if (ret<0)
                {
                    //LOG_WARNING("Unable to initialize resampler context.");
                    return false;
                }

                if ( !( mContext->audioframe = av_frame_alloc() ) ) {
                    //LOG_WARNING("Unable to allocate audio frame.");
                    assert(0);
                    return false;
                }
            }
        }
    }

    return true;
}

void VideoBackgroundLoader::restart()
{
    stop();

    {
        SDL_LockMutex(backgroundMutex);

        mContext->frames.clear();
        if (backgroundReaderWaiting) {
            backgroundReaderWaiting = false;
            SDL_CondSignal(framesFullCVar);
        }
        SDL_UnlockMutex(backgroundMutex);
    }

    av_seek_frame(mContext->formatctx, mContext->videoindex, 0, AVSEEK_FLAG_ANY);
    av_seek_frame(mContext->formatctx, mContext->audioindex, 0, AVSEEK_FLAG_ANY);

    start();
}

bool VideoBackgroundLoader::finished()
{
    SDL_LockMutex(backgroundMutex);
    bool finished = mState == STOPPED || eof && mContext->frames.empty();
    SDL_UnlockMutex(backgroundMutex);

    return finished;
}

int SDLCALL VideoBackgroundLoader::run(void* inst)
{
    assert(inst);
    VideoBackgroundLoader* loader = (VideoBackgroundLoader*)inst;
    loader->startLoadingThread();
    return 0;
}


void VideoBackgroundLoader::start()
{
    SDL_LockMutex(backgroundMutex);
    if (mState == STOPPED) {
        mState = STARTING;
        quitRequested = false;
        eof = false;
        mThread = SDL_CreateThread(&VideoBackgroundLoader::run, "Video decode", this);
    }
    SDL_UnlockMutex(backgroundMutex);
    SDL_Delay(1);
}

void VideoBackgroundLoader::stop()
{
    {
        SDL_LockMutex(backgroundMutex);
        if (mState != STOPPED)
            mState = STOPPING;
        quitRequested = true;

        if (mContext->audiostream) {
            mContext->audiostream->deactivate();
            mContext->audiostream.reset();
        }

        // wake it up if it's sleeping
        if (backgroundReaderWaiting) {
            backgroundReaderWaiting = false;
            SDL_CondSignal(framesFullCVar);
        }
        SDL_UnlockMutex(backgroundMutex);
    }

    if (mThread)
    {
        SDL_WaitThread(mThread, NULL);
        mThread = 0;
    }
    mState = STOPPED;
}

void VideoBackgroundLoader::getVideoInfo(unsigned int *fps_num, unsigned int *fps_den, unsigned int *w, unsigned int *h)
{
    *fps_num = mContext->fpsnumerator;
    *fps_den = mContext->fpsdenominator;
    *w = mContext->videowidth;
    *h = mContext->videoheight;
}

bool VideoBackgroundLoader::readFrame(char *buffer, const unsigned int w, const unsigned int h, int64_t& time)
{
    boost::shared_array<unsigned char> frame;
    {
        SDL_LockMutex(backgroundMutex);
        if ( mContext->frames.size() ) {
            frame = mContext->frames.front().data;
            time  = mContext->frames.front().time;
            mContext->frames.pop_front();

            // wake the reader up if it was sleeping
            if (backgroundReaderWaiting) {
                backgroundReaderWaiting = false;
                SDL_CondSignal(framesFullCVar);
            }
        } else {
            // no frames to get
            SDL_UnlockMutex(backgroundMutex);
            return false;
        }
        SDL_UnlockMutex(backgroundMutex);
    }

    uint32_t *buf1 = (uint32_t *)buffer;
    uint8_t *buf2 = frame.get();
    unsigned int sh = mContext->videoheight, sw = mContext->videowidth;

    for (unsigned int y = 0; y < sh; ++y) {
        for (unsigned int x = 0; x < sw; ++x) {
            buf1[y * w + x] = 0xff000000 | *(uint32_t *)( buf2 + (y * sw * 3 + x * 3) );
        }
    }
    return true;
}

void VideoBackgroundLoader::startLoadingThread()
{
    {
        SDL_LockMutex(backgroundMutex);
        if (mState != STARTING) {
            mState = STOPPING;
            SDL_UnlockMutex(backgroundMutex);
            return;
        }
        mState = RUNNING;
        SDL_UnlockMutex(backgroundMutex);
    }
    AVPacket packet;

    while (!eof) {
        {
            SDL_LockMutex(backgroundMutex);

            if (quitRequested)
            {
                mState = STOPPING;
                SDL_UnlockMutex(backgroundMutex);
                return;
            }

            // move from initial or stopping to stopped
            if (mState != RUNNING) {
                SDL_UnlockMutex(backgroundMutex);
                return;
            }
            SDL_UnlockMutex(backgroundMutex);
        }

        eof = av_read_frame(mContext->formatctx, &packet) < 0;
        if (!eof) {
            if (packet.stream_index == mContext->videoindex) {
                int framedone = 0;
                avcodec_decode_video2(mContext->videocodecctx, mContext->readframe, &framedone, &packet);
                if (framedone) {
                    size_t bsize = (mContext->videowidth * mContext->videoheight * 3 + 4);
                    boost::shared_array<unsigned char> buffer(new unsigned char[bsize]);
                    if (buffer) {
                        toRGB_convert_ctx = sws_getCachedContext(toRGB_convert_ctx,
                                                 mContext->videowidth, mContext->videoheight,
                                                 mContext->videocodecctx->pix_fmt,
                                                 mContext->videowidth, mContext->videoheight,
                                                 PIX_FMT_BGR24, //NOTE: DX9 only!!!
                                                 sws_flags, NULL, NULL, NULL);

                        sws_scale(toRGB_convert_ctx,
                                  mContext->readframe->data,
                                  mContext->readframe->linesize,
                                  0, mContext->videoheight,
                                  mContext->drawframe->data,
                                  mContext->drawframe->linesize);
                        unsigned int sh = mContext->videoheight, sw = mContext->videowidth;
                        memcpy(buffer.get(), mContext->drawframe->data[0], sw * sh * 3);

                        {
                            SDL_LockMutex(backgroundMutex);
                            avctx::Frame f = {buffer, packet.dts};
                            mContext->frames.push_back(f);

                            if (quitRequested)
                            {
                                mState = STOPPING;
                                SDL_UnlockMutex(backgroundMutex);
                                return;
                            }

                            // if too many buffered frames, sleep
                            if (mContext->frames.size() >= MAX_BUFFERED_FRAMES) {
                                backgroundReaderWaiting = true;
                                while (backgroundReaderWaiting)
                                {
                                    SDL_CondWait(framesFullCVar, backgroundMutex);
                                }
                                backgroundReaderWaiting = false;
                            }
                            SDL_UnlockMutex(backgroundMutex);
                        }
                    }
                }
            } else if (packet.stream_index == mContext->audioindex && mContext->audiobuffer) {
                int frameDone;
                avcodec_decode_audio4(mContext->audiocodecctx, mContext->audioframe, &frameDone, &packet);

                if(frameDone)
                {
                    int srcNSamples = mContext->audioframe->nb_samples;
                    int dstNSamples = mContext->audiobuffersize / 2;

                    if (mContext->audiocodecctx->channels>1)
                    {
                        dstNSamples /= 2;
                    }

                    uint8_t**  src = mContext->audioframe->extended_data;
                    uint8_t*   dst = (uint8_t*)mContext->audiobuffer;
                    dstNSamples = swr_convert(mContext->resamplerctx, &dst, dstNSamples, (const uint8_t**)src, srcNSamples);

                    int size = dstNSamples*2/*sizeof(int16_t)*/; //NOTE: hardcode!!!
                    if (mContext->audiocodecctx->channels>1)
                    {
                        size *= 2;
                    }
                    unsigned long long duration =
                        (unsigned long long)( (double(mContext->audiocodecctx->time_base.num)
                                               / mContext->audiocodecctx->time_base.den) * 1000 );
                    mContext->audiostream->addSample( (char *)mContext->audiobuffer, size, mContext->audiotime, duration );
                    mContext->audiotime += duration;
                }
            }
            av_free_packet(&packet);
        } else { break; }
    }
    {
        SDL_LockMutex(backgroundMutex);
        mState = STOPPING;
        SDL_UnlockMutex(backgroundMutex);
    }
}

TReader::TReader()
{
    mLoader = new VideoBackgroundLoader();
    fps_numerator = 1;
    fps_denominator = 1;
    frame_width = 1;
    frame_height = 1;
}

TReader::~TReader()
{
    finish();
    delete mLoader;
}

int TReader::init()
{
    return false;
}

int TReader::read_info(const char *filename, IStorm3D_StreamBuilder *builder)
{
    if ( !mLoader->init(filename, builder) ) return true;
    mLoader->getVideoInfo(&fps_numerator, &fps_denominator, &frame_width, &frame_height);
    mLoader->start();
    return false;
}

int TReader::nextframe()
{
    return mLoader->finished();
}

int TReader::read_pixels(char *buffer, unsigned int w, unsigned int h, int64_t& time)
{
    return mLoader->readFrame(buffer, w, h, time);
}

int TReader::finish()
{
    if ( !mLoader->finished() )
        mLoader->stop();
    return 1;
}

void TReader::restart()
{
    mLoader->restart();
}
