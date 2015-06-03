// Copyright 2002-2004 Frozenbyte Ltd.

#ifndef INCLUDED_STORM3D_VIDEO_PLAYER_H
#define INCLUDED_STORM3D_VIDEO_PLAYER_H

class Storm3D;
class Storm3D_Scene;
class IStorm3D_StreamBuilder;
struct Storm3D_VideoPlayerData;

class Storm3D_VideoPlayer
{
	Storm3D_VideoPlayerData* data;

public:
	Storm3D_VideoPlayer(Storm3D &storm, Storm3D_Scene &scene, const char *fileName, IStorm3D_StreamBuilder *streamBuilder);
	~Storm3D_VideoPlayer();

	void play();
};

#endif
