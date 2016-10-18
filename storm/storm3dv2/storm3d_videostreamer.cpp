#include <stdint.h>
#include <vector>
#include <string>

#include <istorm3d_streambuffer.h>
#include "igios.h"
#include "storm3d_videostreamer.h"
#include "storm3d.h"
#include "storm3d_texture.h"
#include "storm3d_material.h"
#include "storm3d_scene.h"
#include "treader.h"

#include "../../system/Timer.h"

#include "../../util/Debug_MemoryManager.h"

#ifdef WIN32
#include <direct.h>
#endif

namespace {

    static const int INTERLACE = 2;

} // unnamed


struct Storm3D_VideoStreamer::Data
{
    Storm3D &storm;
    IStorm3D_StreamBuilder *streamBuilder;

    std::shared_ptr<Storm3D_Texture>  ramTexture;
    std::shared_ptr<Storm3D_Texture>  texture1;
    std::shared_ptr<Storm3D_Texture>  texture2;

    std::shared_ptr<Storm3D_Texture>  activeTexture;
    std::shared_ptr<Storm3D_Material> material;

    uint32_t*    buffer;

    VC2          start;
    VC2          end;
    float        alpha;
    bool         finished;
    bool         looping;

    //CRITICAL_SECTION colorBufferLock;
    bool         useDynamicTextures;

    bool         downscaled;

    int          lastFrame;     // last frame we showed
    uint32_t     startTime;     // time video started
    uint32_t     nextFrameTime;

    uint32_t     width, height; // actual video w&h

    bool         higherColorRange;

    Data(Storm3D &storm_, bool downscale_, bool higherColorRange_)
        :   storm(storm_),
        streamBuilder(0),
        buffer(0),
        alpha(1.f),
        finished(false),
        looping(false),
        useDynamicTextures(true),
        downscaled(downscale_),
        //useDynamicTextures(false),
        lastFrame(-1),
        startTime(0),
        nextFrameTime(0),
        width(0),
        height(0),
        higherColorRange(true)
    {
        // FIXME: do we need downscaled and higherColorRange ?
    }

    ~Data()
    {
        delete[] buffer;
    }

    std::shared_ptr<TReader> test;

    void initialize(const std::string &fileName)
    {
        test = std::shared_ptr<TReader>( new TReader() );
        if ( test->read_info(fileName.c_str(), streamBuilder) )
            return;

        if ( test->init() ) {
            assert(0);
            //igiosWarning("init failed\n");
            return;
        }

        width = test->frame_width;
        height = test->frame_height;

        startTime = SDL_GetTicks();
        nextFrameTime = startTime;
    }

    bool initStormResources()
    {
        if (width == 0 || height == 0)
            return false;

        if (useDynamicTextures) {
            /*
               if(downscaled)
               {
                width /= 2;
                height /= 2;
               }
             */

            IStorm3D_Texture::TEXTYPE textype = IStorm3D_Texture::TEXTYPE_DYNAMIC_LOCKABLE;

            texture1.reset(static_cast<Storm3D_Texture *>(storm.CreateNewTexture(width,
                height,
                textype)),
                [](Storm3D_Texture* texture) { texture->Release(); });
            texture2.reset(static_cast<Storm3D_Texture *>(storm.CreateNewTexture(width,
                height,
                textype)),
                [](Storm3D_Texture* texture) { texture->Release(); });

            activeTexture = texture1;
            if (!texture1 || !texture2)
                return false;
        }
        else {
            ramTexture.reset(new Storm3D_Texture(&storm, width, height,
                IStorm3D_Texture::TEXTYPE_RAM),
                [](Storm3D_Texture* texture) { texture->Release(); });
            texture1.reset(static_cast<Storm3D_Texture *>(storm.CreateNewTexture(width,
                height,
                IStorm3D_Texture::TEXTYPE_DYNAMIC)),
                [](Storm3D_Texture* texture) { texture->Release(); });
            texture2.reset(static_cast<Storm3D_Texture *>(storm.CreateNewTexture(width,
                height,
                IStorm3D_Texture::TEXTYPE_DYNAMIC)),
                [](Storm3D_Texture* texture) { texture->Release(); });

            activeTexture = texture1;
            if (!texture1 || !texture2 || !ramTexture)
                return false;
        }

        material.reset( new Storm3D_Material(&storm, "video material") );
        if (!material)
            return false;

        Storm3D_SurfaceInfo info = storm.GetScreenSize();
        int windowSizeX = info.width;
        int windowSizeY = info.height;

        if (useDynamicTextures)
            material->SetBaseTexture( activeTexture.get() );
        else
            material->SetBaseTexture( activeTexture.get() );

        int x1 = 0;
        int y1 = 0;
        int x2 = windowSizeX;
        int y2 = windowSizeY;

        float textureRatio = float(width) / float(height);
        y2 = int(windowSizeX / textureRatio);
        y1 = (windowSizeY - y2) / 2;
        y2 += y1;

        start = VC2( float(x1), float(y1) );
        end = VC2( float(x2 - x1), float(y2 - y1) );

        return true;
    }

