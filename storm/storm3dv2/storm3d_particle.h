// Copyright 2002-2004 Frozenbyte Ltd.

#pragma once


//------------------------------------------------------------------
// Includes
//------------------------------------------------------------------
#include "storm3d.h"
#include "storm3d_common_imp.h"
#include "IStorm3D_Particle.h"
#include "VertexFormats.h"

class Storm3D_ParticleSystem : public IStorm3D_ParticleSystem {
public:

    struct IParticleArray
    {
        uint32_t numParticles = 0;
        uint32_t offsetParticles = 0;
        IStorm3D_Material* m_mtl = nullptr;
        COL factor{};
        Storm3D_ParticleTextureAnimationInfo m_animInfo;

        virtual ~IParticleArray() {}

        virtual uint32_t fillDynamicBuffer(Vertex_P3DUV2 *pointer, Storm3D_Scene *scene) = 0;
        virtual void prepareRender(gfx::Renderer& renderer) = 0;
    };

    //struct PointArray : public IParticleArray
    //{
    //    Storm3D_PointParticle* m_parts;

    //    PointArray(IStorm3D_Material* mtl, Storm3D_PointParticle* parts, uint32_t nParts, const COL &factor_, bool distortion_)
    //        : m_parts(parts)
    //    {
    //        distortion = distortion_;
    //        numParticles = nParts;
    //        m_mtl = mtl;
    //        factor = factor_;
    //    }

    //    uint32_t fillDynamicBuffer(Vertex_P3DUV2 *pointer, uint32_t particleOffset, Storm3D_Scene *scene) override { return 0; }
    //    void prepareRender(gfx::Renderer& renderer, uint32_t &vertexOffset, uint32_t &particleAmount) override {}
    //};

    struct QuadArray : public IParticleArray
    {
        Storm3D_PointParticle* m_parts;
        bool faceUp;

        QuadArray(IStorm3D_Material* mtl, Storm3D_PointParticle* parts, uint32_t nParts, Storm3D_ParticleTextureAnimationInfo* info, const COL &factor_, bool faceUp_)
            : m_parts(parts), faceUp(faceUp_)
        {
            numParticles = nParts;
            m_mtl = mtl;
            factor = factor_;

            bool animEnabled = info && info->textureUSubDivs && info->textureVSubDivs;
            m_animInfo = animEnabled ? *info : Storm3D_ParticleTextureAnimationInfo{ 0 };
        }

        uint32_t fillDynamicBuffer(Vertex_P3DUV2 *pointer, Storm3D_Scene *scene) override;
        void prepareRender(gfx::Renderer& renderer) override;
    };

    struct LineArray : public IParticleArray
    {
        Storm3D_LineParticle* m_parts;

        LineArray(IStorm3D_Material* mtl, Storm3D_LineParticle* parts, uint32_t nParts, Storm3D_ParticleTextureAnimationInfo* info, const COL &factor_)
            : m_parts(parts)
        {
            numParticles = nParts;
            m_mtl = mtl;
            factor = factor_;

            bool animEnabled = info && info->textureUSubDivs && info->textureVSubDivs;
            m_animInfo = animEnabled ? *info : Storm3D_ParticleTextureAnimationInfo{ 0 };
        }

        uint32_t fillDynamicBuffer(Vertex_P3DUV2 *pointer, Storm3D_Scene *scene) override;
        void prepareRender(gfx::Renderer& renderer) override;
    };

    std::vector<std::unique_ptr<IParticleArray>> m_normalArrays;
    std::vector<std::unique_ptr<IParticleArray>> m_distortionArrays;
    Storm3D* Storm3D2;

    void renderPoints(IStorm3D_Material* mtl, Storm3D_PointParticle* parts, int nParts, const COL &factor, bool distortion)
    {
        // Not implemented!
        //m_particleArrays.push_back(new PointArray(mtl, parts, nParts, factor, distortion));
    }

    void renderQuads(IStorm3D_Material* mtl, Storm3D_PointParticle* parts, int nParts,
        Storm3D_ParticleTextureAnimationInfo* info, const COL &factor, bool distortion, bool faceUp)
    {
        QuadArray* array = new QuadArray(mtl, parts, nParts, info, factor, faceUp);
        auto& arrays = distortion ? m_distortionArrays : m_normalArrays;
        arrays.emplace_back(array);
    }

    void renderLines(IStorm3D_Material* mtl, Storm3D_LineParticle* parts, int nParts,
        Storm3D_ParticleTextureAnimationInfo* info, const COL &factor, bool distortion)
    {
        LineArray* array = new LineArray(mtl, parts, nParts, info, factor);
        auto& arrays = distortion ? m_distortionArrays : m_normalArrays;
        arrays.emplace_back(array);
    }

    void Render(Storm3D_Scene *scene, bool distortion);
    void Clear();

    Storm3D_ParticleSystem(Storm3D *storm)
        : Storm3D2(storm)
    {
    }
};
