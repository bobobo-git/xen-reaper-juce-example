#pragma once

#include "JuceHeader.h"
#include <atomic>
#include <thread>

String getRubberBandExeLocation()
{
	if (HasExtState("xenrubberband", "exelocation") == true)
	{
		return String(CharPointer_UTF8(GetExtState("xenrubberband", "exelocation")));
	}
	else
	{
#ifdef WIN32
		FileChooser chooser("Choose RubberBand executable", File(), "*.exe");
#else
		FileChooser chooser("Choose RubberBand executable", File());
#endif
		if (chooser.browseForFileToOpen())
		{
			String temp = chooser.getResult().getFullPathName();
			SetExtState("xenrubberband", "exelocation", temp.toRawUTF8(), true);
			return temp;
		}
	}
	return String();
}

inline std::unique_ptr<PCM_sink> createPCMSink(const String& outfilename, const String& format, char bitdepth, int chans, int samplerate)
{
	if (format == "WAV")
	{
		char cfg[] = { 'e','v','a','w', bitdepth, 0 };
		return std::unique_ptr<PCM_sink>(PCM_Sink_Create(outfilename.toRawUTF8(),
			cfg, sizeof(cfg), chans, samplerate, true));
	}
	return nullptr;
}

inline String renderAudioAccessor(AudioAccessor* acc, String outfilename, int outchans, double outsr)
{
	double t0 = GetAudioAccessorStartTime(acc);
	double t1 = GetAudioAccessorEndTime(acc);
	double len = t1 - t0;
	int64_t lenframes = len*outsr;
	int bufsize = 32768;
	std::vector<double> accbuf(outchans*bufsize);
	std::vector<double> sinkbuf(outchans*bufsize);
	std::vector<double*> sinkbufptrs(outchans);
	for (int i = 0; i < outchans; ++i)
		sinkbufptrs[i] = &sinkbuf[i*bufsize];
	auto sink = createPCMSink(outfilename, "WAV", 32, outchans, outsr);
	if (sink != nullptr)
	{
		int64_t counter = 0;
		while (counter < lenframes)
		{
			int framestoread = std::min<int64_t>(bufsize, lenframes - counter);
			GetAudioAccessorSamples(acc, outsr, outchans, (double)counter / outsr, framestoread, accbuf.data());
			for (int i = 0; i < outchans; ++i)
			{
				for (int j = 0; j < framestoread; ++j)
				{
					sinkbufptrs[i][j] = accbuf[j*outchans + i];
				}
			}
			sink->WriteDoubles(sinkbufptrs.data(), framestoread, outchans, 0, 1);
			counter += bufsize;
		}
	}
	else return "Could not create sink";
	return String();
}

inline void deleteFileIfExists(String fn)
{
	if (fn.isEmpty() == true)
		return;
	File temp(fn);
	if (temp.existsAsFile() == true)
		temp.deleteFile();
}

inline String getTempFileNameAtProjectDirectory(String extension)
{
	char buf[4096];
	GetProjectPath(buf, 4096);
	return String(CharPointer_UTF8(buf)) + "/" + Uuid().toString() + extension;
}

inline String outFileNameFromItem(MediaItem* item)
{
	MediaItem_Take* take = GetActiveTake(item);
	const char* tk_name = GetTakeName(take);
	String temp;
	if (strlen(tk_name) > 0)
		temp = String(CharPointer_UTF8(tk_name));
	if (temp.endsWithIgnoreCase(".wav"))
		temp = temp.dropLastCharacters(4);
	char ppbuf[4096];
	GetProjectPath(ppbuf, 4096);
	String projectpath = String(CharPointer_UTF8(ppbuf));
	temp = projectpath + "/" + temp + "_rubberband.wav";
	File tempfile(temp);
	tempfile = tempfile.getNonexistentSibling();
	return tempfile.getFullPathName();
}

struct RubberBandParams
{
	double m_time_ratio = 1.0;
	double m_pitch = 0.0;
	int m_crispness = 5;
	String m_outfn;
	String m_rb_exe;
};

bool processItemWithRubberBandAsync(MediaItem* srcitem, 
	RubberBandParams params, 
	std::function<void(String)> completionHandler)
{
	auto task = [srcitem,params,completionHandler]()
	{
		MediaItem* item = srcitem;
		MediaItem_Take* take = GetActiveTake(item);
		PCM_source* src = GetMediaItemTake_Source(take);
		if (src == nullptr)
		{
			completionHandler("Could not get media item take source");
			return;
		}
		StringArray args;
		args.add(params.m_rb_exe);
		args.add("-c" + String(params.m_crispness));
		args.add("-t" + String(params.m_time_ratio));
		args.add("-p" + String(params.m_pitch));
		String infn = CharPointer_UTF8(src->GetFileName());
		String outfn = params.m_outfn;
		auto acc = std::shared_ptr<AudioAccessor>(CreateTakeAudioAccessor(take), [](AudioAccessor* aa)
		{ DestroyAudioAccessor(aa); });
		if (acc != nullptr)
		{
			String tempfn = getTempFileNameAtProjectDirectory(".wav");
			String err = renderAudioAccessor(acc.get(), tempfn, src->GetNumChannels(), src->GetSampleRate());
			if (err.isEmpty() == true)
			{
				args.add(tempfn);
				args.add(outfn);
				ChildProcess childproc;
				if (childproc.start(args) == true)
				{
					childproc.waitForProcessToFinish(60000);
					deleteFileIfExists(tempfn);
					if (childproc.getExitCode() == 0)
					{
						completionHandler(String());
					}
					else
						completionHandler(childproc.readAllProcessOutput());
				}
				else
				{
					deleteFileIfExists(tempfn);
					completionHandler("Could not start child process");
				}
			}
			else completionHandler(err);
		}
		else completionHandler("Could not create AudioAccessor");
		
		
	};
	std::thread th(task);
	th.detach();
	return true;
}

