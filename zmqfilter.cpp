
#include "zmqfilter.h"
#include <string>



typedef struct {
	int width;
	int height;
} FrameSize;

static FrameSize frameSizes[] = {

	// 4 x 3
	{80, 60},
	{160, 120},
	{240, 180},
	{320, 240},
	{400, 300},
	{480, 360},
	{560, 420},
	{640, 480},
	{800, 600},
	{960, 720},


	// 16 x 9
	{80, 45},
	{160, 90},
	{240, 135},
	{320, 180},
	{400, 225},
	{480, 270},
	{560, 315},
	{640, 360},
	{800, 450},
	{1280, 720},



};

#define DEFAULT_FRAME_SIZE_INDEX (7)
#define FRAME_SIZE_COUNT (sizeof(frameSizes) / sizeof(FrameSize))



//////////////////////////////////////////////////////////////////////////
//  CVCam is the source filter which masquerades as a capture device
//////////////////////////////////////////////////////////////////////////
CUnknown * WINAPI CVCam::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    ASSERT(phr);
    CUnknown *punk = new CVCam(lpunk, phr);
    return punk;
}

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT *phr) : 
    CSource(NAME("Virtual Cam"), lpunk, CLSID_ZMQVirtualCam)
{
    ASSERT(phr);
    CAutoLock cAutoLock(&m_cStateLock);
    // Create the one and only output pin
    m_paStreams = (CSourceStream **) new CVCamStream*[1];
    m_paStreams[0] = new CVCamStream(phr, this, FILTERNAME);
}

HRESULT CVCam::QueryInterface(REFIID riid, void **ppv)
{
    //Forward request for IAMStreamConfig & IKsPropertySet to the pin
    if(riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet))
        return m_paStreams[0]->QueryInterface(riid, ppv);
    else
        return CSource::QueryInterface(riid, ppv);
}

//////////////////////////////////////////////////////////////////////////
// CVCamStream is the one and only output pin of CVCam which handles 
// all the stuff.
//////////////////////////////////////////////////////////////////////////
CVCamStream::CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName) :
    CSourceStream(NAME("Virtual Cam"),phr, pParent, pPinName), 
	m_pParent(pParent),
	m_MQCtx(zmq_ctx_new()),	
	m_MQSocket(nullptr)
{
    // Set the default media type as 320x240x24@15
    GetMediaType(DEFAULT_FRAME_SIZE_INDEX+1, &m_mt);
	createSocket();
}

CVCamStream::~CVCamStream()
{
	closeSocket();

	if (m_MQCtx)
	{
		zmq_ctx_destroy(m_MQCtx);
		m_MQCtx = nullptr;
	}
} 


void dprintf(const char * i, int a,int b)
{
	char buf[1000];
	sprintf_s(buf, 1000, i, a, b);
	OutputDebugStringA(buf);
}

bool CVCamStream::createSocket()
{	
	closeSocket();
	m_MQSocket = zmq_socket(m_MQCtx, ZMQ_REQ);
	int i = zmq_connect(m_MQSocket, "tcp://localhost:5558");	
	int val = 500; // 500 ms.
	zmq_setsockopt(m_MQSocket, ZMQ_RCVTIMEO, &val, sizeof(val));
	zmq_setsockopt(m_MQSocket, ZMQ_SNDTIMEO, &val, sizeof(val));
	return true;
}

void CVCamStream::closeSocket()
{
	if (m_MQSocket)
	{
		zmq_close(m_MQSocket);
	}
}

