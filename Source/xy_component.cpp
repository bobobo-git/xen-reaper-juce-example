#include "xy_component.h"
#include "reaper_plugin_functions.h"

XYComponent::XYComponent() :
	m_x_skew_slider(Slider::LinearHorizontal, Slider::TextBoxRight),
	m_y_skew_slider(Slider::LinearHorizontal, Slider::TextBoxRight)
{
	setSize(100, 100);
	startTimer(1,50);
	m_x_skew_slider.setRange(0.1, 4.0);
	m_y_skew_slider.setRange(0.1, 4.0);
	m_x_skew_slider.addListener(this);
	m_y_skew_slider.addListener(this);
}

void XYComponent::timerCallback(int id)
{
	if (id == 1)
	{
		if (m_xymode == XYMode::Constant)
			return;
		if (m_path.isEmpty() == false && m_path_finished == true)
		{
			double pdur = m_path_duration;
			double playpos = fmod(Time::getMillisecondCounterHiRes() - m_tpos, pdur);
			double pathposnorm = 1.0 / pdur*playpos;
			if (m_timewarp >= 0.0)
				pathposnorm = pow(pathposnorm, 1.0 + 4.0*m_timewarp);
			else
				pathposnorm = 1.0 - pow(1.0 - pathposnorm, 1.0 + 4.0*-m_timewarp);
			double pathpos = m_path.getLength() * pathposnorm;
			auto pt = m_path.getPointAlongPath(pathpos);
			updateFXParams(pt.x, 1.0 - pt.y);
			m_x_pos = pt.x;
			m_y_pos = pt.y;
			repaint();
		}
	}
	if (id == 2)
	{
		m_path_finished = true;
		if (m_auto_close_path == true)
			m_path.closeSubPath();
		repaint();
		m_tpos = Time::getMillisecondCounterHiRes();
		stopTimer(2);
	}
}

void XYComponent::paint(Graphics & g)
{
	g.fillAll(Colours::black);
	g.setColour(Colours::yellow);
	const float size = 20.0;
	g.fillEllipse(m_x_pos*getWidth() - size / 2, m_y_pos*getHeight() - size / 2, size, size);
	g.setColour(Colours::white);
	if (m_xymode == XYMode::Constant)
		return;
	if (m_path_finished == false)
		g.setColour(Colours::white);
	else g.setColour(Colours::green);
	juce::AffineTransform transform;
	transform = transform.scaled(getWidth(), getHeight());
	Path scaled = m_path;
	scaled.applyTransform(transform);
	g.strokePath(scaled, PathStrokeType(2.0f));
}

void XYComponent::mouseDown(const MouseEvent & ev)
{
	if (ev.mods.isRightButtonDown() == false && m_xymode==XYMode::Path)
	{
		if (isTimerRunning(2)==false)
			m_path.clear();
		stopTimer(2);
		m_path.startNewSubPath(1.0 / getWidth()*ev.x, 1.0 / getHeight()*ev.y);
		m_path_finished = false;
		repaint();
	}
	if (ev.mods.isRightButtonDown() == true)
	{
		showOptionsMenu();
	}
}

void XYComponent::mouseDrag(const MouseEvent & ev)
{
	m_x_pos = jlimit(0.0, 1.0, 1.0 / getWidth()*ev.x);
	m_y_pos = jlimit(0.0, 1.0, 1.0 / getHeight()*ev.y);
	if (m_xymode == XYMode::Path)
		m_path.lineTo(m_x_pos, m_y_pos);
	updateFXParams(m_x_pos, 1.0 - m_y_pos);
	repaint();
}

void XYComponent::updateFXParams(double x, double y)
{
	NormalisableRange<double> nrx(0.0, 1.0, 0.001, m_x_assignment.m_param_skew);
	x = nrx.convertFrom0to1(x);
	NormalisableRange<double> nry(0.0, 1.0, 0.001, m_y_assignment.m_param_skew);
	y = nry.convertFrom0to1(y);
	if (m_x_assignment.m_track_id >= 0)
	{
		MediaTrack* track = GetTrack(nullptr, m_x_assignment.m_track_id);
		if (track != nullptr)
			TrackFX_SetParamNormalized(track, m_x_assignment.m_fx, m_x_assignment.m_param, x);
	}
	if (m_y_assignment.m_track_id >= 0)
	{
		MediaTrack* track = GetTrack(nullptr, m_y_assignment.m_track_id);
		if (track != nullptr)
			TrackFX_SetParamNormalized(track, m_y_assignment.m_fx, m_y_assignment.m_param, y);
	}
}

