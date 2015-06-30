// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

//------------------------------------------------------------------
// Includes
//------------------------------------------------------------------
#include "storm3d.h"
#include "storm3d_scene_piclist.h"
#include "storm3d_texture.h"
#include "storm3d_material.h"
#include "storm3d_scene.h"
#include "Clipper.h"

#include "../../util/Debug_MemoryManager.h"


//------------------------------------------------------------------
// Storm3D_Scene_PicList_Picture
//------------------------------------------------------------------
Storm3D_Scene_PicList_Picture::Storm3D_Scene_PicList_Picture(Storm3D *s2, Storm3D_Scene *scene,Storm3D_Material *_mat,VC2 _position,VC2 _size,float alpha_, float rotation_, float x1_,float y1_,float x2_,float y2_, bool wrap_) 
:	Storm3D_Scene_PicList(s2,scene,_position,_size),material(_mat),alpha(alpha_),rotation(rotation_),x1(x1_),y1(y1_),x2(x2_),y2(y2_),wrap(wrap_),customShape(NULL)
{
}

Storm3D_Scene_PicList_Picture::~Storm3D_Scene_PicList_Picture()
{
	if(customShape)
	{
		delete customShape->vertices;
		delete customShape;
		customShape = NULL;
	}
}

void Storm3D_Scene_PicList_Picture::createCustomShape(struct Vertex_P2DUV *vertices, int numVertices)
{
	customShape = new CustomShape();
	customShape->numVertices = numVertices;
	customShape->vertices = new Vertex_P2DUV[numVertices];
	memcpy(customShape->vertices, vertices, numVertices * sizeof(Vertex_P2DUV));
}

void Storm3D_Scene_PicList_Picture::Render()
{
	if(wrap)
	{
		Storm3D2->GetD3DDevice().SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		Storm3D2->GetD3DDevice().SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	}

	IStorm3D_Material::ATYPE alphaType = IStorm3D_Material::ATYPE_NONE;
	if(material)
	{
		alphaType = material->GetAlphaType();
		if(alpha < 0.99f && alphaType == IStorm3D_Material::ATYPE_NONE)
			material->SetAlphaType(IStorm3D_Material::ATYPE_USE_TEXTRANSPARENCY);

		// Apply the texture
		material->ApplyBaseTextureExtOnly();

		// Animate
		{
			IStorm3D_Texture *ti = material->GetBaseTexture();
			// FIXED: crashed to null pointer here. -jpk
			if (ti != NULL)
			{
				Storm3D_Texture *t = static_cast<Storm3D_Texture *> (ti);
				t->AnimateVideo();
			}
		}
	}

	// Create 3d-vector
	VC2 pos(position.x,position.y);

	// Create color (color+alpha)
	DWORD col = 0xFFFFFFFF;
	if(material)
	{
		COL c(1.f, 1.f, 1.f);
		float newAlpha = 1.f;
		c = material->GetColor();
		newAlpha = alpha * (1-material->GetTransparency());
		col=D3DCOLOR_ARGB((int)((newAlpha)*255.0f),(int)(c.r*255.0f),(int)(c.g*255.0f),(int)(c.b*255.0f));
	}

	// Render it
    Storm3D2->GetD3DDevice().SetStdProgram(GfxDevice::SSF_2D_POS | GfxDevice::SSF_COLOR | GfxDevice::SSF_TEXTURE);
    Storm3D2->GetD3DDevice().SetFVF(FVF_P2DUV);

    VC2 pixsz = Storm3D2->GetD3DDevice().pixelSize();

	// render with custom shape
	if(customShape && customShape->vertices)
	{
		// use combined alpha
		float alpha_mul = 1.0f;
		if(material)
		{
			alpha_mul = alpha * (1.0f - material->GetTransparency());
		}
        for (int i = 0; i < customShape->numVertices; i++)
        {
            DWORD c = customShape->vertices[i].d;
            int newAlpha = (int)((c >> 24) * alpha_mul);
            c &= 0x00FFFFFF;
            c |= (newAlpha & 0xFF) << 24;
            customShape->vertices[i].d = c;
            customShape->vertices[i].p.x = frozenbyte::storm::convX_SCtoDS(customShape->vertices[i].p.x-0.5f, pixsz.x);
            customShape->vertices[i].p.y = frozenbyte::storm::convY_SCtoDS(customShape->vertices[i].p.y-0.5f, pixsz.y);
        }
		Storm3D2->GetD3DDevice().DrawPrimitiveUP(D3DPT_TRIANGLELIST,customShape->numVertices/3,customShape->vertices,sizeof(Vertex_P2DUV));
		scene->AddPolyCounter(customShape->numVertices/3);
	}
	// render quad
	else
	{
		VC2 p[4];
		p[0] = VC2(x1, y2);
		p[1] = VC2(x1, y1);
		p[2] = VC2(x2, y2);
		p[3] = VC2(x2, y1);

		float xc = .5f; //(x2 - x1) * .5f;
		float yc = .5f; //(y2 - y1) * .5f;

		if(fabsf(rotation) > 0.001f)
		{
			for(unsigned int i = 0; i < 4; ++i)
			{
				float x = p[i].x - xc;
				float y = p[i].y - yc;

				p[i].x =  x * cosf(rotation) + y * sinf(rotation);
				p[i].y = -x * sinf(rotation) + y * cosf(rotation);

				p[i].x += xc;
				p[i].y += yc;
			}
		}

        // Create a quad
        Vertex_P2DUV vx[4] = {
            {VC2(pos.x,        pos.y+size.y), col, p[0]},
            {VC2(pos.x,        pos.y),        col, p[1]},
            {VC2(pos.x+size.x, pos.y+size.y), col, p[2]},
            {VC2(pos.x+size.x, pos.y),        col, p[3]},
        };

        for (int i = 0; i < 4; ++i)
        {
            vx[i].p.x = frozenbyte::storm::convX_SCtoDS(vx[i].p.x-0.5f, pixsz.x);
            vx[i].p.y = frozenbyte::storm::convY_SCtoDS(vx[i].p.y-0.5f, pixsz.y);
        }

		Storm3D2->GetD3DDevice().DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vx,sizeof(Vertex_P2DUV));
		scene->AddPolyCounter(2);
	}

	if(material)
		material->SetAlphaType(alphaType);

	if(wrap)
	{
		Storm3D2->GetD3DDevice().SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		Storm3D2->GetD3DDevice().SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	}
}



