// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

//------------------------------------------------------------------
// Includes
//------------------------------------------------------------------
#include "storm3d.h"

#include "storm3d_scene.h"
#include "storm3d_particle.h"
#include "storm3d_material.h"
#include "../../util/Debug_MemoryManager.h"

using namespace frozenbyte::storm;

const uint32_t MAX_PARTICLES = 4096;

void Storm3D_ParticleSystem::Render(Storm3D_Scene *scene, bool distortion)
{
    gfx::Renderer& renderer = Storm3D2->renderer;
    gfx::Device& device = renderer.device;
    gfx::ProgramManager& programManager = renderer.programManager;

    GFX_TRACE_SCOPE("Storm3D_ParticleSystem::Render");

    //device.SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    frozenbyte::storm::setCulling(device, D3DCULL_NONE);
    device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device.SetRenderState(D3DRS_ALPHAREF, 0x1);

    int index = (distortion) ? 1 : 0;
    auto& arrays = distortion ? m_distortionArrays : m_normalArrays;

    auto it = arrays.begin();
    for (; it != arrays.end(); ++it)
    {
        Vertex_P3DUV2 *buffer = 0;
        renderer.lockDynVtx<Vertex_P3DUV2>((*it)->numParticles * 4, &buffer, &(*it)->offsetParticles);
        (*it)->fillDynamicBuffer(buffer, scene);
        renderer.unlockDynVtx();
    }

    programManager.setStdProgram(device, gfx::ProgramManager::SSF_COLOR | gfx::ProgramManager::SSF_TEXTURE);
    programManager.setProjectionMatrix(scene->camera.GetProjectionMatrix());
    
    renderer.setDynVtxBuffer<Vertex_P3DUV2>();
    renderer.setFVF(FVF_P3DUV2);

    // Render arrays
    it = arrays.begin();
    for (; it != arrays.end(); ++it)
    {
        uint32_t vertexOffset = 0;
        uint32_t particleAmount = 0;

        programManager.setViewMatrix(scene->camera.GetViewMatrix());
        (*it)->prepareRender(renderer);

        renderer.drawQuads((*it)->offsetParticles, (*it)->numParticles);
    }
}

void Storm3D_ParticleSystem::Clear()
{
    m_normalArrays.clear();
    m_distortionArrays.clear();
}

namespace quad_util
{
    inline void rotatePointFast(float& x1, float& y1, float tx, float ty, float ca, float sa) {
        x1 = ca * tx - sa * ty;
        y1 = sa * tx + ca * ty;
    }

    void rotateToward(const VC3 &a, const VC3 &b, QUAT &result)
    {
        VC3 axis = a.GetCrossWith(b);
        float dot = a.GetDotWith(b);

        if (dot < -0.99f)
        {
            result = QUAT();
            return;
        }

        result.x = axis.x;
        result.y = axis.y;
        result.z = axis.z;
        result.w = (dot + 1.0f);
        result.Normalize();
    }
}

