#pragma once

#include "reaper_plugin_functions.h"
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include "JuceHeader.h"

class SID_PCM_Source : public PCM_source, public Timer
{
public:

	SID_PCM_Source();
	~SID_PCM_Source();

	void timerCallback() override;

	// Inherited via PCM_source
	PCM_source * Duplicate() override;

	bool IsAvailable() override;

	const char * GetType() override;

	bool SetFileName(const char * newfn) override;

	const char *GetFileName() override;

	int GetNumChannels() override;

	double GetSampleRate() override;

	double GetLength() override;

	int PropertiesWindow(HWND hwndParent) override;

	void GetSamples(PCM_source_transfer_t * block) override;

	void GetPeakInfo(PCM_source_peaktransfer_t * block) override;

	void SaveState(ProjectStateContext * ctx) override;

	int LoadState(const char * firstline, ProjectStateContext * ctx) override;

	void Peaks_Clear(bool deleteFile) override;

	int PeaksBuild_Begin() override;

	int PeaksBuild_Run() override;

	void PeaksBuild_Finish() override;

	int Extended(int call, void *parm1, void *parm2, void *parm3) override;

private:
	void renderSID();
	void renderSIDintoMultichannel(String outfn, String outdir);
	void adjustParentTrackChannelCount();
	std::unique_ptr<PCM_source> m_playsource;
	String m_sidfn;
	String m_displaysidname;
	double m_sidlen = 60.0;
	int m_sid_track = 0;
	int m_sid_channelmode = 0;
	int m_sid_sr = 44100;
	MediaItem* m_item = nullptr;
};
