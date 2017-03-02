#include "xy_component.h"
#include "reaper_plugin_functions.h"

XYComponent::XYComponent()
{
	setSize(100, 100);
	startTimer(50);
}

void XYComponent::timerCallback()
{
	if (m_path.isEmpty() == false && m_path_finished == true)
	{
		double playpos = fmod(Time::getMillisecondCounterHiRes() - m_tpos, 5000.0);
		double pathpos = m_path.getLength() / 5000.0*playpos;
		auto pt = m_path.getPointAlongPath(pathpos);
		updateFXParams(pt.x, 1.0 - pt.y);
		m_x_pos = pt.x;
		m_y_pos = pt.y;
		repaint();
	}
}

void XYComponent::paint(Graphics & g)
{
	g.fillAll(Colours::black);
	g.setColour(Colours::yellow);
	const float size = 20.0;
	g.fillEllipse(m_x_pos*getWidth() - size / 2, m_y_pos*getHeight() - size / 2, size, size);
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
	if (ev.mods.isRightButtonDown() == false)
	{
		m_path.clear();
		m_path.startNewSubPath(1.0 / getWidth()*ev.x, 1.0 / getHeight()*ev.y);
		m_path_finished = false;
		repaint();
	}
	if (ev.mods.isRightButtonDown() == true)
	{
		PopupMenu menu;
		menu.addItem(7, "Choose parameters...");
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
					
					String fxparname = String(buf1) + " : " + String(buf2);
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
			m_x_target_track = tk - 1;
			m_x_target_fx = fx;
			m_x_target_par = par;
		}
		if (r == 2)
		{
			m_y_target_track = tk - 1;
			m_y_target_fx = fx;
			m_y_target_par = par;
		}
		if (r == 3)
		{
			m_x_target_track = -1;
		}
		if (r == 4)
		{
			m_y_target_track = -1;
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
					m_x_target_track = track;
					m_x_target_fx = fx;
					m_x_target_par = param;
				}
				if (which == 1)
				{
					m_y_target_track = track;
					m_y_target_fx = fx;
					m_y_target_par = param;
				}
			};
			comp->setSize(getWidth() - 40, getHeight() - 40);
			CallOutBox::launchAsynchronously(comp, { 0,0,10,10 }, this);
		}
	}
}

void XYComponent::mouseDrag(const MouseEvent & ev)
{
	m_x_pos = jlimit(0.0, 1.0, 1.0 / getWidth()*ev.x);
	m_y_pos = jlimit(0.0, 1.0, 1.0 / getHeight()*ev.y);
	m_path.lineTo(m_x_pos, m_y_pos);
	updateFXParams(m_x_pos, 1.0 - m_y_pos);
	repaint();
}

void XYComponent::updateFXParams(double x, double y)
{
	if (m_x_target_track >= 0)
	{
		MediaTrack* track = GetTrack(nullptr, m_x_target_track);
		if (track != nullptr)
			TrackFX_SetParamNormalized(track, m_x_target_fx, m_x_target_par, x);
	}
	if (m_y_target_track >= 0)
	{
		MediaTrack* track = GetTrack(nullptr, m_y_target_track);
		if (track != nullptr)
			TrackFX_SetParamNormalized(track, m_y_target_fx, m_y_target_par, y);
	}
}

void XYComponent::mouseUp(const MouseEvent & ev)
{
	m_path_finished = true;
	m_path.closeSubPath();
	repaint();
	m_tpos = Time::getMillisecondCounterHiRes();
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
	m_tv.setColour(TreeView::ColourIds::backgroundColourId, Colours::white);
	ParameterTreeItem* rootitem = new ParameterTreeItem(this, "Root", -1, -1, -1, false);
	for (int i = 0; i < CountTracks(nullptr); ++i)
	{
		MediaTrack* track = GetTrack(nullptr, i);
		char buf[4096];
		GetSetMediaTrackInfo_String(track, "P_NAME", buf, false);
		String trackname(buf);
		if (trackname.isEmpty())
			trackname = String(i + 1);
		ParameterTreeItem* trackitem = new ParameterTreeItem(this, trackname, i, -1, -1, false);
		rootitem->addSubItem(trackitem, -1);
		trackitem->setOpen(true);
		for (int j = 0; j < TrackFX_GetCount(track); ++j)
		{
			TrackFX_GetFXName(track, j, buf, 4096);
			ParameterTreeItem* fxitem = new ParameterTreeItem(this, String(buf), i, j, -1, false);
			trackitem->addSubItem(fxitem, -1);
			fxitem->setOpen(true);
			for (int k = 0; k < TrackFX_GetNumParams(track, j); ++k)
			{
				TrackFX_GetParamName(track, j, k, buf, 4096);
				ParameterTreeItem* paramitem = new ParameterTreeItem(this, String(buf), i, j, k, true);
				fxitem->addSubItem(paramitem, -1);
			}
		}
	}
	m_tv.setRootItem(rootitem);
	m_tv.setRootItemVisible(false);
	
}

ParameterChooserComponent::~ParameterChooserComponent()
{
	m_tv.deleteRootItem();
}

void ParameterChooserComponent::resized()
{
	m_tv.setBounds(0, 0, getWidth(), getHeight());
}