uint32_t Storm3D_ParticleSystem::QuadArray::fillDynamicBuffer(Vertex_P3DUV2 *pointer, Storm3D_Scene *scene)
{
    Vertex_P3DUV2 *vp = pointer;

    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    if (m_animInfo.numFrames > 1)
    {
        frameWidth = 1.0f / m_animInfo.textureUSubDivs;
        frameHeight = 1.0f / m_animInfo.textureVSubDivs;
    }

    DWORD stride = sizeof(Vertex_P3DUV2);
    D3DXMATRIX mv = scene->camera.GetViewMatrix();
    DWORD c0 = 0;

    // Up rotation
    MAT view(scene->camera.GetViewMatrix());
    VC3 worldUp(0, 1.f, 0);
    view.RotateVector(worldUp);
    QUAT q;
    quad_util::rotateToward(worldUp, VC3(0, 0, 1.f), q);
    MAT g;
    g.CreateRotationMatrix(q);

    for (uint32_t i = 0; i < numParticles; i++, vp += 4)
    {
        Storm3D_PointParticle& p = m_parts[i];

        float sa = sinf(p.angle);
        float ca = cosf(p.angle);
        float hsize = 0.5f*p.size;

        float x1, y1, x2, y2, x3, y3, x4, y4;

        quad_util::rotatePointFast(x1, y1, -hsize, hsize, ca, sa);
        quad_util::rotatePointFast(x2, y2, hsize, hsize, ca, sa);
        quad_util::rotatePointFast(x3, y3, -hsize, -hsize, ca, sa);
        quad_util::rotatePointFast(x4, y4, hsize, -hsize, ca, sa);

        VC3 v;

        v.x = p.position.x * mv.m[0][0] +
            p.position.y * mv.m[1][0] +
            p.position.z * mv.m[2][0] + mv.m[3][0];

        v.y = p.position.x * mv.m[0][1] +
            p.position.y * mv.m[1][1] +
            p.position.z * mv.m[2][1] + mv.m[3][1];

        v.z = p.position.x * mv.m[0][2] +
            p.position.y * mv.m[1][2] +
            p.position.z * mv.m[2][2] + mv.m[3][2];

        VC3 v1(x1, y1, 0);
        VC3 v2(x2, y2, 0);
        VC3 v3(x3, y3, 0);
        VC3 v4(x4, y4, 0);

        if (faceUp)
        {
            g.RotateVector(v1);
            g.RotateVector(v2);
            g.RotateVector(v3);
            g.RotateVector(v4);
        }

        v1 += v;
        v2 += v;
        v3 += v;
        v4 += v;

        /*
        VC3 v1(v.x + x1, v.y + y1, v.z);
        VC3 v2(v.x + x2, v.y + y2, v.z);
        VC3 v3(v.x + x3, v.y + y3, v.z);
        VC3 v4(v.x + x4, v.y + y4, v.z);

        {
        v1 -= v;
        g.RotateVector(v1);
        v1 += v;

        v2 -= v;
        g.RotateVector(v2);
        v2 += v;

        v3 -= v;
        g.RotateVector(v3);
        v3 += v;

        v4 -= v;
        g.RotateVector(v4);
        v4 += v;
        }
        */

        vp[0].p = v1;
        vp[1].p = v2;
        vp[2].p = v3;
        vp[3].p = v4;

        // Fill texturecoords
        if (m_animInfo.numFrames > 1) {

            int frame = (int)p.frame % m_animInfo.numFrames;

            int col = frame % m_animInfo.textureUSubDivs;
            int row = frame / m_animInfo.textureUSubDivs;

            float tx = frameWidth * (float)col;
            float ty = frameHeight * (float)row;

            vp[0].uv0 = VC2(tx, ty);
            vp[1].uv0 = VC2(tx + frameWidth, ty);
            vp[2].uv0 = VC2(tx, ty + frameHeight);
            vp[3].uv0 = VC2(tx + frameWidth, ty + frameHeight);
        }
        else
        {
            vp[0].uv0 = VC2(0.0f, 0.0f);
            vp[1].uv0 = VC2(1.0f, 0.0f);
            vp[2].uv0 = VC2(0.0f, 1.0f);
            vp[3].uv0 = VC2(1.0f, 1.0f);
        }

        c0 = (((DWORD)(p.alpha * 255.0f) & 0xff) << 24) |
            (((DWORD)(factor.r * p.color.r * 255.0f) & 0xff) << 16) |
            (((DWORD)(factor.g * p.color.g * 255.0f) & 0xff) << 8) |
            (((DWORD)(factor.b * p.color.b * 255.0f) & 0xff));

        vp[0].d = c0;
        vp[1].d = c0;
        vp[2].d = c0;
        vp[3].d = c0;
    }

    return numParticles;
}

