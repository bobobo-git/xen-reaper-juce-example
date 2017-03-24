#pragma once

#include "JuceHeader.h"

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
		if (HasExtState("xenrubberband", "exelocation") == true)
		{
			const char* exefn = GetExtState("xenrubberband", "exelocation");
			m_rubberband_exe = String(CharPointer_UTF8(exefn));
		}
		else
		{
#ifdef WIN32
			FileChooser chooser("Choose RubberBand executable",File(),"*.exe");
#else
			FileChooser chooser("Choose RubberBand executable", File());
#endif
			if (chooser.browseForFileToOpen())
			{
				m_rubberband_exe = chooser.getResult().getFullPathName();
				SetExtState("xenrubberband", "exelocation", m_rubberband_exe.toRawUTF8(), true);
			}
		}
		addAndMakeVisible(&m_time_ratio_slider);
		addAndMakeVisible(&m_pitch_slider);
		addAndMakeVisible(&m_crispness_combo);
		addAndMakeVisible(&m_apply_button);
		m_time_ratio_slider.setRange(0.1, 10.0);
		m_time_ratio_slider.setValue(2.0);
		m_time_ratio_slider.addListener(this);
		m_pitch_slider.setRange(-24.0, 24.0);
		m_pitch_slider.setValue(-4.0);
		m_pitch_slider.addListener(this);
		for (int i = 0; i < 10; ++i)
			m_crispness_combo.addItem(String(i), i + 1);
		m_crispness_combo.setSelectedId(5);
		m_crispness_combo.addListener(this);
		m_apply_button.addListener(this);
		m_apply_button.setButtonText("Apply");
		setSize(350, 200);
	}
	void resized() override
	{
		m_time_ratio_slider.setBounds(1, 1, getWidth() - 2, 22);
		m_pitch_slider.setBounds(1, m_time_ratio_slider.getBottom() + 2, getWidth() - 2, 22);
		m_crispness_combo.setBounds(1, m_pitch_slider.getBottom() + 2, getWidth() - 2, 22);
		m_apply_button.setBounds(1, m_crispness_combo.getBottom() + 2, 100, 22);
	}
	void sliderValueChanged(Slider* slid) override
	{

	}
	void comboBoxChanged(ComboBox* combo) override
	{

	}
	void buttonClicked(Button* but) override
	{
		if (but == &m_apply_button)
		{
			processSelectedItem();
		}
	}
	void timerCallback(int id) override
	{
		if (id == 1)
		{
			if (m_processing == true)
			{
				if (m_child_process.isRunning() == false)
				{
					stopTimer(1);
					m_processing = false;
					m_apply_button.setEnabled(true);
					if (m_child_process.getExitCode() == 0)
					{
						InsertMedia(m_current_out_file.toRawUTF8(), 3);
						UpdateArrange();
					}
					else
						showBubbleMessage(m_child_process.readAllProcessOutput());
				}
			}
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
		MediaItem* item = GetSelectedMediaItem(nullptr, 0);
		MediaItem_Take* take = GetActiveTake(item);
		PCM_source* src = GetMediaItemTake_Source(take);
		if (src == nullptr)
		{
			showBubbleMessage("Could not get media item take source");
			return;
		}
		StringArray args;
		args.add(m_rubberband_exe);
		args.add("-c" + String(m_crispness_combo.getSelectedId() - 1));
		args.add("-t" + String(m_time_ratio_slider.getValue()));
		args.add("-p" + String(m_pitch_slider.getValue()));
		String infn = CharPointer_UTF8(src->GetFileName());
		char buf[2048];
		GetProjectPath(buf, 2048);
		if (strlen(buf) > 0)
		{
			juce::Uuid uid;
			String outfn = String(CharPointer_UTF8(buf)) + "/" + uid.toString() + ".wav";
			args.add(infn);
			args.add(outfn);
			if (m_child_process.start(args) == true)
			{
				// Child process now started but we won't wait for it to finish here because that would
				// block the GUI event loop from running, so start the timer with id 1 that
				// will check for the child process finish state
				m_processing = true;
				m_current_out_file = outfn;
				m_apply_button.setEnabled(false);
				startTimer(1, 100);
			}
			else showBubbleMessage("Could not start child process");
		}
		else showBubbleMessage("Could not get valid project path");
	}
	void showBubbleMessage(String txt)
	{
		auto bub = new BubbleMessageComponent;
		addChildComponent(bub);
		bub->showAt(&m_apply_button, AttributedString(txt), 5000, true, true);
	}
private:
	Slider m_time_ratio_slider;
	Slider m_pitch_slider;
	ComboBox m_crispness_combo;
	TextButton m_apply_button;
	ChildProcess m_child_process;
	String m_rubberband_exe;
	bool m_processing = false;
	String m_current_out_file;
};
