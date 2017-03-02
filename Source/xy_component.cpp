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
					PopupMenu menu;
					String fxparname = String(buf1) + " : " + String(buf2);
					menu.addItem(1, "Assign " + fxparname + " to X axis");
					menu.addItem(2, "Assign " + fxparname + " to Y axis");
					menu.addItem(3, "Remove X assignment");
					menu.addItem(4, "Remove Y assignment");
					menu.addItem(5, "Clear path");
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
				}
			}
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
