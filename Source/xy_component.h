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

class ParameterChooserComponent : public Component, public TextEditor::Listener
{
public:
	ParameterChooserComponent();
	~ParameterChooserComponent();
	void resized() override;
	void textEditorTextChanged(TextEditor& ed) override;
	std::function<void(int, int, int, int)> OnParameterAssign;
private:
	TreeView m_tv;
	TextEditor m_filter_edit;
	void updateTree(String filter);
};

enum class XYMode
{
	Constant,
	Path
};

class FXAssignment
{
public:
	FXAssignment() {}
	FXAssignment(int tkid, int fxid, int parid) :
		m_track_id(tkid), m_fx(fxid), m_param(parid) {}
	int m_track_id = -1;
	int m_fx = -1;
	int m_param = -1;
	double m_param_skew = 1.0;
	//GUID* m_fx_GUID;
};

class XYComponent : public Component, public MultiTimer, public Slider::Listener
{
public:
	XYComponent();
	void timerCallback(int id) override;
	void paint(Graphics& g) override;
	void mouseDown(const MouseEvent& ev) override;
	void mouseDrag(const MouseEvent& ev) override;
	void updateFXParams(double x, double y);
	void mouseUp(const MouseEvent& ev) override;
	void setPathDuration(double len);
	void setTimeWarp(double w);
	void showOptionsMenu();
private:
	void sliderValueChanged(Slider* slid) override;
	double m_x_pos = 0.5;
	double m_y_pos = 0.5;
	FXAssignment m_x_assignment;
	FXAssignment m_y_assignment;
	Path m_path;
	double m_tpos = 0.0;
	bool m_path_finished = false;
	bool m_auto_close_path = true;
	double m_path_duration = 5000.0;
	double m_path_dur_at_drag_start = 5000.0;
	double m_timewarp = 0.0;
	XYMode m_xymode = XYMode::Path;
	Slider m_x_skew_slider;
	Slider m_y_skew_slider;
};

class XYComponentWithSliders : public Component, public Slider::Listener
{
public:
	XYComponentWithSliders();
	void sliderValueChanged(Slider* slid) override;
	void resized() override;
	void showOptionsMenu();
private:
	Slider m_slid_pathdur;
	Slider m_slid_timewarp;
	XYComponent m_xycomp;
};

class XYContainer : public Component, public Button::Listener
{
public:
	XYContainer();
	void resized() override;
	void buttonClicked(Button* but) override;
	void addTab();
	void removeCurrentTab();
private:
	TabbedComponent m_tabs;
	TextButton m_add_but;
	TextButton m_rem_but;
	TextButton m_options_but;
	void updateTabNames();
};