void XYComponent::mouseUp(const MouseEvent & ev)
{
	if (ev.mods.isCommandDown() == true)
		return;
	if (m_xymode == XYMode::Constant)
		return;
	startTimer(2, 2000);
}

void XYComponent::setPathDuration(double len)
{
	m_path_duration = jlimit<double>(0.1,120000.0,len);
}

void XYComponent::setTimeWarp(double w)
{
	m_timewarp = jlimit<double>(-1.0, 1.0, w);
}

void XYComponent::showOptionsMenu()
{
	PopupMenu menu;
	menu.addItem(7, "Choose parameters...");
	menu.addItem(8, "Auto-close path", true, m_auto_close_path);
	PopupMenu modemenu;
	modemenu.addItem(100, "Constant", true, m_xymode == XYMode::Constant);
	modemenu.addItem(101, "Path", true, m_xymode == XYMode::Path);
	menu.addSubMenu("Mode", modemenu, true);
	menu.addSectionHeader("X axis parameter scaling");
	menu.addCustomItem(1000, &m_x_skew_slider,200,20,false);
	menu.addSectionHeader("Y axis parameter scaling");
	menu.addCustomItem(1000, &m_y_skew_slider, 200, 20, false);
	int tk = -1;
	int fx = -1;
	int par = -1;
	GetLastTouchedFX(&tk, &fx, &par);
	if (tk >= 1 && fx >= 0)
	{
		MediaTrack* track = GetTrack(nullptr, tk - 1);
		if (track != nullptr)
		{
			char buf1[2048];
			char buf2[2048];
			if (TrackFX_GetFXName(track, fx, buf1, 2048) == true &&
				TrackFX_GetParamName(track, fx, par, buf2, 2048) == true)
			{

				String fxparname = String(CharPointer_UTF8(buf1)) + " : " + String(CharPointer_UTF8(buf2));
				menu.addItem(1, "Assign " + fxparname + " to X axis");
				menu.addItem(2, "Assign " + fxparname + " to Y axis");
				menu.addItem(3, "Remove X assignment");
				menu.addItem(4, "Remove Y assignment");
				menu.addItem(5, "Clear path");
			}
		}
	}
	int r = menu.show();
	if (r == 1)
	{
		m_x_assignment = { tk - 1,fx,par };
	}
	if (r == 2)
	{
		m_y_assignment = { tk - 1, fx, par };
	}
	if (r == 3)
	{
		m_x_assignment = { -1,-1,-1 };
	}
	if (r == 4)
	{
		m_y_assignment = { -1,-1,-1 };
	}
	if (r == 5)
	{
		m_path.clear();
		m_path_finished = false;
		repaint();
	}
	if (r == 7)
	{
		ParameterChooserComponent* comp = new ParameterChooserComponent;
		comp->OnParameterAssign = [this](int which, int track, int fx, int param)
		{
			if (which == 0)
			{
				m_x_assignment = { track,fx,param };
			}
			if (which == 1)
			{
				m_y_assignment = { track,fx,param };
			}
		};
		comp->setSize(getWidth() - 40, getHeight() - 40);
		CallOutBox::launchAsynchronously(comp, { 0,0,10,10 }, this);
	}
	if (r == 8)
	{
		m_auto_close_path = !m_auto_close_path;
	}
	if (r == 100)
	{
		m_xymode = XYMode::Constant;
		m_path.clear();
	}
	if (r == 101)
		m_xymode = XYMode::Path;
	repaint();
}

void XYComponent::sliderValueChanged(Slider * slid)
{
	if (slid == &m_x_skew_slider)
		m_x_assignment.m_param_skew = slid->getValue();
	if (slid == &m_y_skew_slider)
		m_y_assignment.m_param_skew = slid->getValue();
}

bool ParameterTreeItem::mightContainSubItems()
{
	if (m_isleaf == true)
		return false;
	return true;
}

