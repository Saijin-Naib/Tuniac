#pragma once
#include "iaudiosource.h"
#include "Array.h"

class CBASSDecoderPlugin :
	public IAudioSourceSupplier
{
protected:
	IAudioSourceHelper	*	m_pHelper;
	TCHAR				*	exts[40];
	unsigned long			count;

public:
	CBASSDecoderPlugin(void);
	~CBASSDecoderPlugin(void);
protected:
	bool					DoBASSInit();

public:
	virtual void			Destroy(void);

	virtual void			SetHelper(IAudioSourceHelper * pHelper);
	virtual LPTSTR			GetName(void);
	virtual GUID			GetPluginID(void);

	virtual unsigned long	GetFlags(void);

	virtual bool			About(HWND hParent);
	virtual bool			Configure(HWND hParent);

	virtual bool			CanHandle(LPTSTR szSource);
	virtual	unsigned long	GetNumCommonExts(void);
	virtual	LPTSTR			GetCommonExt(unsigned long ulIndex);

	virtual IAudioSource *		CreateAudioSource(LPTSTR szSource, IAudioFileIO * pFileIO);
};