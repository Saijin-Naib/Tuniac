#include "StdAfx.h"
#include <emmintrin.h>
#include "svprenderer.h"

extern HANDLE	hInst;

typedef BOOL (APIENTRY *PFNWGLSWAPINTERVALFARPROC)( int );
PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = 0;

void setVSync(int interval=1)
{
	const char *extensions = (char*)glGetString( GL_EXTENSIONS );

	if( strstr( extensions, "WGL_EXT_swap_control" ) == 0 )
		return; // Error: WGL_EXT_swap_control extension not supported on your computer.\n");
	else
	{
		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALFARPROC)wglGetProcAddress( "wglSwapIntervalEXT" );

		if( wglSwapIntervalEXT )
			wglSwapIntervalEXT(interval);
	}
}

__m64 Get_m64(__int64 n)
{
    union __m64__m64
    {
        __m64 m;
        __int64 i;
    } mi;

    mi.i = n;
    return mi.m;
}

#if defined (_M_IX86)

void ChangeBrightnessC_MMX(
    unsigned long* pSource, 
    unsigned long* pDest, 
    int nNumberOfPixels, 
    int nChange)
{
    if ( nChange > 255 )
        nChange = 255;
    else if ( nChange < -255 )
        nChange = -255;

    BYTE b = (BYTE) abs(nChange);

    // make 64 bits value with b in each byte
    __int64 c = b;

    for ( int i = 1; i <= 7; i++ )
    {
        c = c << 8;
        c |= b;
    }

    // 8 pixels are processed in one loop
    int nNumberOfLoops = nNumberOfPixels / 8;

    __m64* pIn = (__m64*) pSource;          // input pointer
    __m64* pOut = (__m64*) pDest;           // output pointer

    __m64 tmp;                              // work variable


    _mm_empty();                            // emms

    __m64 nChange64 = Get_m64(c);

    if ( nChange > 0 )
    {
        for ( int i = 0; i < nNumberOfLoops; i++ )
        {
            tmp = _m_paddusb(*pIn, nChange64); // Unsigned addition 
                                                 // with saturation.
                                                 // tmp = *pIn + nChange64
                                                 // for each byte

            *pOut = tmp;

            pIn++;                               // next 8 pixels
            pOut++;
        }
    }
    else
    {
        for ( int i = 0; i < nNumberOfLoops; i++ )
        {
            tmp = _mm_subs_pu8(*pIn, nChange64); // Unsigned subtraction 
                                                 // with saturation.
                                                 // tmp = *pIn - nChange64
                                                 // for each byte

            *pOut = tmp;

            pIn++;                                      // next 8 pixels
            pOut++;
        }
    }

    _mm_empty();                            // emms
}

#else

void ChangeBrightnessC_MMX(
    unsigned long* pSource, 
    unsigned long* pDest, 
    int nNumberOfPixels, 
    int nChange)
{
	for(int rep = 0; rep < nNumberOfPixels; rep++)
	{
		int x = (int)*pSource;

		x-=nChange;

		if(x<0)
			x=0;

		*pDest = x;

		pSource++;
	}
}

#endif

bool SVPRenderer::AddFolderOfSVP(LPTSTR	szFolder)
{
	WIN32_FIND_DATA		w32fd;
	HANDLE				hFind;

	TCHAR	szFilename[1024];

	StrCpyN(szFilename, szFolder, 1024);
	PathAddBackslash(szFilename);
	StrCat(szFilename, TEXT("*.*"));

	hFind = FindFirstFile( szFilename, &w32fd); 
	if(hFind != INVALID_HANDLE_VALUE) 
	{
		do
		{
			if(StrCmp(w32fd.cFileName, TEXT(".")) == 0 || StrCmp(w32fd.cFileName, TEXT("..")) == 0 )
				continue;

			TCHAR temp[1024];

			StrCpy(temp, szFolder);
			PathAddBackslash(temp);
			StrCat(temp, w32fd.cFileName);

			if(GetFileAttributes(temp) & FILE_ATTRIBUTE_DIRECTORY)
			{
				AddFolderOfSVP(temp);	
			}
			else
			{
				LPTSTR ext = PathFindExtension(temp);

				if(!StrCmpI(ext, TEXT(".SVP")))
				{
					LPTSTR string = (LPTSTR)malloc(2048 * sizeof(TCHAR));

					StrCpy(string, temp);

					m_VisFilenameArray.AddTail(string);
				}
			}
		} while(FindNextFile(hFind, &w32fd));

		FindClose(hFind); 
	}

	return true;
}


