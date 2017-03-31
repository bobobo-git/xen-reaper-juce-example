#include "sid_pcm_source.h"
#include "lineparse.h"

SID_PCM_Source::SID_PCM_Source()
{
	//ShowConsoleMsg("SID ctor\n");
}

SID_PCM_Source::~SID_PCM_Source()
{
	//ShowConsoleMsg("SID dtor\n");
}

void SID_PCM_Source::timerCallback()
{
	stopTimer();
	renderSID();
}


PCM_source * SID_PCM_Source::Duplicate()
{
	//ShowConsoleMsg("SID duplicate\n");
	SID_PCM_Source* dupl = new SID_PCM_Source;
	dupl->m_sidfn = m_sidfn;
	dupl->m_sidlen = m_sidlen;
	dupl->m_sid_track = m_sid_track;
	dupl->renderSID();
	return dupl;
}

bool SID_PCM_Source::IsAvailable()
{
	return m_playsource != nullptr;
}

const char * SID_PCM_Source::GetType()
{
	return "SIDSOURCE";
}

bool SID_PCM_Source::SetFileName(const char * newfn)
{
	stopTimer();
	String temp = String(CharPointer_UTF8(newfn));
	if (temp != m_sidfn && temp.endsWithIgnoreCase("sid"))
	{
		m_sidfn = String(CharPointer_UTF8(newfn));
		m_displaysidname = m_sidfn;
		startTimer(500);
		return true;
	}
	return false;
}

const char * SID_PCM_Source::GetFileName()
{
	return m_displaysidname.toRawUTF8();
}

int SID_PCM_Source::GetNumChannels()
{
	return 1;
}

double SID_PCM_Source::GetSampleRate()
{
	return 44100.0;
}

double SID_PCM_Source::GetLength()
{
	if (m_playsource == nullptr)
		return m_sidlen;
	return m_playsource->GetLength();
}

int SID_PCM_Source::PropertiesWindow(HWND hwndParent)
{
	return 0;
}

void SID_PCM_Source::GetSamples(PCM_source_transfer_t * block)
{
	if (m_playsource == nullptr)
		return;
	m_playsource->GetSamples(block);
}

void SID_PCM_Source::GetPeakInfo(PCM_source_peaktransfer_t * block)
{
	if (m_playsource == nullptr)
		return;
	m_playsource->GetPeakInfo(block);
}

void SID_PCM_Source::SaveState(ProjectStateContext * ctx)
{
	ctx->AddLine("FILE \"%s\" %f %d", m_sidfn.toRawUTF8(),m_sidlen,m_sid_track);
}

int SID_PCM_Source::LoadState(const char * firstline, ProjectStateContext * ctx)
{
	LineParser lp;
	char buf[2048];
	for (;;)
	//while (ctx->GetLine(buf, 2048) != -1)
	{
		if (ctx->GetLine(buf, sizeof(buf))) 
			break;
		lp.parse(buf);
		if (strcmp(lp.gettoken_str(0), "FILE") == 0)
		{
			m_sidfn = String(CharPointer_UTF8(lp.gettoken_str(1)));
			m_sidlen = lp.gettoken_float(2);
			m_sid_track = lp.gettoken_int(3);
		}
		if (lp.gettoken_str(0)[0] == '>')
		{
			renderSID();
			return 0;
		}
	}
	return -1;
}

void SID_PCM_Source::Peaks_Clear(bool deleteFile)
{
	if (m_playsource == nullptr)
		return;
	m_playsource->Peaks_Clear(deleteFile);
}

int SID_PCM_Source::PeaksBuild_Begin()
{
	if (m_playsource == nullptr)
		return 0;
	return m_playsource->PeaksBuild_Begin();
}

int SID_PCM_Source::PeaksBuild_Run()
{
	if (m_playsource == nullptr)
		return 0;
	return m_playsource->PeaksBuild_Run();
}

void SID_PCM_Source::PeaksBuild_Finish()
{
	if (m_playsource == nullptr)
		return;
	m_playsource->PeaksBuild_Finish();
}

void SID_PCM_Source::renderSID()
{
	if (m_sidfn.isEmpty() == true)
		return;
	MemoryBlock mb;
	mb.append(m_sidfn.toRawUTF8(), m_sidfn.getNumBytesAsUTF8());
	double outlen = m_sidlen;
	mb.append(&outlen, sizeof(double));
	int numoutchans = 1;
	mb.append(&numoutchans, sizeof(int));
	mb.append(&m_sid_track, sizeof(int));
	juce::MD5 hash(mb);
	char buf[2048];
	GetProjectPath(buf, 2048);
	String outfolder = String(CharPointer_UTF8(buf)) + "/sid_cache";
	File diskfolder(outfolder);
	if (diskfolder.exists() == false)
	{
		diskfolder.createDirectory();
	}
	String outfn = outfolder+"/" + hash.toHexString() + ".wav";
	File temp(outfn);
	if (temp.existsAsFile() == true)
	{
		PCM_source* src = PCM_Source_CreateFromFile(outfn.toRawUTF8());
		if (src != nullptr)
		{
			m_playsource = std::unique_ptr<PCM_source>(src);
			Main_OnCommand(40047, 0); // build any missing peaks
			//ShowConsoleMsg("Using cached SID render\n");
			return;
		}
	}
	//ShowConsoleMsg("Rendering sid...\n");
	//ShowConsoleMsg(m_sidfn.toRawUTF8());
	StringArray args;
	args.add("C:\\Portable_Apps\\SID_to_WAV_v1.8\\SID2WAV.EXE");
	args.add("-t" + String(outlen));
	args.add("-16");
	if (m_sid_track > 0)
		args.add("-o" + String(m_sid_track));
	args.add(m_sidfn);
	//ShowConsoleMsg("export outfile name ");
	//ShowConsoleMsg(outfn.toRawUTF8());
	//ShowConsoleMsg("\n");
	args.add(outfn);
	m_childprocess.start(args);
	m_childprocess.waitForProcessToFinish(60000);
	if (m_childprocess.getExitCode() == 0)
	{
		//ShowConsoleMsg("rendered ok!\n");
		PCM_source* src = PCM_Source_CreateFromFile(outfn.toRawUTF8());
		if (src != nullptr)
		{
			m_playsource = std::unique_ptr<PCM_source>(src);
			Main_OnCommand(40047, 0); // build any missing peaks
		}
		else ShowConsoleMsg("Could not create pcm_source\n");
	}
	else
	{
		String temp = m_childprocess.readAllProcessOutput();
		ShowConsoleMsg(temp.toRawUTF8());
	}
}
