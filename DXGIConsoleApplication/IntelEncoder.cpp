#include "IntelEnoder.h"

IntelEncoder::IntelEncoder()
{

}

IntelEncoder::~IntelEncoder()
{

}

/**
*	Desktop Duplication API Setup for Capture RGB32 Image
*	Intel Media SDK VPP and encode pipeline setup.
*	RGB32 color conversion to NV12 via VPP then encode
*	Encoding an AVC (H.264) stream
*	Video memory surfaces are used
*/
mfxStatus IntelEncoder::InitializeX()
{
	DUPL_RETURN Ret;
	UINT Output = 0;

	// Make duplication manager
	Ret = DuplMgr.InitDupl(stdout, Output);
	if (Ret != DUPL_RETURN_SUCCESS)
	{
		fprintf_s(stdout, "Duplication Manager couldn't be initialized.");
		return MFX_ERR_UNKNOWN;
	}
	if (SetEncodeOptions() == MFX_ERR_NULL_PTR)
	{
		fprintf_s(stdout, "Sink file couldn't be created.");
		return MFX_ERR_NULL_PTR;
	}
	mfxIMPL impl = options.impl;
	//Version 1.3 is selected for Video Conference Mode compatibility.
	mfxVersion ver = { { 3, 1 } };
	pSession = new MFXVideoSession();
	
	pMfxAllocator = (mfxFrameAllocator*)malloc(sizeof(mfxFrameAllocator));
	memset(pMfxAllocator, 0, sizeof(mfxFrameAllocator));
    mfxStatus sts = Initialize(impl, ver, pSession, pMfxAllocator);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = pSession->QueryIMPL(&impl_type);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	if (impl_type == MFX_IMPL_SOFTWARE)
	{
		printf("Implementation type is : SOFTWARE\n");
	}
	else
	{
		printf("Implementation type is : HARDWARE\n");
	}

	sts = SetVppParameters();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	
	sts = SetEncParameters();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Create Media SDK encoder
	mfxENC = new MFXVideoENCODE(*pSession);
	// Create Media SDK VPP component
	mfxVPP = new MFXVideoVPP(*pSession);

	if (impl_type == MFX_IMPL_SOFTWARE)
	{
		sts = QueryAndAllocRequiredSurfacesSW();
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	}
	else
	{
		sts = QueryAndAllocRequiredSurfacesHW();
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	}


	sts = DisableVppDefaultOpsAndAddExtendedVppBuffers();
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


	// Initialize Media SDK VPP
	sts = mfxVPP->Init(&VPPParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


	// Initialize the Media SDK encoder
	sts = mfxENC->Init(&mfxEncParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);  
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	

	// Retrieve video parameters selected by encoder.
	// - BufferSizeInKB parameter is required to set bit stream buffer size
	mfxVideoParam par;
	memset(&par, 0, sizeof(par));
	sts = mfxENC->GetVideoParam(&par);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Prepare Media SDK bit stream buffer
	memset(&mfxBS, 0, sizeof(mfxBS));
	mfxBS.MaxLength = par.mfx.BufferSizeInKB * 1000;
	mfxBS.Data = new mfxU8[mfxBS.MaxLength];
	MSDK_CHECK_POINTER(mfxBS.Data, MFX_ERR_MEMORY_ALLOC);
}

int IntelEncoder::SetEncodeOptions()
{
	options.impl = MFX_IMPL_AUTO_ANY;
	options.Width = DuplMgr.GetImageWidth();
	options.Height = DuplMgr.GetImageHeight();
	options.Bitrate = 4000;
	options.FrameRateN = 30;
	options.FrameRateD = 1;
	options.MeasureLatency = true;
	strcpy_s(options.SinkName, "output.h264");
	fopen_s(&fSink, options.SinkName, "wb");
	MSDK_CHECK_POINTER(fSink, MFX_ERR_NULL_PTR);
}

mfxStatus IntelEncoder::SetVppParameters()
{
	mfxStatus sts = MFX_ERR_NONE;
	// Initialize VPP parameters
	memset(&VPPParams, 0, sizeof(VPPParams));
	// Input data
	VPPParams.vpp.In.FourCC = MFX_FOURCC_RGB4;
	VPPParams.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	VPPParams.vpp.In.CropX = 0;
	VPPParams.vpp.In.CropY = 0;
	VPPParams.vpp.In.CropW = options.Width;
	VPPParams.vpp.In.CropH = options.Height;
	VPPParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	VPPParams.vpp.In.FrameRateExtN = options.FrameRateN;
	VPPParams.vpp.In.FrameRateExtD = options.FrameRateD;
	// width must be a multiple of 16
	// height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	VPPParams.vpp.In.Width = MSDK_ALIGN(options.Width);
	VPPParams.vpp.In.Height =
		(MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.In.PicStruct) ?
		MSDK_ALIGN16(options.Height) :
		MSDK_ALIGN32(options.Height);
	// Output data
	VPPParams.vpp.Out.FourCC = MFX_FOURCC_NV12;
	VPPParams.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	VPPParams.vpp.Out.CropX = 0;
	VPPParams.vpp.Out.CropY = 0;
	VPPParams.vpp.Out.CropW = options.Width;
	VPPParams.vpp.Out.CropH = options.Height;
	VPPParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	VPPParams.vpp.Out.FrameRateExtN = options.FrameRateN;
	VPPParams.vpp.Out.FrameRateExtD = options.FrameRateD;
	// width must be a multiple of 16
	// height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	VPPParams.vpp.Out.Width = MSDK_ALIGN(VPPParams.vpp.Out.CropW);
	VPPParams.vpp.Out.Height =
		(MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.Out.PicStruct) ?
		MSDK_ALIGN16(VPPParams.vpp.Out.CropH) :
		MSDK_ALIGN32(VPPParams.vpp.Out.CropH);

	VPPParams.AsyncDepth = 1;
	//VPPParams.mfx.GopRefDist = 1;
	//VPPParams.mfx.NumRefFrame = 1;

	if (impl_type == MFX_IMPL_SOFTWARE)
	{
		VPPParams.IOPattern =  MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
	}
	else
	{
		VPPParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
	}

	return sts;
}

mfxStatus IntelEncoder::DisableVppDefaultOpsAndAddExtendedVppBuffers()
{
	mfxStatus sts = MFX_ERR_NONE;
	// Disable default VPP operations
	memset(&extDoNotUse, 0, sizeof(mfxExtVPPDoNotUse));
	extDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
	extDoNotUse.Header.BufferSz = sizeof(mfxExtVPPDoNotUse);
	extDoNotUse.NumAlg = 4;
	extDoNotUse.AlgList = new mfxU32[extDoNotUse.NumAlg];
	MSDK_CHECK_POINTER(extDoNotUse.AlgList, MFX_ERR_MEMORY_ALLOC);
	extDoNotUse.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE;       // turn off denoising (on by default)
	extDoNotUse.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS;        // turn off scene analysis (on by default)
	extDoNotUse.AlgList[2] = MFX_EXTBUFF_VPP_DETAIL;        // turn off detail enhancement (on by default)
	extDoNotUse.AlgList[3] = MFX_EXTBUFF_VPP_PROCAMP;       // turn off processing amplified (on by default)

	// Add extended VPP buffers
	extBuffers[0] = (mfxExtBuffer*)& extDoNotUse;
	VPPParams.ExtParam = extBuffers;
	VPPParams.NumExtParam = 1;

	return sts;
}

mfxStatus IntelEncoder::SetEncParameters()
{

	mfxStatus sts = MFX_ERR_NONE;
	// Initialize encoder parameters
	memset(&mfxEncParams, 0, sizeof(mfxEncParams));
	mfxEncParams.mfx.CodecId = MFX_CODEC_AVC;
	mfxEncParams.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;
	mfxEncParams.mfx.TargetKbps = options.Bitrate;
	mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_VBR /*MFX_RATECONTROL_VCM*/;
	mfxEncParams.mfx.FrameInfo.FrameRateExtN = options.FrameRateN;
	mfxEncParams.mfx.FrameInfo.FrameRateExtD = options.FrameRateD;
	mfxEncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	mfxEncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	mfxEncParams.mfx.FrameInfo.CropX = 0;
	mfxEncParams.mfx.FrameInfo.CropY = 0;
	mfxEncParams.mfx.FrameInfo.CropW = options.Width;
	mfxEncParams.mfx.FrameInfo.CropH = options.Height;
	mfxEncParams.AsyncDepth = 1;
	mfxEncParams.mfx.GopRefDist = 1;
	mfxEncParams.mfx.NumRefFrame = 1;
	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	mfxEncParams.mfx.FrameInfo.Width = MSDK_ALIGN(options.Width);
	mfxEncParams.mfx.FrameInfo.Height =
		(MFX_PICSTRUCT_PROGRESSIVE == mfxEncParams.mfx.FrameInfo.PicStruct) ?
		MSDK_ALIGN16(options.Height) :
		MSDK_ALIGN32(options.Height);

	if (impl_type == MFX_IMPL_SOFTWARE)
	{
		mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
	}
	else
	{
		mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
	}

	return sts;
}
mfxStatus IntelEncoder::QueryAndAllocRequiredSurfacesSW()
{
	mfxStatus sts = MFX_ERR_NONE;

	// Query number of required surfaces for encoder

	mfxFrameAllocRequest EncRequest;
	memset(&EncRequest, 0, sizeof(EncRequest));
	sts = mfxENC->QueryIOSurf(&mfxEncParams, &EncRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	

	// Query number of required surfaces for VPP
	mfxFrameAllocRequest VPPRequest[2];     // [0] - in, [1] - out
	memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest) * 2);
	sts = mfxVPP->QueryIOSurf(&VPPParams, VPPRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	EncRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT;     // surfaces are shared between VPP output and encode input

	// Determine the required number of surfaces for VPP input and for VPP output (encoder input)
	nSurfNumVPPIn = VPPRequest[0].NumFrameSuggested;
	nSurfNumVPPOutEnc = EncRequest.NumFrameSuggested + VPPRequest[1].NumFrameSuggested; //TBC
	//mfxU16 nVPPSurfNumOut = VPPRequest[1].NumFrameSuggested; 

	EncRequest.NumFrameSuggested = nSurfNumVPPOutEnc;

	//mfxU16 nEncSurfNum = EncRequest.NumFrameSuggested;

	//VPPRequest[0].Type |= WILL_WRITE; // This line is only required for Windows DirectX11 to ensure that surfaces can be written to by the application
	//VPPRequest[1].Type |= WILL_READ; // This line is only required for Windows DirectX11 to ensure that surfaces can be retrieved by the application


	// Allocate surfaces for VPP: In
	// - Width and height of buffer must be aligned, a multiple of 32
	// - Frame surface array keeps pointers all surface planes and general frame info
	mfxU16 width = (mfxU16)MSDK_ALIGN(VPPParams.vpp.In.Width);
	mfxU16 height = (mfxU16)MSDK_ALIGN16(VPPParams.vpp.In.Height);
	mfxU8 bitsPerPixel = 32;        // NV12 format is a 12 bits per pixel format
	mfxU32 surfaceSize = width * height * bitsPerPixel / 8;
	mfxU8* surfaceBuffersIn = (mfxU8*) new mfxU8[surfaceSize * nSurfNumVPPIn];
	
	pmfxSurfacesVPPIn = new mfxFrameSurface1 *[nSurfNumVPPIn];
	MSDK_CHECK_POINTER(pmfxSurfacesVPPIn, MFX_ERR_MEMORY_ALLOC);
	for (int i = 0; i < nSurfNumVPPIn; i++) {
		pmfxSurfacesVPPIn[i] = new mfxFrameSurface1;
		memset(pmfxSurfacesVPPIn[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pmfxSurfacesVPPIn[i]->Info), &(VPPParams.vpp.In), sizeof(mfxFrameInfo));
		pmfxSurfacesVPPIn[i]->Data.B = &surfaceBuffersIn[surfaceSize * i];
		pmfxSurfacesVPPIn[i]->Data.G = pmfxSurfacesVPPIn[i]->Data.B + 1;
		pmfxSurfacesVPPIn[i]->Data.R = pmfxSurfacesVPPIn[i]->Data.B + 2;
		pmfxSurfacesVPPIn[i]->Data.A = pmfxSurfacesVPPIn[i]->Data.B + 3;
		/*pmfxSurfacesVPPIn[i]->Data.U = pmfxSurfacesVPPIn[i]->Data.Y + width * height;
		pmfxSurfacesVPPIn[i]->Data.V = pmfxSurfacesVPPIn[i]->Data.U + 1;*/
		pmfxSurfacesVPPIn[i]->Data.Pitch =  width * bitsPerPixel / 8;
		/*if (!bEnableInput) {
			ClearYUVSurfaceSysMem(pmfxSurfacesVPPIn[i], width, height);
		}*/
	}

	// Allocate surfaces for VPP: Out
	width = (mfxU16)MSDK_ALIGN(VPPParams.vpp.Out.Width);
	height = (mfxU16)MSDK_ALIGN16(VPPParams.vpp.Out.Height);
	surfaceSize = width * height * bitsPerPixel / 8;
	mfxU8* surfaceBuffersOut = (mfxU8*) new mfxU8[surfaceSize * nSurfNumVPPOutEnc];

	pVPPSurfacesVPPOutEnc = new mfxFrameSurface1 *[nSurfNumVPPOutEnc];
	MSDK_CHECK_POINTER(pVPPSurfacesVPPOutEnc, MFX_ERR_MEMORY_ALLOC);
	for (int i = 0; i < nSurfNumVPPOutEnc; i++) {
		pVPPSurfacesVPPOutEnc[i] = new mfxFrameSurface1;
		memset(pVPPSurfacesVPPOutEnc[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pVPPSurfacesVPPOutEnc[i]->Info), &(VPPParams.vpp.Out), sizeof(mfxFrameInfo));
		pVPPSurfacesVPPOutEnc[i]->Data.Y = &surfaceBuffersOut[surfaceSize * i];
		pVPPSurfacesVPPOutEnc[i]->Data.U = pVPPSurfacesVPPOutEnc[i]->Data.Y + width * height;
		pVPPSurfacesVPPOutEnc[i]->Data.V = pVPPSurfacesVPPOutEnc[i]->Data.U + 1;
		pVPPSurfacesVPPOutEnc[i]->Data.Pitch = width;
	}

	return sts;
}
mfxStatus IntelEncoder::QueryAndAllocRequiredSurfacesHW()
{
	mfxStatus sts = MFX_ERR_NONE;
	// Query number of required surfaces for encoder
	mfxFrameAllocRequest EncRequest;
	memset(&EncRequest, 0, sizeof(EncRequest));
	sts = mfxENC->QueryIOSurf(&mfxEncParams, &EncRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Query number of required surfaces for VPP
	mfxFrameAllocRequest VPPRequest[2];     // [0] - in, [1] - out
	memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest) * 2);
	sts = mfxVPP->QueryIOSurf(&VPPParams, VPPRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	EncRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT;     // surfaces are shared between VPP output and encode input

	// Determine the required number of surfaces for VPP input and for VPP output (encoder input)
	nSurfNumVPPIn = VPPRequest[0].NumFrameSuggested;
	nSurfNumVPPOutEnc = EncRequest.NumFrameSuggested + VPPRequest[1].NumFrameSuggested;

	EncRequest.NumFrameSuggested = nSurfNumVPPOutEnc;

	VPPRequest[0].Type |= WILL_WRITE; // This line is only required for Windows DirectX11 to ensure that surfaces can be written to by the application

	// Allocate required surfaces
	sts = pMfxAllocator->Alloc(pMfxAllocator->pthis, &VPPRequest[0], &mfxResponseVPPIn);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	sts = pMfxAllocator->Alloc(pMfxAllocator->pthis, &EncRequest, &mfxResponseVPPOutEnc);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Allocate surface headers (mfxFrameSurface1) for VPPIn
	pmfxSurfacesVPPIn = new mfxFrameSurface1 *[nSurfNumVPPIn];
	MSDK_CHECK_POINTER(pmfxSurfacesVPPIn, MFX_ERR_MEMORY_ALLOC);
	for (int i = 0; i < nSurfNumVPPIn; i++) {
		pmfxSurfacesVPPIn[i] = new mfxFrameSurface1;
		memset(pmfxSurfacesVPPIn[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pmfxSurfacesVPPIn[i]->Info), &(VPPParams.vpp.In), sizeof(mfxFrameInfo));
		pmfxSurfacesVPPIn[i]->Data.MemId = mfxResponseVPPIn.mids[i];
	}

	pVPPSurfacesVPPOutEnc = new mfxFrameSurface1 *[nSurfNumVPPOutEnc];
	MSDK_CHECK_POINTER(pVPPSurfacesVPPOutEnc, MFX_ERR_MEMORY_ALLOC);
	for (int i = 0; i < nSurfNumVPPOutEnc; i++) {
		pVPPSurfacesVPPOutEnc[i] = new mfxFrameSurface1;
		memset(pVPPSurfacesVPPOutEnc[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pVPPSurfacesVPPOutEnc[i]->Info), &(VPPParams.vpp.Out), sizeof(mfxFrameInfo));
		pVPPSurfacesVPPOutEnc[i]->Data.MemId = mfxResponseVPPOutEnc.mids[i];
	}
	return sts;
}

mfxStatus IntelEncoder::RunVppAndEncode()
{
	mfxStatus sts = MFX_ERR_NONE;
	// ===================================
	// Start processing frames
	//

	mfxGetTime(&tStart);

	nEncSurfIdx = 0;
	nVPPSurfIdx = 0;
	nFrame = 0;

	//
	// Stage 1: Main VPP/encoding loop
	//
	while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
	{
		nVPPSurfIdx = GetFreeSurfaceIndex(pmfxSurfacesVPPIn, nSurfNumVPPIn);    // Find free input frame surface
		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nVPPSurfIdx, MFX_ERR_MEMORY_ALLOC);

		if (impl_type == MFX_IMPL_SOFTWARE)
		{
			//sts = LoadRawRGBFrame(pmfxSurfacesVPPIn[nVPPSurfIdx], fSource);  // Load frame from file into surface
			sts = LoadRawRGBFrameDDA(pmfxSurfacesVPPIn[nVPPSurfIdx], &DuplMgr);  // Load frame from file into surface
			MSDK_BREAK_ON_ERROR(sts);
		}
		else
		{
			// Surface locking required when read/write video surfaces
			sts = pMfxAllocator->Lock(pMfxAllocator->pthis, pmfxSurfacesVPPIn[nVPPSurfIdx]->Data.MemId, &(pmfxSurfacesVPPIn[nVPPSurfIdx]->Data));
			MSDK_BREAK_ON_ERROR(sts);

			//sts = LoadRawRGBFrame(pmfxSurfacesVPPIn[nVPPSurfIdx], fSource);  // Load frame from file into surface
			sts = LoadRawRGBFrameDDA(pmfxSurfacesVPPIn[nVPPSurfIdx], &DuplMgr);  // Load frame from file into surface
			MSDK_BREAK_ON_ERROR(sts);

			sts = pMfxAllocator->Unlock(pMfxAllocator->pthis, pmfxSurfacesVPPIn[nVPPSurfIdx]->Data.MemId, &(pmfxSurfacesVPPIn[nVPPSurfIdx]->Data));
			MSDK_BREAK_ON_ERROR(sts);

		}
		nEncSurfIdx = GetFreeSurfaceIndex(pVPPSurfacesVPPOutEnc, nSurfNumVPPOutEnc);    // Find free output frame surface
		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nEncSurfIdx, MFX_ERR_MEMORY_ALLOC);

		for (;;) {
			// Process a frame asychronously (returns immediately)
			sts = mfxVPP->RunFrameVPPAsync(pmfxSurfacesVPPIn[nVPPSurfIdx], pVPPSurfacesVPPOutEnc[nEncSurfIdx], NULL, &syncpVPP);
			if (MFX_WRN_DEVICE_BUSY == sts) {
				MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
			}
			else
				break;
		}

		if (MFX_ERR_MORE_DATA == sts)
			continue;

		// MFX_ERR_MORE_SURFACE means output is ready but need more surface (example: Frame Rate Conversion 30->60)
		// * Not handled in this example!

		MSDK_BREAK_ON_ERROR(sts);

		for (;;) {
			// Encode a frame asychronously (returns immediately)
			sts = mfxENC->EncodeFrameAsync(NULL, pVPPSurfacesVPPOutEnc[nEncSurfIdx], &mfxBS, &syncpEnc);

			if (MFX_ERR_NONE < sts && !syncpEnc) {  // Repeat the call if warning and no output
				if (MFX_WRN_DEVICE_BUSY == sts)
					MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
			}
			else if (MFX_ERR_NONE < sts && syncpEnc) {
				sts = MFX_ERR_NONE;     // Ignore warnings if output is available
				break;
			}
			else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
				// Allocate more bitstream buffer memory here if needed...
				break;
			}
			else
				break;
		}

		if (MFX_ERR_NONE == sts) {
			sts = pSession->SyncOperation(syncpEnc, 60000);   // Synchronize. Wait until encoded frame is ready
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

			sts = WriteBitStreamFrame(&mfxBS, fSink);
			MSDK_BREAK_ON_ERROR(sts);

			++nFrame;
			printf("Frame number: %d\r", nFrame);
		}
	}

	// MFX_ERR_MORE_DATA means that the input file has ended, need to go to buffering loop, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfxGetTime(&tEnd);
	elapsed = TimeDiffMsec(tEnd, tStart) / 1000;
	double fps = ((double)nFrame / elapsed);
	printf("\nExecution time: %3.2f s (%3.2f fps)\n", elapsed, fps);
}