SVPRenderer::SVPRenderer(void)
{
}

SVPRenderer::~SVPRenderer(void)
{
}

bool SVPRenderer::RenderVisual(void)
{
	if(m_TheVisual)
	{
		unsigned long NumChannels = (unsigned long)m_pHelper->GetVariable(Variable_NumChannels);
		if(NumChannels == INVALID)
			return true;

		if(NumChannels != ulOldNumChannels)
		{
			if(visdata)
			{
				VirtualFree(visdata, 0, MEM_RELEASE);
			}

			ulOldNumChannels = NumChannels;
			visdata = (float *)VirtualAlloc(NULL, 512 * NumChannels * sizeof(float), MEM_COMMIT, PAGE_READWRITE);
		}

		vd.MillSec	= (unsigned long)m_pHelper->GetVariable(Variable_PositionMS);

		m_pHelper->GetVisData(visdata, 512 * NumChannels);

		int sample = 0;
		float fSamples[2][512];
		for(int x=0; x<512; x++)
		{
			if(NumChannels == 1)
			{
				fSamples[0][x] = fSamples[1][x] = visdata[sample];
				vd.Waveform[0][x] = vd.Waveform[1][x] = (visdata[sample]*64.0f);
			}
			else
			{
				fSamples[0][x] = visdata[sample];
				vd.Waveform[0][x] = (visdata[sample]*64.0f);

				fSamples[1][x] = visdata[sample+1];
				vd.Waveform[1][x] = (visdata[sample+1]*64.0f);
			}
			sample+=NumChannels;
		}

		if(m_TheVisual->NeedsSpectrum())
		{
			long tempbuffer = 0;
			//left
			kiss_fftr(kiss_cfg, fSamples[0], freq_data);
			for(int p = 0; p < 256; p++)
			{
				tempbuffer = (freq_data[p].r * 4.0f);
				if(tempbuffer < 0)
					tempbuffer = -tempbuffer;
				vd.Spectrum[1][p] = vd.Spectrum[0][p] = tempbuffer;
			}
			if(NumChannels > 1)
			{
				//right
				kiss_fftr(kiss_cfg, fSamples[1], freq_data);
				for(int p = 0; p < 256; p++)
				{
					tempbuffer = (freq_data[p].r * 4.0f);
					if(tempbuffer < 0)
						tempbuffer = -tempbuffer;
					vd.Spectrum[1][p] = tempbuffer;
				}
			}
		}

		if(m_TheVisual->NeedsVisFX())
			ChangeBrightnessC_MMX(m_textureData, m_textureData, iVisRes*iVisRes*4, -24);

		return m_TheVisual->Render(m_textureData, iVisRes, iVisRes, iVisRes, &vd);
	}

	return true;
}


void	SVPRenderer::Destroy(void)
{
	if(m_textureData)
	{
		VirtualFree(m_textureData, 0, MEM_RELEASE);
		m_textureData = NULL;
	}

	if(visdata)
	{
		VirtualFree(visdata, 0, MEM_RELEASE);
		visdata = NULL;
	}

	kiss_fft_free(kiss_cfg);
	free(freq_data);
	
	m_pHelper->SetVisualPref(TEXT("SVPRenderer"), TEXT("CurrentVis"), REG_DWORD, (LPBYTE)&m_SelectedVisual, sizeof(int));
	delete this;
}

LPTSTR	SVPRenderer::GetPluginName(void)
{
	return TEXT("Sonique SVP Visual Plugin");
}

unsigned long SVPRenderer::GetFlags(void)
{
	return PLUGINFLAGS_CONFIG | PLUGINFLAGS_ABOUT;
}

bool	SVPRenderer::SetHelper(ITuniacVisHelper *pHelper)
{
	m_pHelper = pHelper;
	return true;
}