class RubberBandGUI : public Component, 
	public MultiTimer, 
	public Slider::Listener,
	public ComboBox::Listener,
	public Button::Listener
{
public:
	RubberBandGUI() :
		m_time_ratio_slider(Slider::LinearHorizontal,Slider::TextBoxRight),
		m_pitch_slider(Slider::LinearHorizontal, Slider::TextBoxRight)
	{
		m_rubberband_exe = getRubberBandExeLocation();
		addAndMakeVisible(&m_time_ratio_slider);
		addAndMakeVisible(&m_pitch_slider);
		addAndMakeVisible(&m_crispness_combo);
		addAndMakeVisible(&m_apply_button);
		m_time_ratio_slider.setRange(0.1, 10.0, 0.001);
		m_time_ratio_slider.setSkewFactorFromMidPoint(1.0);
		m_time_ratio_slider.setValue(2.0);
		m_time_ratio_slider.addListener(this);
		m_time_ratio_slider.setTooltip("Time ratio");
		m_pitch_slider.setRange(-24.0, 24.0, 0.1);
		m_pitch_slider.setValue(-4.0);
		m_pitch_slider.addListener(this);
		m_pitch_slider.setTooltip("Pitch (semitones)");
		
        m_crispness_combo.addItem("Mushy", 1);
        m_crispness_combo.addItem("Piano", 2);
        m_crispness_combo.addItem("Smooth", 3);
        m_crispness_combo.addItem("Balanced multitimbral mixture",4);
        m_crispness_combo.addItem("Unpitched percussion with stable notes",5);
        m_crispness_combo.addItem("Crisp monophonic instrumental",6);
        m_crispness_combo.addItem("Unpitched solo percussion",7);
        
        m_crispness_combo.setSelectedId(4);
		m_crispness_combo.addListener(this);
		m_crispness_combo.setTooltip("Algorithm mode");

		m_apply_button.addListener(this);
		m_apply_button.setButtonText("Apply");
		addChildComponent(&m_elapsed_label);
		setSize(350, 200);
		startTimer(1, 100);
	}
	void resized() override
	{
		m_time_ratio_slider.setBounds(1, 1, getWidth() - 2, 22);
		m_pitch_slider.setBounds(1, m_time_ratio_slider.getBottom() + 2, getWidth() - 2, 22);
		m_crispness_combo.setBounds(1, m_pitch_slider.getBottom() + 2, getWidth() - 2, 22);
		m_apply_button.setBounds(1, m_crispness_combo.getBottom() + 2, 100, 22);
		m_elapsed_label.setBounds(m_apply_button.getRight() + 2, m_apply_button.getY(), 200, 22);
	}
	void sliderValueChanged(Slider* slid) override {}
	void comboBoxChanged(ComboBox* combo) override {}
	void buttonClicked(Button* but) override
	{
		if (but == &m_apply_button)
			processSelectedItem();
	}
	void timerCallback(int id) override
	{
		if (id == 1 && m_processing == true)
		{
			double elapsed = Time::getMillisecondCounterHiRes() - m_process_start_time;
			m_elapsed_label.setText(String(elapsed / 1000.0, 1) + " seconds elapsed",dontSendNotification);
		}
	}
	void processSelectedItem()
	{
		if (m_processing == true)
			return;
		if (CountSelectedMediaItems(nullptr) == 0)
		{
			showBubbleMessage("No media item selected");
			return;
		}
		MediaItem* sel_item = GetSelectedMediaItem(nullptr, 0);
		RubberBandParams params;
		params.m_time_ratio = m_time_ratio_slider.getValue();
		params.m_pitch = m_pitch_slider.getValue();
		params.m_crispness = m_crispness_combo.getSelectedId() - 1;
		params.m_rb_exe = m_rubberband_exe;
		params.m_outfn = outFileNameFromItem(sel_item);
		getLastUsedParameters() = params;
		auto completionHandler = [this,params](String err)
		{
			m_processing = false;
			if (err.isEmpty() == false)
				showBubbleMessage(err);
			MessageManager::callAsync([this, err, params]()
			{
				m_apply_button.setEnabled(true);
				m_elapsed_label.setVisible(false);
				if (err.isEmpty() == true)
				{
					InsertMedia(params.m_outfn.toRawUTF8(), 3);
					UpdateArrange();
				}
			});
			
		};
		m_process_start_time = Time::getMillisecondCounterHiRes();
		m_processing = true;
		processItemWithRubberBandAsync(sel_item, params, completionHandler);
		m_apply_button.setEnabled(false);
		m_elapsed_label.setVisible(true);
	}
	void showBubbleMessage(String txt, int ms=5000)
	{
		MessageManager::callAsync([this,txt,ms]()
		{
			if (txt.isEmpty() == true) return;
			auto bub = new BubbleMessageComponent;
			addChildComponent(bub);
			bub->showAt(&m_apply_button, AttributedString(txt), ms, true, true);
		});
	}
	static RubberBandParams& getLastUsedParameters()
	{
		static RubberBandParams s_params;
		return s_params;
	}
private:
	Slider m_time_ratio_slider;
	Slider m_pitch_slider;
	ComboBox m_crispness_combo;
	TextButton m_apply_button;
	Label m_elapsed_label;
	ChildProcess m_child_process;
	String m_rubberband_exe;
	std::atomic<bool> m_processing{ false };
	double m_process_start_time = 0.0;
	
};