void Storm3D_ParticleSystem::QuadArray::prepareRender(gfx::Renderer& renderer)
{
    gfx::Device& device = renderer.device;
    gfx::ProgramManager& programManager = renderer.programManager;

    if (m_mtl)
    {
        static_cast<Storm3D_Material*>(m_mtl)->ApplyBaseTextureExtOnly();
    }
    else
    {
        device.SetTexture(0, NULL);
    }

    D3DMATRIX dm;
    dm._12 = dm._13 = dm._14 = 0;
    dm._21 = dm._23 = dm._24 = 0;
    dm._31 = dm._32 = dm._34 = 0;
    dm._41 = dm._42 = dm._43 = 0;
    dm._11 = dm._22 = dm._33 = dm._44 = 1;

    programManager.setWorldMatrix(dm);
    programManager.setViewMatrix(dm);
    programManager.commitConstants(device);
}

// LineArray

// this code is a rip off from flipcode cotd, originally
// posted by Pierre Terdiman, remember to greet!!!!!!!!!
// needed because storm3d:s line particle implementation
// didn't really work at all.

namespace line_utils
{
    inline void transPoint3x3(Vector& dest, const Vector& src, const D3DXMATRIX& m) {

        dest.x = src.x * m.m[0][0] + src.y * m.m[1][0] + src.z * m.m[2][0];
        dest.y = src.x * m.m[0][1] + src.y * m.m[1][1] + src.z * m.m[2][1];
        dest.z = src.x * m.m[0][2] + src.y * m.m[1][2] + src.z * m.m[2][2];

    }

    inline void computePoint(Vector& dest, float x, float y, const D3DXMATRIX& rot, const Vector& trans)
    {
        dest.x = trans.x + x * rot.m[0][0] + y * rot.m[1][0];
        dest.y = trans.y + x * rot.m[0][1] + y * rot.m[1][1];
        dest.z = trans.z + x * rot.m[0][2] + y * rot.m[1][2];
    }

    float computeConstantScale(const Vector& pos, const D3DXMATRIX& view, const D3DXMATRIX& proj)
    {
        static const float mRenderWidth = 640.0f;

        Vector ppcam0;
        //D3DXVec3TransformCoord(&ppcam0, &pos, &view);
        transPoint3x3(ppcam0, pos, view);
        ppcam0.x += view.m[3][0];
        ppcam0.y += view.m[3][1];
        ppcam0.z += view.m[3][2];


        Vector ppcam1 = ppcam0;
        ppcam1.x += 1.0f;

        float l1 = 1.0f / (ppcam0.x*proj.m[0][3] + ppcam0.y*proj.m[1][3] + ppcam0.z*proj.m[2][3] + proj.m[3][3]);
        float c1 = (ppcam0.x*proj.m[0][0] + ppcam0.y*proj.m[1][0] + ppcam0.z*proj.m[2][0] + proj.m[3][0])*l1;
        float l2 = 1.0f / (ppcam1.x*proj.m[0][3] + ppcam1.y*proj.m[1][3] + ppcam1.z*proj.m[2][3] + proj.m[3][3]);
        float c2 = (ppcam1.x*proj.m[0][0] + ppcam1.y*proj.m[1][0] + ppcam1.z*proj.m[2][0] + proj.m[3][0])*l2;
        float CorrectScale = 1.0f / (c2 - c1);
        return (CorrectScale / mRenderWidth);
    }
}