bool	SVPRenderer::Attach(HDC hDC)
{
	m_LastWidth			= 0;
	m_LastHeight		= 0;
	m_SelectedVisual	= 0;
	m_TheVisual			= NULL;
	iVisRes				= 512;

	TCHAR				szVisualsPath[2048];
	GetModuleFileName((HMODULE)hInst, szVisualsPath, 512);
	PathRemoveFileSpec(szVisualsPath);
	PathAddBackslash(szVisualsPath);
	StrCat(szVisualsPath, TEXT("svp"));
	PathAddBackslash(szVisualsPath);

	AddFolderOfSVP(szVisualsPath);

	kiss_cfg = kiss_fftr_alloc(512,0,NULL,NULL);
	freq_data = (kiss_fft_cpx*)KISS_FFT_MALLOC((512)*sizeof(kiss_fft_cpx));

	DWORD				lpRegType = REG_DWORD;
	DWORD				iRegSize = sizeof(int);

	m_pHelper->GetVisualPref(TEXT("SVPRenderer"), TEXT("VisRes"), &lpRegType, (LPBYTE)&iVisRes, &iRegSize);
	m_textureData = (unsigned long*)VirtualAlloc(NULL, iVisRes*iVisRes*iVisRes*sizeof(unsigned long), MEM_COMMIT, PAGE_READWRITE);

	ulOldNumChannels = (unsigned long)m_pHelper->GetVariable(Variable_NumChannels);
	visdata = (float *)VirtualAlloc(NULL, 512 * ulOldNumChannels * sizeof(float), MEM_COMMIT, PAGE_READWRITE);

	if(m_VisFilenameArray.GetCount())
	{
		lpRegType = REG_DWORD;
		iRegSize = sizeof(int);
		m_pHelper->GetVisualPref(TEXT("SVPRenderer"), TEXT("CurrentVis"), &lpRegType, (LPBYTE)&m_SelectedVisual, &iRegSize);
		if(m_SelectedVisual >= m_VisFilenameArray.GetCount())
			m_SelectedVisual = 0;

		SetActiveVisual(m_SelectedVisual);
	}

	m_hArrow = (HBITMAP)LoadImage((HINSTANCE)hInst, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    ::GetObject (m_hArrow, sizeof (m_ArrowBM), &m_ArrowBM);

	m_glRC = NULL;
	m_glDC = hDC;

	GLuint		PixelFormat;

	PIXELFORMATDESCRIPTOR pfd ;
	memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR)) ;
	pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR); 
	pfd.nVersion   = 1 ; 
	pfd.dwFlags    =	PFD_DOUBLEBUFFER |
						PFD_SUPPORT_OPENGL |
						PFD_DRAW_TO_WINDOW |
						PFD_GENERIC_ACCELERATED;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24 ;
	pfd.cDepthBits = 32 ;
	pfd.iLayerType = PFD_MAIN_PLANE ;

	if (!(PixelFormat=ChoosePixelFormat(m_glDC,&pfd)))
		return false;

	if(!SetPixelFormat(hDC,PixelFormat,&pfd))
		return false;

	if (!(m_glRC=wglCreateContext(m_glDC)))
		return false;

	if(!wglMakeCurrent(m_glDC, m_glRC))
		return false;

	setVSync(1);

	//if(!wglMakeCurrent(m_glDC, m_glRC))
	//	return false;

	glClearColor(0.0, 0.0, 0.0, 0.0);
	//glClear (GL_COLOR_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable (GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,	GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,	GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,		GL_CLAMP  );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,		GL_CLAMP  );

	m_LastMove = GetTickCount();

	return true;
}
bool	SVPRenderer::Detach()
{
	if (m_glRC)
	{
		wglMakeCurrent(NULL,NULL);
		wglDeleteContext(m_glRC);
		m_glRC=NULL;
	}

	m_glDC = NULL;

	if(m_textureData)
	{
		VirtualFree(m_textureData, 0, MEM_RELEASE);
		m_textureData = NULL;
	}

	if(visdata)
	{
		VirtualFree(visdata, 0, MEM_RELEASE);
		visdata = NULL;
	}

	kiss_fft_free(kiss_cfg);
	free(freq_data);
	
	m_pHelper->SetVisualPref(TEXT("SVPRenderer"), TEXT("CurrentVis"), REG_DWORD, (LPBYTE)&m_SelectedVisual, sizeof(int));
	return true;
}