mfxStatus IntelEncoder::FlushVppAndEncoder()
{
	mfxGetTime(&tStart);
	//
	// Stage 2: Retrieve the buffered VPP frames
	//
	mfxStatus sts = MFX_ERR_NONE;
	while (MFX_ERR_NONE <= sts) {
		nEncSurfIdx = GetFreeSurfaceIndex(pVPPSurfacesVPPOutEnc, nSurfNumVPPOutEnc);    // Find free output frame surface
		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nEncSurfIdx, MFX_ERR_MEMORY_ALLOC);

		for (;;) {
			// Process a frame asychronously (returns immediately)
			sts = mfxVPP->RunFrameVPPAsync(NULL, pVPPSurfacesVPPOutEnc[nEncSurfIdx], NULL, &syncpVPP);
			if (MFX_WRN_DEVICE_BUSY == sts) {
				MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
			}
			else
				break;
		}

		MSDK_BREAK_ON_ERROR(sts);

		for (;;) {
			// Encode a frame asychronously (returns immediately)
			sts = mfxENC->EncodeFrameAsync(NULL, pVPPSurfacesVPPOutEnc[nEncSurfIdx], &mfxBS, &syncpEnc);

			if (MFX_ERR_NONE < sts && !syncpEnc) {  // Repeat the call if warning and no output
				if (MFX_WRN_DEVICE_BUSY == sts)
					MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
			}
			else if (MFX_ERR_NONE < sts && syncpEnc) {
				sts = MFX_ERR_NONE;     // Ignore warnings if output is available
				break;
			}
			else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
				// Allocate more bitstream buffer memory here if needed...
				break;
			}
			else
				break;
		}

		if (MFX_ERR_NONE == sts) {
			sts = pSession->SyncOperation(syncpEnc, 60000);   // Synchronize. Wait until encoded frame is ready
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

			sts = WriteBitStreamFrame(&mfxBS, fSink);
			MSDK_BREAK_ON_ERROR(sts);

			++nFrame;
			printf("Frame number: %d\r", nFrame);
		}
	}

	// MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	//
	// Stage 3: Retrieve the buffered encoder frames
	//
	while (MFX_ERR_NONE <= sts) {
		for (;;) {
			// Encode a frame asychronously (returns immediately)
			sts = mfxENC->EncodeFrameAsync(NULL, NULL, &mfxBS, &syncpEnc);

			if (MFX_ERR_NONE < sts && !syncpEnc) {  // Repeat the call if warning and no output
				if (MFX_WRN_DEVICE_BUSY == sts)
					MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
			}
			else if (MFX_ERR_NONE < sts && syncpEnc) {
				sts = MFX_ERR_NONE;     // Ignore warnings if output is available
				break;
			}
			else
				break;
		}

		if (MFX_ERR_NONE == sts) {
			sts = pSession->SyncOperation(syncpEnc, 60000);   // Synchronize. Wait until encoded frame is ready
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

			//sts = WriteBitStreamFrame(&mfxBS, fSink);
			MSDK_BREAK_ON_ERROR(sts);

			++nFrame;
			printf("Frame number: %d\r", nFrame);
		}
	}

	// MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfxGetTime(&tEnd);
	elapsed += TimeDiffMsec(tEnd, tStart) / 1000;
	double fps = ((double)nFrame / elapsed);
	printf("\nExecution time: %3.2f s (%3.2f fps)\n", elapsed, fps);
}