uint32_t Storm3D_ParticleSystem::LineArray::fillDynamicBuffer(Vertex_P3DUV2 *pointer, Storm3D_Scene *scene)
{
    Vertex_P3DUV2 *vp = pointer;

    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    if (m_animInfo.numFrames > 1)
    {
        frameWidth = 1.0f / m_animInfo.textureUSubDivs;
        frameHeight = 1.0f / m_animInfo.textureVSubDivs;
    }

    DWORD stride = sizeof(Vertex_P3DUV2);
    D3DXMATRIX inview, view, proj;
    view = scene->camera.GetViewMatrix();

    D3DXMatrixInverse(&inview, NULL, &view);
    proj = scene->camera.GetProjectionMatrix();

    for (uint32_t i = 0; i < numParticles; i++, vp += 4)
    {
        Storm3D_LineParticle& p = m_parts[i];

        // Compute delta in camera space
        Vector Delta;
        line_utils::transPoint3x3(Delta, p.position[1] - p.position[0], view);

        // Compute size factors
        /*if(constantsize)
        {
            // Compute scales so that screen-size is constant
            SizeP0 *= computeConstantScale(p0, view, proj);
            SizeP1 *= computeConstantScale(p1, view, proj);
        }*/

        // Compute quad vertices
        float Theta0 = atan2f(-Delta.x, -Delta.y);
        float c0 = p.size[0] * cosf(Theta0);
        float s0 = p.size[0] * sinf(Theta0);
        line_utils::computePoint(vp[0].p, c0, -s0, inview, p.position[0]);
        line_utils::computePoint(vp[1].p, -c0, s0, inview, p.position[0]);

        float Theta1 = atan2f(Delta.x, Delta.y);
        float c1 = p.size[1] * cosf(Theta1);
        float s1 = p.size[1] * sinf(Theta1);
        line_utils::computePoint(vp[2].p, -c1, s1, inview, p.position[1]);
        line_utils::computePoint(vp[3].p, c1, -s1, inview, p.position[1]);

        // Fill texturecoords
        if (m_animInfo.numFrames > 1)
        {
            int frame = (int)p.frame % m_animInfo.numFrames;
            int col = frame % m_animInfo.textureUSubDivs;
            int row = frame / m_animInfo.textureUSubDivs;

            float tx = frameWidth * (float)col;
            float ty = frameHeight * (float)row;

            vp[0].uv0 = VC2(tx, ty);
            vp[1].uv0 = VC2(tx + frameWidth, ty);
            vp[2].uv0 = VC2(tx, ty + frameHeight);
            vp[3].uv0 = VC2(tx + frameWidth, ty + frameHeight);
        }
        else
        {
            vp[0].uv0 = VC2(0.0f, 0.0f);
            vp[1].uv0 = VC2(1.0f, 0.0f);
            vp[2].uv0 = VC2(0.0f, 1.0f);
            vp[3].uv0 = VC2(1.0f, 1.0f);
        }

        uint32_t col0 = (((uint32_t)(p.alpha[0] * 255.0f) & 0xff) << 24) |
            (((uint32_t)(factor.r * p.color[0].r * 255.0f) & 0xff) << 16) |
            (((uint32_t)(factor.g * p.color[0].g * 255.0f) & 0xff) << 8) |
            (((uint32_t)(factor.b * p.color[0].b * 255.0f) & 0xff));

        uint32_t col1 = (((uint32_t)(p.alpha[1] * 255.0f) & 0xff) << 24) |
            (((uint32_t)(factor.r * p.color[1].r * 255.0f) & 0xff) << 16) |
            (((uint32_t)(factor.g * p.color[1].g * 255.0f) & 0xff) << 8) |
            (((uint32_t)(factor.b * p.color[1].b * 255.0f) & 0xff));

        vp[0].d = col0;
        vp[1].d = col0;
        vp[2].d = col1;
        vp[3].d = col1;
    }

    return numParticles;
}

void Storm3D_ParticleSystem::LineArray::prepareRender(gfx::Renderer& renderer)
{
    gfx::Device& device = renderer.device;
    gfx::ProgramManager& programManager = renderer.programManager;

    if (m_mtl)
    {
        static_cast<Storm3D_Material*>(m_mtl)->ApplyBaseTextureExtOnly();
    }
    else
    {
        device.SetTexture(0, NULL);
    }

    D3DMATRIX dm;
    dm._12 = dm._13 = dm._14 = 0;
    dm._21 = dm._23 = dm._24 = 0;
    dm._31 = dm._32 = dm._34 = 0;
    dm._41 = dm._42 = dm._43 = 0;
    dm._11 = dm._22 = dm._33 = dm._44 = 1;

    programManager.setWorldMatrix(dm);
    programManager.commitConstants(device);
}