bool	SVPRenderer::Render(int w, int h)
{
	CAutoLock m(&m_RenderLock);

	// just to make sure!
	//if(!wglMakeCurrent(m_glDC, m_glRC))
	//	return false;

	if((m_LastWidth != w) || (m_LastHeight != h))
	{
		m_LastWidth		= w;
		m_LastHeight	= h;

		SetRect(&m_NextVisRect, 
				w-(64+16),
				h-(64+16),
				w-(64+16)+64,
				h-(64+16)+64);

		SetRect(&m_PrevVisRect, 
				(16),
				h-(64+16),
				(16)+64,
				h-(64+16)+64);

		glViewport(0,0,w,h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluOrtho2D(0.0, (float)w, (float)h, 0.0);
		glMatrixMode(GL_MODELVIEW);
		//glLoadIdentity();
	}

	//glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear (GL_COLOR_BUFFER_BIT);
	//glLoadIdentity();

	RenderVisual();

	//the visual
	glTexImage2D(	GL_TEXTURE_2D, 
					0, 
					GL_RGB,
					iVisRes, 
					iVisRes, 
					0,
					GL_BGRA_EXT, 
					GL_UNSIGNED_BYTE, 
					m_textureData);


	glColor4f(1,1,1,1);

	//resize the visual to fit window
	//glEnable(GL_TEXTURE_2D);
	glBegin (GL_QUADS);
		glTexCoord2d(0.0, 0.0);
		glVertex2f( 0.0f,  0.0f);	// Bottom Left Of The Texture and Quad
		glTexCoord2d(0.0, 1.0);
		glVertex2f( 0.0f,  m_LastHeight);	// Bottom Right Of The Texture and Quad
		glTexCoord2d(1.0, 1.0);
		glVertex2f( m_LastWidth, m_LastHeight);	// Top Right Of The Texture and Quad
		glTexCoord2d(1.0, 0.0);
		glVertex2f( m_LastWidth,  0.0f);	// Top Left Of The Texture and Quad
	glEnd ();

	//arrows
	if((GetTickCount() - m_LastMove) < 4000)
	{
		if((GetTickCount() - m_LastMove) < 2000)
		{
			//clear arrow
			glColor4f(1,1,1,1);
		}
		else
		{
			//fade arrow
			int val = (GetTickCount() - m_LastMove) - 2000;
			float scale = (float) (2000-val) / 2000.0f;
			glColor4f(1,1,1,scale);
		}

		//load arrow
		//glEnable(GL_TEXTURE_2D);
		glTexImage2D(	GL_TEXTURE_2D, 
						0, 
						GL_ALPHA,
						m_ArrowBM.bmWidth, 
						m_ArrowBM.bmHeight, 
						0,
						GL_ALPHA, 
						GL_UNSIGNED_BYTE, 
						m_ArrowBM.bmBits);

		//left
		glBegin (GL_QUADS);
			glTexCoord2d(1.0, 1.0); glVertex2i(m_NextVisRect.left,	m_NextVisRect.top);
			glTexCoord2d(1.0, 0.0); glVertex2i(m_NextVisRect.left,	m_NextVisRect.bottom);
			glTexCoord2d(0.0, 0.0); glVertex2i(m_NextVisRect.right, m_NextVisRect.bottom);
			glTexCoord2d(0.0, 1.0); glVertex2i(m_NextVisRect.right,	m_NextVisRect.top);
		glEnd();

		//right
		glBegin (GL_QUADS);
			glTexCoord2d(0.0, 0.0); glVertex2i(m_PrevVisRect.left,	m_PrevVisRect.top);
			glTexCoord2d(0.0, 1.0); glVertex2i(m_PrevVisRect.left,	m_PrevVisRect.bottom);
			glTexCoord2d(1.0, 1.0); glVertex2i(m_PrevVisRect.right, m_PrevVisRect.bottom);
			glTexCoord2d(1.0, 0.0); glVertex2i(m_PrevVisRect.right,	m_PrevVisRect.top);
		glEnd();
	}

	glFlush();
	SwapBuffers(m_glDC);

	return true;
}

bool	SVPRenderer::About(HWND hWndParent)
{
	MessageBox(hWndParent, TEXT("Sonique Visual Plugin renderer. Tony Million 2009"), GetPluginName(), MB_OK | MB_ICONINFORMATION);
	return true;
}
bool	SVPRenderer::Configure(HWND hWndParent)
{
	CAutoLock m(&m_RenderLock);

	if(m_textureData)
	{
		VirtualFree(m_textureData, 0, MEM_RELEASE);
		m_textureData = NULL;
	}
	if(iVisRes == 256)
		iVisRes = 512;
	else if(iVisRes == 512)
		iVisRes = 640;
	else
		iVisRes = 256;

	m_textureData = (unsigned long*)VirtualAlloc(NULL, iVisRes*iVisRes*iVisRes*sizeof(unsigned long), MEM_COMMIT, PAGE_READWRITE);

	m_pHelper->SetVisualPref(TEXT("SVPRenderer"), TEXT("VisRes"), REG_DWORD, (LPBYTE)&iVisRes, sizeof(int));

	return true;
}

bool	SVPRenderer::Notify(unsigned long Notification)
{
	return true;
}

bool SVPRenderer::SetActiveVisual(int visindex)
{
	CAutoLock m(&m_RenderLock);

	// set no visual
	if(visindex == -1)
	{
		m_TheVisual = NULL;
		m_SelectedVisual = 0;
		return true;
	}

	TCHAR	szVisFilename[2048];
	char settingsdir[2048];
	if(m_TheVisual)
	{
		StrCpy(szVisFilename, m_VisFilenameArray[m_SelectedVisual]);
		StrCat(szVisFilename, TEXT(".ini"));
		WideCharToMultiByte(CP_ACP, 0, szVisFilename, 512, settingsdir, 512, NULL, NULL);

		SoniqueVisExternal * t = m_TheVisual;
		m_TheVisual = NULL;
		t->SaveSettings(settingsdir);
		t->Shutdown();
		delete t;
	}

	TCHAR	szPath[2048];
	StrCpy(szPath, m_VisFilenameArray[visindex]);
	PathRemoveFileSpec(szPath);
	PathAddBackslash(szPath);

	StrCpy(szVisFilename, m_VisFilenameArray[visindex]);

	TCHAR oldFolder[2048];
	GetCurrentDirectory(2048, oldFolder);
	SetCurrentDirectory(szPath);

	SoniqueVisExternal * newVis = new SoniqueVisExternal();
	if(newVis->LoadFromExternalDLL(szVisFilename))
	{
		StrCat(szVisFilename, TEXT(".ini"));
		WideCharToMultiByte(CP_ACP, 0, szVisFilename, 512, settingsdir, 512, NULL, NULL);

		newVis->LoadSettings(settingsdir);

		m_TheVisual = newVis;
		m_SelectedVisual = visindex;
	}
	else
	{
		delete newVis;
	}

	SetCurrentDirectory(oldFolder);

	return true;
}

bool	SVPRenderer::MouseFunction(unsigned long function, int x, int y)
{
	if(function == VISUAL_MOUSEFUNCTION_DOWN)
	{
		POINT pt;

		pt.x = x;
		pt.y = y;

		if(m_VisFilenameArray.GetCount())
		{
			if(m_VisFilenameArray.GetCount() == 1)
				return true;

			m_LastMove = GetTickCount();

			if(m_TheVisual==NULL)
			{
				SetActiveVisual(0);
				return true;
			}

			if(PtInRect(&m_NextVisRect, pt))
			{
				if(m_SelectedVisual < m_VisFilenameArray.GetCount()-1)
				{
					SetActiveVisual(m_SelectedVisual+1);
				}
				else
				{
					SetActiveVisual(0);
				}
				return true;
			}
			else if(PtInRect(&m_PrevVisRect, pt))
			{
				if(m_SelectedVisual > 0)
				{
					SetActiveVisual(m_SelectedVisual-1);
				}
				else
				{
					SetActiveVisual(m_VisFilenameArray.GetCount()-1);
				}
				return true;
			}


			// we need to scale the point to a 512x512 point then send it to the vis

			int visx = (int)(((float)x / (float)m_LastWidth) * iVisRes);
			int visy = (int)(((float)y / (float)m_LastHeight) * iVisRes);

			if(m_TheVisual)
				m_TheVisual->Clicked(visx,visy);
		}
	}

	return true;
}
