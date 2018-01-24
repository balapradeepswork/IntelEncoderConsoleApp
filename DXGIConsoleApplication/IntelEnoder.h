#ifndef _INTELENCODER_H_
#define _INTELENCODER_H_

#include "DuplicationManager.h"
#include "common_utils.h"

#define MSDK_MAX_PATH 280

struct EncodeOptions {
	mfxIMPL impl; // OPTION_IMPL

	
	char SinkName[MSDK_MAX_PATH];   // OPTION_FSINK

	mfxU16 Width; // OPTION_GEOMETRY
	mfxU16 Height;

	mfxU16 Bitrate; // OPTION_BITRATE

	mfxU16 FrameRateN; // OPTION_FRAMERATE
	mfxU16 FrameRateD;

	bool MeasureLatency; // OPTION_MEASURE_LATENCY
};

mfxStatus LoadRawRGBFrameDDA(mfxFrameSurface1* pSurface, DUPLICATIONMANAGER* DuplMgr);

class IntelEncoder
{
public:
	//Methods
	IntelEncoder();
	~IntelEncoder();
	mfxStatus InitializeX();
	mfxStatus RunVppAndEncode();
	mfxStatus FlushVppAndEncoder();
	void CloseResources();

	//Vars
	MFXVideoENCODE *mfxENC; 
	MFXVideoVPP *mfxVPP;
	mfxIMPL impl_type;

private:
	//Vars
	FILE* fSink;
	EncodeOptions options;
	DUPLICATIONMANAGER DuplMgr;
	MFXVideoSession *pSession;
	mfxFrameAllocator *pMfxAllocator;
	mfxVideoParam mfxEncParams;
	mfxVideoParam VPPParams; 
	mfxExtBuffer* extBuffers[1];
	mfxFrameSurface1** pmfxSurfacesVPPIn;
	mfxFrameSurface1** pVPPSurfacesVPPOutEnc;
	mfxU16 nSurfNumVPPIn;
	mfxU16 nSurfNumVPPOutEnc;
	mfxBitstream mfxBS; 
	mfxExtVPPDoNotUse extDoNotUse;
	mfxFrameAllocResponse mfxResponseVPPIn;
	mfxFrameAllocResponse mfxResponseVPPOutEnc;
	int nEncSurfIdx;
	int nVPPSurfIdx;
	mfxSyncPoint syncpVPP, syncpEnc;
	mfxU32 nFrame;
	mfxTime tStart, tEnd;
	double elapsed;
	
	//Methods
	int SetEncodeOptions();
	mfxStatus SetVppParameters();
	mfxStatus SetEncParameters();
	mfxStatus DisableVppDefaultOpsAndAddExtendedVppBuffers();
	mfxStatus QueryAndAllocRequiredSurfacesHW();
	mfxStatus QueryAndAllocRequiredSurfacesSW();

};

#endif