mfxStatus LoadRawRGBFrameDDA(mfxFrameSurface1* pSurface, DUPLICATIONMANAGER* DuplMgr)
{

	DUPL_RETURN Ret;
	static int frameCount = 0;
	if (500 > frameCount++)
	{
		Ret = DuplMgr->GetFrame(pSurface);
		if (Ret != DUPL_RETURN_SUCCESS)
		{
			fprintf_s(stdout, "Could not get the frame.");
			return MFX_ERR_MORE_DATA;
		}
		return MFX_ERR_NONE;
	}
	else
	{
		return MFX_ERR_MORE_DATA;
	}
}

void IntelEncoder::CloseResources()
{
	// ===================================================================
	// Clean up resources
	//  - It is recommended to close Media SDK components first, before releasing allocated surfaces, since
	//    some surfaces may still be locked by internal Media SDK resources.

	mfxENC->Close();
	mfxVPP->Close();
	// session closed automatically on destruction

	for (int i = 0; i < nSurfNumVPPIn; i++)
		delete pmfxSurfacesVPPIn[i];
	MSDK_SAFE_DELETE_ARRAY(pmfxSurfacesVPPIn);
	for (int i = 0; i < nSurfNumVPPOutEnc; i++)
		delete pVPPSurfacesVPPOutEnc[i];
	MSDK_SAFE_DELETE_ARRAY(pVPPSurfacesVPPOutEnc);
	MSDK_SAFE_DELETE_ARRAY(mfxBS.Data);
	MSDK_SAFE_DELETE_ARRAY(extDoNotUse.AlgList);

	pMfxAllocator->Free(pMfxAllocator->pthis, &mfxResponseVPPIn);
	pMfxAllocator->Free(pMfxAllocator->pthis, &mfxResponseVPPOutEnc);

	free(pMfxAllocator);
	delete pSession;

	if (fSink) fclose(fSink);

	Release();
}