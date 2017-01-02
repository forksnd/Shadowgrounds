#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

#include <vector>
#include <memory>

#include "Storm3D_ProceduralManager.h"
#include "storm3d.h"
#include "storm3d_texture.h"
#include "storm3d_terrain_utils.h"
#include "Storm3D_ShaderManager.h"
#include <IStorm3D_Logger.h>
#include <d3d9.h>

struct TexCoord
{
	VC2 baseFactor;
	VC2 baseOffset;

	VC2 offsetFactor;
	VC2 offsetOffset;

	float timeValue = 0.0f;
};

void clamp(float &value)
{
	float intval = floorf(value);
	if(fabsf(intval) > 1.f)
		value = fmodf(value, intval);

	/*
	if(value > 0)
	{
		float intval = floorf(value);
		if(intval > 1.f)
			value = fmodf(value, intval);
	}
	else
	{
		float intval = floorf(value);
		if(intval < -1.f)
			value = fmodf(value, intval);
	}
	*/
}

void animateSource(TexCoord &coord, const Storm3D_ProceduralManager::Source &source, float timeFactor)
{
	coord.timeValue += timeFactor;
	coord.baseFactor.x = 1.f / source.texture.scale.x;
	coord.baseFactor.y = 1.f / source.texture.scale.y;
	coord.offsetFactor.x = coord.baseFactor.x;
	coord.offsetFactor.y = coord.baseFactor.y;

	coord.baseOffset.x += timeFactor * source.texture.speed.x;
	coord.baseOffset.y += timeFactor * source.texture.speed.y;
	coord.offsetOffset.x = source.radius.x * sinf(coord.timeValue * source.offset.speed.x);
	coord.offsetOffset.y = source.radius.y * cosf(coord.timeValue * source.offset.speed.y);
	coord.offsetOffset.x += source.linearSpeed.x * coord.timeValue;
	coord.offsetOffset.y += source.linearSpeed.y * coord.timeValue;

	clamp(coord.baseOffset.x);
	clamp(coord.baseOffset.y);
	clamp(coord.offsetOffset.x);
	clamp(coord.offsetOffset.y);
}

typedef std::shared_ptr<Storm3D_Texture> ManagedTexture;
ManagedTexture createManagedTexture(Storm3D &storm, const std::string& name)
{
    ManagedTexture mangedTexture;

    if (!name.empty())
    {
        Storm3D_Texture *texture = static_cast<Storm3D_Texture*>(storm.CreateNewTexture(name.c_str()));
        if (texture)
        {
            texture->setAutoRelease(false);
            mangedTexture.reset(texture, [](Storm3D_Texture* tex) { tex->Release(); });
        }
    }

    return mangedTexture;
}

struct ProceduralEffect
{
	Storm3D_ProceduralManager::Effect effect;
	ManagedTexture texture1;
	ManagedTexture texture2;
	ManagedTexture offset1;
	ManagedTexture offset2;
	ManagedTexture distortion1;
	ManagedTexture distortion2;
	ManagedTexture fallback;

	TexCoord coord1;
	TexCoord coord2;
	TexCoord displaceCoord1;
	TexCoord displaceCoord2;

	void init(Storm3D& storm)
	{
        texture1 = createManagedTexture(storm, effect.source1.texture.texture);
        texture2 = createManagedTexture(storm, effect.source2.texture.texture);
        offset1 = createManagedTexture(storm, effect.source1.offset.texture);
        offset2 = createManagedTexture(storm, effect.source2.offset.texture);
        distortion1 = createManagedTexture(storm, effect.distortion1.offset.texture);
        distortion2 = createManagedTexture(storm, effect.distortion2.offset.texture);
        fallback = createManagedTexture(storm, effect.fallback);

		if (effect.fallback.empty())
		{
			storm.getLogger()->warning("ProceduralEffect::init - effect has no fallback texture.");
		}
	}

	void animate(int ms)
	{
		float timeFactor = ms / 1000.f;
		animateSource(coord1, effect.source1, timeFactor);
		animateSource(coord2, effect.source2, timeFactor);

		animateSource(displaceCoord1, effect.distortion1, timeFactor);
		animateSource(displaceCoord2, effect.distortion2, timeFactor);
	}
};