//------------------------------------------------------------------
// Storm3D_Scene_PicList_Picture3D
//------------------------------------------------------------------
Storm3D_Scene_PicList_Picture3D::Storm3D_Scene_PicList_Picture3D(Storm3D *s2,
		Storm3D_Scene *scene,Storm3D_Material *_mat,VC3 _position,VC2 _size) :
	Storm3D_Scene_PicList(s2,scene,_position,_size),material(_mat)
{
}


void Storm3D_Scene_PicList_Picture3D::Render()
{
	// Calculate sprites position on screen
	VC3 scpos;
	float w,rz;

	// Calculate position, and render sprite only if it's visible
	if (scene->camera.GetTransformedToScreen(position,scpos,w,rz))
	if (rz<scene->camera.vis_range)
	{
		// VC3 size
		size/=rz;

		// Apply the texture
		material->ApplyBaseTextureExtOnly();

		// Get viewport size
		Storm3D_SurfaceInfo ss=Storm3D2->GetScreenSize();

		// Create 3d-vector
		VC3 pos=VC3(scpos.x*ss.width,scpos.y*ss.height,scpos.z);
		size.x*=ss.width;
		size.y*=ss.height;

		// Create color (color+alpha)
		COL c=material->GetColor();
		float newAlpha = (1-material->GetTransparency());
		DWORD col=D3DCOLOR_ARGB((int)(newAlpha*255.0f),(int)(c.r*255.0f),(int)(c.g*255.0f),(int)(c.b*255.0f));

		// Create a quad
		float hsx=size.x*0.5f;
		float hsy=size.y*0.5f;
		Vertex_P2DUV vx[4];
        vx[0]={VC2(pos.x-hsx,pos.y+hsy),col,VC2(0,1)};
        vx[1]={VC2(pos.x-hsx,pos.y-hsy),col,VC2(0,0)};
        vx[2]={VC2(pos.x+hsx,pos.y+hsy),col,VC2(1,1)};
        vx[3]={VC2(pos.x+hsx,pos.y-hsy),col,VC2(1,0)};

		if (Clip2DRectangle(Storm3D2,vx[1],vx[2])) 
		{
			// Copy clipping
			vx[0].p.x=vx[1].p.x;
			vx[0].uv.x=vx[1].uv.x;
			vx[3].p.y=vx[1].p.y;
			vx[3].uv.y=vx[1].uv.y;
			vx[0].p.y=vx[2].p.y;
			vx[0].uv.y=vx[2].uv.y;
			vx[3].p.x=vx[2].p.x;
			vx[3].uv.x=vx[2].uv.x;

            Storm3D_SurfaceInfo si = Storm3D2->GetScreenSize();

            for (int i = 0; i < 4; ++i)
            {
                vx[i].p.x = vx[i].p.x * 2.0f / si.width - 1.0f;
                vx[i].p.y = 1.0f - vx[i].p.y * 2.0f / si.height;
            }

            // Render it (with Z buffer read)
			Storm3D2->GetD3DDevice().SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
			Storm3D2->GetD3DDevice().SetStdProgram(GfxDevice::SSF_2D_POS|GfxDevice::SSF_COLOR|GfxDevice::SSF_TEXTURE);
			Storm3D2->GetD3DDevice().SetFVF(FVF_P2DUV);

			Storm3D2->GetD3DDevice().DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vx,sizeof(Vertex_P2DUV));
			scene->AddPolyCounter(2);
			Storm3D2->GetD3DDevice().SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
		}
	}
}