void ParameterTreeItem::paintItem(Graphics & g, int w, int h)
{
	if (isSelected())
		g.fillAll(Colours::lightblue);
	else
		g.fillAll(Colours::Colours::white);
	g.setColour(Colours::black);
	g.drawText(m_txt, 0, 0, w, h, Justification::left);
}

void ParameterTreeItem::itemClicked(const MouseEvent & e)
{
	if (e.mods.isRightButtonDown() == true && m_isleaf == true)
	{
		PopupMenu menu;
		menu.addItem(1, "Assign to X axis");
		menu.addItem(2, "Assign to Y axis");
		int r = menu.show();
		if (r > 0)
		{
			if (m_chooser->OnParameterAssign)
				m_chooser->OnParameterAssign(r-1, m_track_index, m_fx_index, m_param_index);
		}
	}
}

void ParameterTreeItem::itemDoubleClicked(const MouseEvent & e)
{
}

void ParameterTreeItem::itemSelectionChanged(bool isNowSelected)
{
}

ParameterChooserComponent::ParameterChooserComponent()
{
	addAndMakeVisible(&m_tv);
	addAndMakeVisible(&m_filter_edit);
	m_filter_edit.addListener(this);
	m_tv.setColour(TreeView::ColourIds::backgroundColourId, Colours::white);
	updateTree(String());
}

ParameterChooserComponent::~ParameterChooserComponent()
{
	m_tv.deleteRootItem();
}

void ParameterChooserComponent::resized()
{
	m_filter_edit.setBounds(1, 1, getWidth() - 2, 22);
	m_tv.setBounds(0, 24, getWidth(), getHeight()-25);
}

void ParameterChooserComponent::textEditorTextChanged(TextEditor & ed)
{
	if (&ed == &m_filter_edit)
		updateTree(m_filter_edit.getText());
}

bool containsAllTokens(const String& txt, const StringArray& filtertokens)
{
	int cnt = 0;
	for (int i = 0; i < filtertokens.size(); ++i)
	{
		if (txt.containsIgnoreCase(filtertokens[i]) == true)
			++cnt;
	}
	return cnt == filtertokens.size();
}

void ParameterChooserComponent::updateTree(String filter)
{
	StringArray filtertokens = StringArray::fromTokens(filter, " ");
	m_tv.deleteRootItem();
	ParameterTreeItem* rootitem = new ParameterTreeItem(this, "Root", -1, -1, -1, false);
	for (int i = 0; i < CountTracks(nullptr); ++i)
	{
		MediaTrack* track = GetTrack(nullptr, i);
		char buf[4096];
		GetSetMediaTrackInfo_String(track, "P_NAME", buf, false);
		String trackname = String(CharPointer_UTF8(buf));
		if (trackname.isEmpty())
			trackname = String(i + 1);
		ParameterTreeItem* trackitem = nullptr;  
		for (int j = 0; j < TrackFX_GetCount(track); ++j)
		{
			TrackFX_GetFXName(track, j, buf, 4096);
			String fxname = String(CharPointer_UTF8(buf));
			ParameterTreeItem* fxitem = nullptr;  
			for (int k = 0; k < TrackFX_GetNumParams(track, j); ++k)
			{
				TrackFX_GetParamName(track, j, k, buf, 4096);
				String parname = String(CharPointer_UTF8(buf));
				String pathtopar = trackname + fxname + parname;
				if (containsAllTokens(pathtopar, filtertokens) == true)
				{
					if (trackitem == nullptr)
					{
						trackitem = new ParameterTreeItem(this, trackname, i, -1, -1, false);
						rootitem->addSubItem(trackitem, -1);
						trackitem->setOpen(true);
					}
					if (fxitem == nullptr)
					{
						fxitem = new ParameterTreeItem(this, fxname, i, j, -1, false);
						trackitem->addSubItem(fxitem, -1);
						fxitem->setOpen(true);
					}
					ParameterTreeItem* paramitem = new ParameterTreeItem(this, parname, i, j, k, true);
					fxitem->addSubItem(paramitem, -1);
				}
			}
		}
	}
	m_tv.setRootItem(rootitem);
	m_tv.setRootItemVisible(false);
}