typedef std::map<std::string, ProceduralEffect> ProceduralEffectList;

struct Storm3D_ProceduralManager::Data
{
	Storm3D &storm;
	CComPtr<IDirect3DTexture9> target;
	CComPtr<IDirect3DTexture9> offsetTarget;

	ProceduralEffectList effects;
	std::string active;

	bool distortionMode;
	bool hasDistortion;

	bool useFallback;
    ManagedTexture fallback;

	Data(Storm3D &storm_)
	:	storm(storm_),
		distortionMode(false),
		hasDistortion(false),
		//useFallback(true)
		useFallback(false)
	{
	}

	void addEffect(const std::string &name, const Effect &effect)
	{
		ProceduralEffect result;
		result.effect = effect;
		result.init(storm);

        if (result.texture1 && result.texture2)
        {
            effects[name] = result;
        }
		else
		{
			std::string message = "Cannot find both textures for procedural effect: ";
			message += name;
			storm.getLogger()->error(message.c_str());
			storm.getLogger()->info(result.effect.source1.texture.texture.c_str());
			storm.getLogger()->info(result.effect.source2.texture.texture.c_str());
		}
	}

	void init(CComPtr<IDirect3DTexture9> target_, CComPtr<IDirect3DTexture9> offsetTarget_)
	{
		target = target_;
		offsetTarget = offsetTarget_;
	}

    void setActive(const std::string &name)
    {
        if (name.empty())
        {
            active = name;
            return;
        }

        ProceduralEffectList::iterator it = effects.find(name);
        if (it == effects.end())
            return;

        fallback = it->second.fallback;
        active = name;
    }

    void render(const ProceduralEffect &e, float width, float height, bool offsetTarget)
    {
        GFX_TRACE_SCOPE("Storm3D_ProceduralManager::render");
        gfx::Renderer& renderer = storm.renderer;
        gfx::Device& device = renderer.device;
        gfx::ProgramManager& programManager = renderer.programManager;

        if (e.texture1)
            e.texture1->Apply(1);
        if (e.texture2)
            e.texture2->Apply(3);

        if (offsetTarget)
        {
            if (e.distortion1)
                e.distortion1->Apply(0);
            if (e.distortion2)
                e.distortion2->Apply(2);

            if (!e.distortion1 || !e.distortion2 || !e.effect.enableDistortion)
                return;
        }
        else
        {
            if (e.offset1)
                e.offset1->Apply(0);
            if (e.offset2)
                e.offset2->Apply(2);
        }

        const TexCoord &c1 = (offsetTarget) ? e.displaceCoord1 : e.coord1;
        const TexCoord &c2 = (offsetTarget) ? e.displaceCoord2 : e.coord2;

        float uvScaleBias[4 * 4] = {
            c1.offsetFactor.x, c1.offsetFactor.y, c1.offsetOffset.x, c1.offsetOffset.y,
            c1.baseFactor.x, c1.baseFactor.y, c1.baseOffset.x, c1.baseOffset.y,
            c2.offsetFactor.x, c2.offsetFactor.y, c2.offsetOffset.x, c2.offsetOffset.y,
            c2.baseFactor.x, c2.baseFactor.y, c2.baseOffset.x, c2.baseOffset.y
        };

        device.SetVertexShaderConstantF(0, uvScaleBias, 4);

        float scale1 = (offsetTarget) ? e.effect.distortion1.offset.scale.x : e.effect.source1.offset.scale.x;
        float scale2 = (offsetTarget) ? e.effect.distortion2.offset.scale.x : e.effect.source2.offset.scale.x;

        if (offsetTarget)
        {
            float scale = (scale1 + scale2) * .25f;
            float psUniforms[4 * 3] = {
                scale, scale, scale, scale,
            };
            device.SetPixelShaderConstantF(0, psUniforms, 1);

            programManager.setStdProgram(device, gfx::ProgramManager::PROCEDURAL_DISTORTION);
        }
        else
        {
            float psUniforms[4 * 2] = {
                scale1, 0.0f, 0.0f, scale1,
                scale2, 0.0f, 0.0f, scale2,
            };
            device.SetPixelShaderConstantF(0, psUniforms, 2);

            programManager.setStdProgram(device, gfx::ProgramManager::PROCEDURAL);
        }

        renderer.drawFullScreenQuad();

        device.SetPixelShader(0);
    }