HRESULT CVCamStream::QueryInterface(REFIID riid, void **ppv)
{   
    // Standard OLE stuff
    if(riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if(riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);

    AddRef();
    return S_OK;
}


//////////////////////////////////////////////////////////////////////////
//  This is the routine where we create the data being output by the Virtual
//  Camera device.
//////////////////////////////////////////////////////////////////////////

HRESULT CVCamStream::FillBuffer(IMediaSample *pms)
{
    REFERENCE_TIME rtNow;
    
    REFERENCE_TIME avgFrameTime = ((VIDEOINFOHEADER*)m_mt.pbFormat)->AvgTimePerFrame;

    rtNow = m_rtLastTime;
    m_rtLastTime += avgFrameTime;
	
    BYTE *pData;
    long lDataLen;
    pms->GetPointer(&pData);
    lDataLen = pms->GetSize();

	std::string getFrame = "GET_FRAME " + std::to_string(((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biWidth) + " " + std::to_string(((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biHeight) + " " + std::to_string(((VIDEOINFOHEADER*)m_mt.pbFormat)->bmiHeader.biBitCount) + " " + std::to_string(lDataLen);
	int status = zmq_send(m_MQSocket, getFrame.c_str(), getFrame.length(), 0);
	if (status == EAGAIN)
	{
		createSocket();
	}
	else
	{
		status = zmq_recv(m_MQSocket, pData, lDataLen, 0);
		if (status == EAGAIN)
		{
			createSocket();
		}
	}
    
	// set PTS (presentation) timestamps...
	CRefTime now;
	m_pParent->StreamTime(now);
	REFERENCE_TIME endThisFrame = now + avgFrameTime;
    pms->SetTime((REFERENCE_TIME *) &now, &endThisFrame);
    pms->SetSyncPoint(TRUE);
    return NOERROR;
} // FillBuffer


//
// Notify
// Ignore quality management messages sent from the downstream filter
STDMETHODIMP CVCamStream::Notify(IBaseFilter * pSender, Quality q)
{
    return E_NOTIMPL;
} // Notify

//////////////////////////////////////////////////////////////////////////
// This is called when the output format has been negotiated
//////////////////////////////////////////////////////////////////////////
HRESULT CVCamStream::SetMediaType(const CMediaType *pmt)
{	
    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->Format());
	dprintf("******** SetMediaType %d %d", pvi->bmiHeader.biWidth, pvi->bmiHeader.biHeight);
    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CVCamStream::GetMediaType(int iPosition, CMediaType *pmt)
{
    if(iPosition < 0) return E_INVALIDARG;
    if(iPosition > FRAME_SIZE_COUNT) return VFW_S_NO_MORE_ITEMS;

    if(iPosition == 0) 
    {
        *pmt = m_mt;
        return S_OK;
    }

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount    = 24;
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth      = frameSizes[ iPosition - 1 ].width;
    pvi->bmiHeader.biHeight     = frameSizes[iPosition - 1].height;
    pvi->bmiHeader.biPlanes     = 1;
    pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;
	dprintf("******** GetMediaType %d %d", pvi->bmiHeader.biWidth, pvi->bmiHeader.biHeight);
    pvi->AvgTimePerFrame = 1000000;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    // Work out the GUID for the subtype from the header info.
    const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
    pmt->SetSubtype(&SubTypeGUID);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
    
    return NOERROR;

} // GetMediaType

// This method is called to see if a given output format is supported
HRESULT CVCamStream::CheckMediaType(const CMediaType *pMediaType)
{		
    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)(pMediaType->Format());
	dprintf("******** CheckMediaType %d %d", pvi->bmiHeader.biWidth, pvi->bmiHeader.biHeight);
	if (pvi->bmiHeader.biBitCount != 24 || pvi->bmiHeader.biCompression !=BI_RGB )
	{
		dprintf("******** CheckMediaType invalid bitcount or compression %d %d", pvi->bmiHeader.biWidth, pvi->bmiHeader.biHeight);
		return E_INVALIDARG;
	}

	for (int i = 0; i < FRAME_SIZE_COUNT; i++)
	{
		if (frameSizes[i].width == pvi->bmiHeader.biWidth && frameSizes[i].height == pvi->bmiHeader.biHeight)
		{
			return S_OK;
		}
	}

	dprintf("******** CheckMediaType invalid %d %d", pvi->bmiHeader.biWidth, pvi->bmiHeader.biHeight);
	return E_INVALIDARG;
} // CheckMediaType

// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CVCamStream::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) m_mt.Format();
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);

    if(FAILED(hr)) return hr;
    if(Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return NOERROR;
} // DecideBufferSize

// Called when graph is run
HRESULT CVCamStream::OnThreadCreate()
{
    m_rtLastTime = 0;
    return NOERROR;
} // OnThreadCreate


//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE *pmt)
{
	
    DECLARE_PTR(VIDEOINFOHEADER, pvi, m_mt.pbFormat);
    m_mt = *pmt;
	dprintf("******** SetFormat %d %d", pvi->bmiHeader.biWidth, pvi->bmiHeader.biHeight);
    IPin* pin; 
    ConnectedTo(&pin);
    if(pin)
    {
        IFilterGraph *pGraph = m_pParent->GetGraph();
        pGraph->Reconnect(this);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetFormat(AM_MEDIA_TYPE **ppmt)
{
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetNumberOfCapabilities(int *piCount, int *piSize)
{
    *piCount = FRAME_SIZE_COUNT;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
	dprintf("******** GetNumberOfCapabilities %d %d", 0,0);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{
    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(VIDEOINFOHEADER, pvi, (*pmt)->pbFormat);

    if (iIndex == 0) iIndex = DEFAULT_FRAME_SIZE_INDEX + 1;

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount    = 24;
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
	pvi->bmiHeader.biWidth = frameSizes[iIndex-1].width;
	pvi->bmiHeader.biHeight = frameSizes[iIndex-1].height;
    pvi->bmiHeader.biPlanes     = 1;
    pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    (*pmt)->majortype = MEDIATYPE_Video;
    (*pmt)->subtype = MEDIASUBTYPE_RGB24;
    (*pmt)->formattype = FORMAT_VideoInfo;
    (*pmt)->bTemporalCompression = FALSE;
    (*pmt)->bFixedSizeSamples= FALSE;
    (*pmt)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*pmt)->cbFormat = sizeof(VIDEOINFOHEADER);
    
    DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);
    
    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;
    pvscc->InputSize.cx = 640;
    pvscc->InputSize.cy = 480;
    pvscc->MinCroppingSize.cx = frameSizes[0].width;
    pvscc->MinCroppingSize.cy = frameSizes[0].height;
    pvscc->MaxCroppingSize.cx = frameSizes[FRAME_SIZE_COUNT].width;
    pvscc->MaxCroppingSize.cy = frameSizes[FRAME_SIZE_COUNT].height;
    pvscc->CropGranularityX = 80;
    pvscc->CropGranularityY = 30;
    pvscc->CropAlignX = 0;
    pvscc->CropAlignY = 0;

    pvscc->MinOutputSize.cx = frameSizes[0].width;
    pvscc->MinOutputSize.cy = frameSizes[0].height;
    pvscc->MaxOutputSize.cx = frameSizes[FRAME_SIZE_COUNT].width;
    pvscc->MaxOutputSize.cy = frameSizes[FRAME_SIZE_COUNT].height;
    pvscc->OutputGranularityX = 0;
    pvscc->OutputGranularityY = 0;
    pvscc->StretchTapsX = 0;
    pvscc->StretchTapsY = 0;
    pvscc->ShrinkTapsX = 0;
    pvscc->ShrinkTapsY = 0;
    pvscc->MinFrameInterval = 200000;   //50 fps
    pvscc->MaxFrameInterval = 50000000; // 0.2 fps
    pvscc->MinBitsPerSecond = (80 * 60 * 3 * 8) / 5;
    pvscc->MaxBitsPerSecond = 640 * 480 * 3 * 8 * 50;
	dprintf("******** GetStreamCaps %d %d", pvi->bmiHeader.biWidth, pvi->bmiHeader.biHeight);
    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////


HRESULT CVCamStream::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, 
                        DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{// Set: Cannot set any properties.
    return E_NOTIMPL;
}

// Get: Return the pin category (our only property). 
HRESULT CVCamStream::Get(
    REFGUID guidPropSet,   // Which property set.
    DWORD dwPropID,        // Which property in that set.
    void *pInstanceData,   // Instance data (ignore).
    DWORD cbInstanceData,  // Size of the instance data (ignore).
    void *pPropData,       // Buffer to receive the property data.
    DWORD cbPropData,      // Size of the buffer.
    DWORD *pcbReturned     // Return the size of the property.
)
{
    if (guidPropSet != AMPROPSETID_Pin)             return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)        return E_PROP_ID_UNSUPPORTED;
    if (pPropData == NULL && pcbReturned == NULL)   return E_POINTER;
    
    if (pcbReturned) *pcbReturned = sizeof(GUID);
    if (pPropData == NULL)          return S_OK; // Caller just wants to know the size. 
    if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.
        
    *(GUID *)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CVCamStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET; 
    return S_OK;
}