XYContainer::XYContainer() : m_tabs(TabbedButtonBar::TabsAtTop)
{
	addAndMakeVisible(&m_tabs);
	addAndMakeVisible(&m_add_but);
	addAndMakeVisible(&m_rem_but);
	addAndMakeVisible(&m_options_but);
	addTab();
	m_add_but.setButtonText("Add");
	m_rem_but.setButtonText("Remove");
	m_options_but.setButtonText("Options...");
	m_add_but.addListener(this);
	m_rem_but.addListener(this);
	m_options_but.addListener(this);
	m_add_but.setTooltip("Add new XY control tab");
	m_rem_but.setTooltip("Remove current XY control tab");
	setSize(100, 100);
}

void XYContainer::resized()
{
	m_tabs.setBounds(0, 0, getWidth(), getHeight()-25);
	m_add_but.setBounds(1, getHeight() - 23, 70, 22);
	m_rem_but.setBounds(m_add_but.getRight()+2, getHeight() - 23, 70, 22);
	m_options_but.setBounds(m_rem_but.getRight() + 2, getHeight() - 23, 100, 22);
}

void XYContainer::buttonClicked(Button * but)
{
	if (but == &m_add_but)
		addTab();
	if (but == &m_rem_but)
		removeCurrentTab();
	if (but == &m_options_but)
	{
		XYComponentWithSliders* comp = dynamic_cast<XYComponentWithSliders*>(m_tabs.getCurrentContentComponent());
		if (comp != nullptr)
		{
			comp->showOptionsMenu();
		}
	}
}

void XYContainer::addTab()
{
	XYComponentWithSliders* comp = new XYComponentWithSliders;
	m_tabs.addTab("temp", Colours::lightgrey, comp, true);
	updateTabNames();
	m_tabs.setCurrentTabIndex(m_tabs.getNumTabs() - 1);
}

void XYContainer::removeCurrentTab()
{
	if (m_tabs.getNumTabs() == 1)
		return;
	int cur = m_tabs.getCurrentTabIndex();
	m_tabs.removeTab(cur);
	updateTabNames();
	m_tabs.setCurrentTabIndex(jlimit<int>(0, m_tabs.getNumTabs() - 1, cur));
}

void XYContainer::updateTabNames()
{
	for (int i = 0; i < m_tabs.getNumTabs(); ++i)
	{
		m_tabs.setTabName(i, String(i + 1));
	}
	m_rem_but.setEnabled(m_tabs.getNumTabs() > 1);
}

XYComponentWithSliders::XYComponentWithSliders() :
	m_slid_pathdur(Slider::RotaryVerticalDrag, Slider::TextBoxBelow),
	m_slid_timewarp(Slider::RotaryVerticalDrag, Slider::TextBoxBelow)
{
	addAndMakeVisible(&m_slid_pathdur);
	m_slid_pathdur.setRange(0.1, 120.0, 0.05);
	m_slid_pathdur.setSkewFactor(0.3);
	m_slid_pathdur.setValue(5.0);
	m_slid_pathdur.addListener(this);
	m_slid_pathdur.setTooltip("Path duration (seconds)");
	addAndMakeVisible(m_slid_timewarp);
	m_slid_timewarp.setRange(-1.0, 1.0);
	m_slid_timewarp.setValue(0.0);
	m_slid_timewarp.addListener(this);
	m_slid_timewarp.setTooltip("Warp path playback (Negative decelerates/Positive accelerates");
	addAndMakeVisible(&m_xycomp);
}

void XYComponentWithSliders::sliderValueChanged(Slider * slid)
{
	if (slid == &m_slid_pathdur)
		m_xycomp.setPathDuration(slid->getValue()*1000.0);
	if (slid == &m_slid_timewarp)
		m_xycomp.setTimeWarp(slid->getValue());
}

void XYComponentWithSliders::resized()
{
	m_slid_pathdur.setBounds(1, 1, 50, 50);
	m_slid_timewarp.setBounds(52, 1, 50, 50);
	m_xycomp.setBounds(1, 55, getWidth() - 2, getHeight() - 55);
}

void XYComponentWithSliders::showOptionsMenu()
{
	m_xycomp.showOptionsMenu();
}