	void update(int ms)
	{
        GFX_TRACE_SCOPE("Storm3D_ProceduralManager::update");
		if(useFallback)
			return;

		gfx::Device &device = storm.GetD3DDevice();
		if(active.empty() || !target || !offsetTarget)
			return;

		ProceduralEffect &e = effects[active];
		e.animate(ms);

		CComPtr<IDirect3DSurface9> renderSurface;
		CComPtr<IDirect3DSurface9> originalSurface;
		target->GetSurfaceLevel(0, &renderSurface);
		if(!renderSurface)
			return;

		device.SetRenderState(D3DRS_ZENABLE, FALSE);
		device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device.GetRenderTarget(0, &originalSurface);

		D3DSURFACE_DESC desc;
		renderSurface->GetDesc(&desc);

        device.SetRenderTarget(0, renderSurface);
        //device.Clear(0, 0, D3DCLEAR_TARGET, 0xFFFFFFFF, 1, 0);
        render(e, float(desc.Width), float(desc.Height), false);

        hasDistortion = distortionMode && e.distortion1 && e.distortion2 && e.effect.enableDistortion;
        if (hasDistortion)
		{
			CComPtr<IDirect3DSurface9> renderSurface;
			offsetTarget->GetSurfaceLevel(0, &renderSurface);

			device.SetRenderTarget(0, renderSurface);
			render(e, float(desc.Width / 2), float(desc.Height / 2), true);
		}

		device.SetRenderState(D3DRS_ZENABLE, TRUE);
		device.SetRenderTarget(0, originalSurface);

		target->GenerateMipSubLevels();
	}

	void enableDistortion(bool enable)
	{
		distortionMode = enable;
	}

	void reset()
	{
		fallback.reset();
		effects.clear();
		if(target)
			target.Release();
		if(offsetTarget)
			offsetTarget.Release();
	}
};

Storm3D_ProceduralManager::Storm3D_ProceduralManager(Storm3D &storm)
{
	data = new Data(storm);
}

Storm3D_ProceduralManager::~Storm3D_ProceduralManager()
{
    assert(data);
    delete data;
}

void Storm3D_ProceduralManager::setTarget(CComPtr<IDirect3DTexture9> &target, CComPtr<IDirect3DTexture9> &offsetTarget)
{
	data->init(target, offsetTarget);
}

void Storm3D_ProceduralManager::addEffect(const std::string &name, const Effect &effect)
{
	data->addEffect(name, effect);
}

void Storm3D_ProceduralManager::enableDistortionMode(bool enable)
{
	data->enableDistortion(enable);
}

void Storm3D_ProceduralManager::useFallback(bool fallback)
{
	data->useFallback = fallback;
}

void Storm3D_ProceduralManager::setActiveEffect(const std::string &name)
{
	data->setActive(name);
}

void Storm3D_ProceduralManager::update(int ms)
{
	data->update(ms);
}

void Storm3D_ProceduralManager::apply(int stage)
{
	if(data->useFallback || !data->target)
	{
		if(data->fallback)
			data->fallback->Apply(stage);
	}
	else
	{
		gfx::Device &device = data->storm.GetD3DDevice();
		if(data->target)
			device.SetTexture(stage, data->target);
	}
}

void Storm3D_ProceduralManager::applyOffset(int stage)
{
	gfx::Device &device = data->storm.GetD3DDevice();
	if(data->offsetTarget)
		device.SetTexture(stage, data->offsetTarget);
}

bool Storm3D_ProceduralManager::hasDistortion() const
{
	if(data->useFallback || !data->target)
		return false;

	return data->hasDistortion;
}

void Storm3D_ProceduralManager::releaseTarget()
{
	data->target.Release();
	data->offsetTarget.Release();
}

void Storm3D_ProceduralManager::reset()
{
	data->reset();
}
