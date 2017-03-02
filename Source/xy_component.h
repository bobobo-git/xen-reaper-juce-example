#pragma once

#include "JuceHeader.h"

class PointWithTime
{
public:
	PointWithTime() {}
	PointWithTime(double tpos, double x, double y)
		: m_time(tpos), m_x(x), m_y(y) {}
	double m_time = 0.0;
	double m_x = 0.0;
	double m_y = 0.0;
};

class XYComponent : public Component, public Timer
{
public:
	XYComponent();
	void timerCallback() override;
	void paint(Graphics& g) override;
	void mouseDown(const MouseEvent& ev) override;
	void mouseDrag(const MouseEvent& ev) override;
	void updateFXParams(double x, double y);
	void mouseUp(const MouseEvent& ev) override;
private:
	double m_x_pos = 0.5;
	double m_y_pos = 0.5;
	int m_x_target_track = -1;
	int m_x_target_fx = -1;
	int m_x_target_par = -1;
	int m_y_target_track = -1;
	int m_y_target_fx = -1;
	int m_y_target_par = -1;
	Path m_path;
	double m_tpos = 0.0;
	bool m_path_finished = false;
};
