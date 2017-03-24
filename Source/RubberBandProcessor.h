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

	}
	void processSelectedItem()
	{
		if (m_child_process.isRunning() == true)
			return;
		StringArray args;
		args.add("C:/Portable_Apps/rubber bantti/rubberband.exe");
		args.add("-c" + String(m_crispness_combo.getSelectedId() - 1));
		args.add("-t" + String(m_time_ratio_slider.getValue()));
		args.add("-p" + String(m_pitch_slider.getValue()));
		m_child_process.start(args);
	}
private:
	Slider m_time_ratio_slider;
	Slider m_pitch_slider;
	ComboBox m_crispness_combo;
	TextButton m_apply_button;
	ChildProcess m_child_process;
};
