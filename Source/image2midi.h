#pragma once

#include "JuceHeader.h"

class Image2MIDIGUI : public Component, public Slider::Listener, public Button::Listener
{
public:
	Image2MIDIGUI()
	{
		addAndMakeVisible(&m_import_button);
		m_import_button.setButtonText("Import image...");
		m_import_button.addListener(this);
		addAndMakeVisible(&m_brightness_th_slider);
		m_brightness_th_slider.addListener(this);
		m_brightness_th_slider.setRange(0.0, 0.99);
		m_brightness_th_slider.setValue(0.5);
		setSize(400, 400);
	}
	void sliderValueChanged(Slider* slid) override
	{
		
	}
	void sliderDragEnded(Slider* slid) override
	{
		MediaItem* item = GetSelectedMediaItem(nullptr, 0);
		MediaItem_Take* take = GetActiveTake(item);
		if (slid == &m_brightness_th_slider)
		{
			generateMIDI(take, m_source_image, m_brightness_th_slider.getValue());
		}
	}
	void buttonClicked(Button* but) override
	{
		if (but == &m_import_button)
		{
			FileChooser myChooser("Please select image file...",
				File::getSpecialLocation(File::userHomeDirectory),
				"*.png;*.jpg");
			if (myChooser.browseForFileToOpen())
			{
				File imgfile(myChooser.getResult());
				Image img = ImageFileFormat::loadFrom(imgfile);
				if (img.isValid())
				{
					m_source_image = img;
					MediaItem* item = GetSelectedMediaItem(nullptr, 0);
					MediaItem_Take* take = GetActiveTake(item);
					generateMIDI(take,m_source_image, m_brightness_th_slider.getValue());
				}
				else ShowConsoleMsg("Image file not valid\n");
			}
		}
	}
	void resized() override
	{
		m_import_button.setTopLeftPosition(1, 1);
		m_import_button.changeWidthToFitText(20);
		m_brightness_th_slider.setBounds(1, m_import_button.getBottom() + 2, getWidth() - 1, 20);
	}
	void generateMIDI(MediaItem_Take* take, Image& img, float brightnessth)	
	{
		if (take == nullptr || img.isValid() == false)
			return;
		int cnt1, cnt2, cnt3 = 0;
		MIDI_CountEvts(take, &cnt1, &cnt2, &cnt3);
		if (cnt1 > 0)
		{
			for (int i = cnt1; i >= 0; --i)
			{
				MIDI_DeleteNote(take, i);
			}
		}
		int w = img.getWidth();
		int h = img.getHeight();
		int gridsize = 16;
		int notecount = 0;
		bool stopadding = false;
		bool nosort = true;
		for (int x = 0; x < w / gridsize; ++x)
		{
			double notepos = 4000.0 / w*(x*gridsize);
			for (int y = 0; y < h / gridsize; ++y)
			{
				auto pix = img.getPixelAt(x*gridsize, y*gridsize);
				if (pix.getBrightness() > brightnessth)
				{
					double pitch = 127.0-(127.0 / h*(y*gridsize));
					++notecount;
					if (notecount > 5000)
						stopadding = true;
					MIDI_InsertNote(take, false, false, notepos, notepos + 50.0, 1, pitch, 127, &nosort);
				}
			}
			if (stopadding == true)
				break;
		}
		MIDI_Sort(take);
		UpdateArrange();
		char buf[100];
		sprintf(buf, "Added %d notes\n", notecount);
		ShowConsoleMsg(buf);
	}
private:
	TextButton m_import_button;
	Image m_source_image;
	Slider m_brightness_th_slider;
};