    bool hasVideo() const
    {
        if (!texture1 || !texture2)
            return false;

        return true;
    }
};

Storm3D_VideoStreamer::Storm3D_VideoStreamer(Storm3D                &storm,
                                             const char             *fileName,
                                             IStorm3D_StreamBuilder *streamBuilder,
                                             bool                    loop,
                                             bool                    downscale,
                                             bool                    higherColorRange)
{
    data = new Data(storm, downscale, higherColorRange);
    data->streamBuilder = streamBuilder;
    data->initialize(fileName);

    if ( !data->initStormResources() )
        return;

    data->looping = loop;
}

Storm3D_VideoStreamer::~Storm3D_VideoStreamer()
{
    assert(data);
    delete data;
}

bool Storm3D_VideoStreamer::hasVideo() const
{
    if (!data)
        return false;

    return data->hasVideo();
}

bool Storm3D_VideoStreamer::hasEnded() const
{
    if (!data || data->finished)
        return true;

    return false;
}

int Storm3D_VideoStreamer::getTime() const
{
    if (!data)
        return 0;

    double frame_duration = ( (double)data->test->fps_numerator / data->test->fps_denominator ) * 1000;

    return (int)(data->lastFrame * frame_duration);
}

bool Storm3D_VideoStreamer::isPlaying() const
{
    return !hasEnded();
}

//void Storm3D_VideoStreamer::stop()
//{
//    assert(data);
//
//    data->test->finish();
//    data->test.reset();
//}

IStorm3D_Material *Storm3D_VideoStreamer::getMaterial()
{
    return data->material.get();
}

void Storm3D_VideoStreamer::setPosition(const VC2 &position, const VC2 &size)
{
    data->start = position;
    data->end = size;
}

void Storm3D_VideoStreamer::setAlpha(float alpha)
{
    data->alpha = alpha;
}

void Storm3D_VideoStreamer::update()
{
    if (data == NULL || data->finished || data->width == 0 || data->height == 0)
        return;

    uint32_t currentTime = SDL_GetTicks();

    // check if it's time to do something
    if (currentTime < data->nextFrameTime)
        return;

    if (data->useDynamicTextures) {
        if (data->activeTexture == data->texture1)
            data->activeTexture = data->texture2;
        else
            data->activeTexture = data->texture1;

        data->material->SetBaseTexture( data->activeTexture.get() );

        int height = data->height;
        int width = data->width;
        if ( !data->test->nextframe() ) {
            if (!data->buffer) {
                data->buffer = new uint32_t[width * height];
                memset(data->buffer, 0x00, width * height * 4);
            }
            uint32_t* buf = data->buffer;

            int64_t time;
            if (data->test->read_pixels( (char *)buf, width, height, time ))
            {
                data->nextFrameTime = data->startTime + (uint32_t)(( (double)data->test->fps_numerator / data->test->fps_denominator ) * 1000 * time);
                data->activeTexture->Copy32BitSysMembufferToTexture((DWORD*)buf);
                data->lastFrame++;
            }
        } else if (data->looping) {
            //FIXME: restart video
            data->test->restart();
            data->startTime     = currentTime;
            data->nextFrameTime = currentTime;
            data->lastFrame = 0;
        } else {
            delete[] data->buffer;
            data->buffer = 0;
            data->finished = true;
            data->test->finish();
        }
    }
}

void Storm3D_VideoStreamer::render(IStorm3D_Scene *scene)
{
    if (!data || !scene || data->finished)
        return;

    update();
	scene->Render2D_Picture(data->material.get(), data->start, data->end, data->alpha, 0, 0,0,1,1);

}

void Storm3D_VideoStreamer::getTextureCoords(float &x, float &y)
{
    x = 1.0f; y = 1.0f;
}
