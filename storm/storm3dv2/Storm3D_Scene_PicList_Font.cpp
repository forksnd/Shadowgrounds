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
#include "storm3d_font.h"
#include "Clipper.h"

#include "../../util/Debug_MemoryManager.h"


//------------------------------------------------------------------
// Storm3D_Scene_PicList_Font::Storm3D_Scene_PicList_Font
//------------------------------------------------------------------
Storm3D_Scene_PicList_Font::Storm3D_Scene_PicList_Font(Storm3D *s2,
		Storm3D_Scene *scene,Storm3D_Font *_font,VC2 _position,VC2 _size,const char *_text,float alpha_,const COL &colorFactor_) :
	Storm3D_Scene_PicList(s2,scene,_position,_size),
	font(_font),
	text(0),
	uniText(0),
	alpha(alpha_),
	colorFactor(colorFactor_)
{
	if (!font->isUnicode())
	{
		// Copy text
		text=new char[strlen(_text)+1];
		strcpy(text,_text);
	}
	else
	{
		// Hack: convert to unicode for non-english languages
		int length = MultiByteToWideChar(CP_ACP | CP_UTF8, 0, _text, strlen(_text) + 1, 0, 0);
		uniText = new wchar_t[length];
		MultiByteToWideChar(CP_ACP | CP_UTF8, 0, _text, strlen(_text) + 1, uniText, length);
	}
}

Storm3D_Scene_PicList_Font::Storm3D_Scene_PicList_Font(Storm3D *s2,
		Storm3D_Scene *scene,Storm3D_Font *_font,VC2 _position,VC2 _size,const wchar_t *_text,float alpha_,const COL &colorFactor_) :
	Storm3D_Scene_PicList(s2,scene,_position,_size),
	font(_font),
	text(0),
	uniText(0),
	alpha(alpha_),
	colorFactor(colorFactor_)
{
	// Copy text
	uniText = new wchar_t[wcslen(_text)+1];
	wcscpy(uniText, _text);
}


//------------------------------------------------------------------
// Storm3D_Scene_PicList_Font::~Storm3D_Scene_PicList_Font
//------------------------------------------------------------------
Storm3D_Scene_PicList_Font::~Storm3D_Scene_PicList_Font()
{
	delete[] text;
	delete[] uniText;
}



//------------------------------------------------------------------
// Storm3D_Scene_PicList_Font::Render
//------------------------------------------------------------------
void Storm3D_Scene_PicList_Font::Render()
{
	// Calculate complete letter amount and letters per texture
	int letters_per_texture=font->tex_letter_rows*font->tex_letter_columns;
	int letter_amt=font->texture_amount*letters_per_texture;

	// Create 3d-vector
	VC2 pos(position.x,position.y);

	// Create color (color+alpha)
	COL color = font->GetColor();
	color *= colorFactor;
	color.Clamp();

	//DWORD col=font->GetColor().GetAsD3DCompatibleARGB();
	//DWORD col=color.GetAsD3DCompatibleARGB();
	DWORD col = D3DCOLOR_ARGB((int)((alpha)*255.0f),(int)(color.r*255.0f),(int)(color.g*255.0f),(int)(color.b*255.0f));
	Storm3D2->GetD3DDevice().SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);

	if(font->font && font->sprite)
	{
		#ifdef _MSC_VER
		#pragma message("**                                             **")
		#pragma message("** Size to screen boundary and enable clipping **")
		#pragma message("**                                             **")
		#endif

		//VC2 _position,VC2 _size
		RECT rc = { int(position.x), int(position.y), int(position.x + size.x + 100), int(position.y + size.y + 1000) };
		//if(uniText)
		//	font->font->DrawTextW(0, uniText, wcslen(uniText), &rc, DT_SINGLELINE | DT_LEFT | DT_NOCLIP, col);
		//else if(text)
		//	font->font->DrawText(0, text, strlen(text), &rc, DT_SINGLELINE | DT_LEFT | DT_NOCLIP, col);

		DWORD flags = D3DXSPRITE_SORT_TEXTURE | D3DXSPRITE_ALPHABLEND;
		font->sprite->Begin(flags);

		if(uniText)
			font->font->DrawTextW(font->sprite, uniText, wcslen(uniText), &rc, DT_LEFT | DT_NOCLIP, col);
		else if(text)
			font->font->DrawText(font->sprite, text, strlen(text), &rc, DT_LEFT | DT_NOCLIP, col);

		font->sprite->End();
	}
	else if(text)
	{
		for(int l=0;l<int(strlen(text));l++)
		{
			// Search for letter
			int let=-1;
			for (int i=0;i<letter_amt;i++) 
			{
				if (font->letter_characters[i]==text[l]) 
				{
					let=i;
					// doh, why not break now when we found it and save time!
					break; 
				} 
				else 
				{
					// if we find the null terminator, just stop there, because
					// otherwise we'll go past the character array size if it
					// does not contain character definitions for total of letter_amt
					// characters. In my opininion requiring such a thing is not nice.
					if (font->letter_characters[i] == '\0') 
						break;
				}
			}

			// Is this letter in font
			if (let>=0)
			{
				// Apply the correct texture
				font->textures[let/letters_per_texture]->Apply(0);

				// Calculate x/y
				int x=let%font->tex_letter_columns;
				int y=(let%letters_per_texture)/font->tex_letter_columns;

				// Calculate texture coordinates
				float tx1=1/(float)font->tex_letter_columns;
				float ty1=1/(float)font->tex_letter_rows;
				float fx=(float)x*tx1;
				float fy=(float)y*ty1;

                // Create a quad
                Vertex_P2DUV vx[4] = {
                    {VC2(pos.x,        pos.y+size.y), col, VC2(fx, fy + ty1)},
                    {VC2(pos.x,        pos.y),        col, VC2(fx, fy)},
                    {VC2(pos.x+size.x, pos.y+size.y), col, VC2(fx + tx1, fy + ty1)},
                    {VC2(pos.x+size.x, pos.y),        col, VC2(fx + tx1, fy)},
                };

				// Clip
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

                    VC2 pixsz = Storm3D2->GetD3DDevice().pixelSize();

                    for (int i = 0; i < 4; ++i)
                    {
                        vx[i].p.x = frozenbyte::storm::convX_SCtoDS(vx[i].p.x-0.5f, pixsz.x);
                        vx[i].p.y = frozenbyte::storm::convY_SCtoDS(vx[i].p.y-0.5f, pixsz.y);
                    }

                    // Render it
                    Storm3D2->GetD3DDevice().SetStdProgram(gfx::Device::SSF_2D_POS | gfx::Device::SSF_COLOR | gfx::Device::SSF_TEXTURE);
                    Storm3D2->renderer.SetFVF(FVF_P2DUV);

                    Storm3D2->renderer.DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vx, sizeof(Vertex_P2DUV));
                    scene->AddPolyCounter(2);
				}
			}

			// Add x-koordinate
			if (let>=0) pos.x+=((float)font->letter_width[let]/64.0f)*size.x;
				else pos.x+=size.x/2.0f;
		}
	}
}


