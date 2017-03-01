#define REAPERAPI_IMPLEMENT

#include "reaper_plugin_functions.h"
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include "JuceHeader.h"

HINSTANCE g_hInst;
HWND g_parent;

reaper_plugin_info_t* g_plugin_info;

enum toggle_state { CannotToggle, ToggleOff, ToggleOn };

class action_entry
{ //class for registering actions
public:
	action_entry(std::string description, std::string idstring, toggle_state togst, std::function<void(action_entry&)> func);
	action_entry(const action_entry&) = delete; // prevent copying
	action_entry& operator=(const action_entry&) = delete; // prevent copying
	action_entry(action_entry&&) = delete; // prevent moving
	action_entry& operator=(action_entry&&) = delete; // prevent moving

	int m_command_id = 0;
	gaccel_register_t m_accel_reg;
	std::function<void(action_entry&)> m_func;
	std::string m_desc;
	std::string m_id_string;
	toggle_state m_togglestate = CannotToggle;

	void* m_data = nullptr;
	template<typename T>
	T* getDataAs() { return static_cast<T*>(m_data); }
};


action_entry::action_entry(std::string description, std::string idstring, toggle_state togst, std::function<void(action_entry&)> func) :
	m_desc(description), m_id_string(idstring), m_func(func), m_togglestate(togst)
{
	if (g_plugin_info != nullptr)
	{
		m_accel_reg.accel = { 0,0,0 };
		m_accel_reg.desc = m_desc.c_str();
		m_accel_reg.accel.cmd = m_command_id = g_plugin_info->Register("command_id", (void*)m_id_string.c_str());
		g_plugin_info->Register("gaccel", &m_accel_reg);
	}
}

std::vector<std::shared_ptr<action_entry>> g_actions;

std::shared_ptr<action_entry> add_action(std::string name, std::string id, toggle_state togst, std::function<void(action_entry&)> f)
{
	auto entry = std::make_shared<action_entry>(name, id, togst, f);
	g_actions.push_back(entry);
	return entry;
}

bool hookCommandProc(int command, int flag) {
	for (auto& e : g_actions) {
		if (e->m_command_id != 0 && e->m_command_id == command) {
			e->m_func(*e);
			return true;
		}
	}
	return false; // failed to run relevant action
}

bool g_juce_inited = false;

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
	XYComponent()
	{
		setSize(100, 100);
		startTimer(50);
	}
	void timerCallback() override
	{
		if (m_path.isEmpty() == false && m_path_finished == true)
		{
			double playpos = fmod(Time::getMillisecondCounterHiRes() - m_tpos, 5000.0);
			double pathpos = m_path.getLength() / 5000.0*playpos;
			auto pt = m_path.getPointAlongPath(pathpos);
			updateFXParams(pt.x, pt.y);
			m_x_pos = pt.x;
			m_y_pos = pt.y;
			repaint();
		}
	}
	void paint(Graphics& g) override
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
	void mouseDown(const MouseEvent& ev) override
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
				MediaTrack* track = GetTrack(nullptr, tk-1);
				if (track != nullptr)
				{
					char buf1[2048];
					char buf2[2048];
					if (TrackFX_GetFXName(track, fx, buf1, 2048) == true &&
						TrackFX_GetParamName(track,fx,par,buf2,2048) == true)
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
	void mouseDrag(const MouseEvent& ev) override
	{
		m_x_pos = jlimit(0.0,1.0, 1.0 / getWidth()*ev.x);
		m_y_pos = jlimit(0.0,1.0, 1.0 / getHeight()*ev.y);
		m_path.lineTo(m_x_pos, m_y_pos);
		updateFXParams(m_x_pos, m_y_pos);
		
		repaint();
	}
	void updateFXParams(double x, double y)
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
	void mouseUp(const MouseEvent& ev) override
	{
		m_path_finished = true;
		m_path.closeSubPath();
		repaint();
		m_tpos = Time::getMillisecondCounterHiRes();
	}
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

class Window : public ResizableWindow
{
public:
	static void initGUIifNeeded()
	{
		if (g_juce_inited == false)
		{
			initialiseJuce_GUI();
			g_juce_inited = true;
		}
	}
	Window(String title, int w, int h, bool resizable, Colour bgcolor)
		: ResizableWindow(title,bgcolor,false)
	{
		setContentNonOwned(&m_xy_component, true);
		setTopLeftPosition(10, 60);
		setSize(w, h);
		setResizable(resizable, false);
		setOpaque(true);
	}
	~Window() {}
	int getDesktopWindowStyleFlags() const override
	{
		if (isResizable() == true)
			return ComponentPeer::windowHasCloseButton | ComponentPeer::windowHasTitleBar | ComponentPeer::windowIsResizable | ComponentPeer::windowHasMinimiseButton;
		return ComponentPeer::windowHasCloseButton | ComponentPeer::windowHasTitleBar | ComponentPeer::windowHasMinimiseButton;
	}
	void userTriedToCloseWindow() override
	{
		setVisible(false);
#ifdef WIN32
		BringWindowToTop(GetMainHwnd());
#endif
	}
    
private:
	XYComponent m_xy_component;
};

std::unique_ptr<Window> g_xy_wnd;

void toggleBrowserWindow(action_entry&)
{
	Window::initGUIifNeeded();
	if (g_xy_wnd == nullptr)
	{
		g_xy_wnd = std::make_unique<Window>("XY Control", 500, 500, true, Colours::black);
		// This call order is important, the window should not be set visible
		// before adding it into the Reaper window hierarchy
		// Currently this only works for Windows, OS-X needs some really annoying special handling
		// not implemented yet
#ifdef WIN32
		g_xy_wnd->addToDesktop(g_xy_wnd->getDesktopWindowStyleFlags(), GetMainHwnd());
#else
		g_xy_wnd->addToDesktop(g_browser_wnd->getDesktopWindowStyleFlags(), 0);
		g_xy_wnd->setAlwaysOnTop(true);
#endif
	}
	g_xy_wnd->setVisible(!g_xy_wnd->isVisible());
}

extern "C"
{
	REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t *rec) {
		if (rec != nullptr) {
#ifdef WIN32
			Process::setCurrentModuleInstanceHandle(hInstance);
#endif
			if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc) return 0;
			g_hInst = hInstance;
			g_plugin_info = rec;
			g_parent = rec->hwnd_main;
			if (REAPERAPI_LoadAPI(rec->GetFunc) > 0) return 0;

			add_action("JUCE test : Show XY Control", "JUCETEST_SHOW_XYCONTROL", CannotToggle, [](action_entry& ae)
			{
				toggleBrowserWindow(ae);
			});
			rec->Register("hookcommand", (void*)hookCommandProc);
			return 1; // our plugin registered, return success
		}
		else
		{
			if (g_juce_inited == true)
			{
				g_xy_wnd = nullptr;
				shutdownJuce_GUI();
			}
			return 0;
		}
	}
};
