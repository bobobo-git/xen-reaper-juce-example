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

class ParameterChooserComponent;

class ParameterTreeItem : public TreeViewItem
{
public:
	ParameterTreeItem(ParameterChooserComponent* chooser, String txt, int trackid, int fxid, int paramid, bool isleaf) :
		m_txt(txt), m_isleaf(isleaf), m_chooser(chooser), m_track_index(trackid),
		m_fx_index(fxid), m_param_index(paramid)
	{}
	bool mightContainSubItems() override;
	void paintItem(Graphics& g, int w, int h) override;
	void itemClicked(const MouseEvent & e) override;
	void itemDoubleClicked(const MouseEvent& e) override;
	void itemSelectionChanged(bool isNowSelected) override;

	String getNodeType()
	{
		if (m_isleaf == true)
			return m_txt;
		return String();
	}
	ParameterChooserComponent* m_chooser = nullptr;
	int m_track_index = -1;
	int m_fx_index = -1;
	int m_param_index = -1;
private:
	String m_txt;
	bool m_isleaf = false;
	
};

class ParameterChooserComponent : public Component
{
public:
	ParameterChooserComponent();
	~ParameterChooserComponent();
	void resized() override;
	std::function<void(int, int, int, int)> OnParameterAssign;
private:
	TreeView m_tv;
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
	bool m_auto_close_path = true;
};
