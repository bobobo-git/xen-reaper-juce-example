#define REAPERAPI_IMPLEMENT

#include "reaper_plugin_functions.h"
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include "JuceHeader.h"
#include "xy_component.h"
#include "image2midi.h"

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
	int m_val = 0;
	int m_valhw = 0;
	int m_relmode = 0;
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

std::shared_ptr<action_entry> add_action(std::string name, std::string id, toggle_state togst, 
	std::function<void(action_entry&)> f)
{
	auto entry = std::make_shared<action_entry>(name, id, togst, f);
	g_actions.push_back(entry);
	return entry;
}

// Reaper calls back to this when it wants to know an actions's toggle state
int toggleActionCallback(int command_id)
{
	for (auto& e : g_actions)
	{
		if (command_id != 0 && e->m_togglestate != CannotToggle && e->m_command_id == command_id)
		{
			if (e->m_togglestate == ToggleOff)
				return 0;
			if (e->m_togglestate == ToggleOn)
				return 1;
		}
	}
	// -1 if action not provided by this extension or is not togglable
	return -1;
}

bool g_juce_messagemanager_inited = false;

class Window : public ResizableWindow
{
public:
	static void initMessageManager()
	{
		if (g_juce_messagemanager_inited == false)
		{
			initialiseJuce_GUI();
			g_juce_messagemanager_inited = true;
		}
	}
	Window(String title, Component* content, int w, int h, bool resizable, Colour bgcolor)
		: ResizableWindow(title,bgcolor,false), m_content_component(content)
	{
		setContentOwned(m_content_component, true);
		setTopLeftPosition(10, 60);
		setSize(w, h);
		setResizable(resizable, false);
		setOpaque(true);
	}
	~Window() 
	{
	}
	int getDesktopWindowStyleFlags() const override
	{
		if (isResizable() == true)
			return ComponentPeer::windowHasCloseButton | ComponentPeer::windowHasTitleBar | ComponentPeer::windowIsResizable | ComponentPeer::windowHasMinimiseButton;
		return ComponentPeer::windowHasCloseButton | ComponentPeer::windowHasTitleBar | ComponentPeer::windowHasMinimiseButton;
	}
	void userTriedToCloseWindow() override
	{
		if (m_assoc_action != nullptr)
		{
			m_assoc_action->m_togglestate = ToggleOff;
			RefreshToolbar(m_assoc_action->m_command_id);
		}
		setVisible(false);
#ifdef WIN32
		BringWindowToTop(GetMainHwnd()); 
#endif
	}
	void visibilityChanged() override
	{
		if (m_assoc_action != nullptr && m_assoc_action->m_togglestate!=CannotToggle)
		{
			if (isVisible() == true)
				m_assoc_action->m_togglestate = ToggleOn;
			else m_assoc_action->m_togglestate = ToggleOff;
		}
		ResizableWindow::visibilityChanged();
	}
	action_entry* m_assoc_action = nullptr;
	template<typename T> 
	T* getComponentAs()
	{
		static_assert(std::is_base_of<Component, T>::value, "Can only get as Component derived classes");
		return dynamic_cast<T*>(m_content_component);
	}
private:
	Component* m_content_component = nullptr;
	TooltipWindow m_tooltipw;
};

class CSurfLoggerWindow;

std::unique_ptr<Window> g_xy_wnd;
std::unique_ptr<Window> g_rubberband_wnd;
std::unique_ptr<Window> g_image2midi_wnd;
std::unique_ptr<Window> g_csurflogger_wnd;

std::unique_ptr<Window> makeWindow(String name, Component* component, int w, int h, bool resizable, Colour backGroundColor)
{
	Window::initMessageManager();
	auto win = std::make_unique<Window>(name, component, w, h, resizable, backGroundColor);
	// This call order is important, the window should not be set visible
	// before adding it into the Reaper window hierarchy
	// Currently this only works for Windows, OS-X needs some really annoying special handling
	// not implemented yet, so just make the window be always on top
#ifdef WIN32
	win->addToDesktop(win->getDesktopWindowStyleFlags(), GetMainHwnd());
#else
	win->addToDesktop(win->getDesktopWindowStyleFlags(), 0);
	win->setAlwaysOnTop(true);
#endif
	return win;
}

class UserInputsDialog : public Component, public Button::Listener
{
public:
	struct field_t
	{
		field_t(String lbl, String initial)
		{
			m_label.setText(lbl, dontSendNotification);
			m_line_edit.setText(initial, dontSendNotification);
			m_line_edit.setColour(TextEditor::textColourId, Colours::black);
			m_line_edit.applyColourToAllText(Colours::black, true);
		}
		Label m_label;
		TextEditor m_line_edit;
	};
	UserInputsDialog()
	{
		addAndMakeVisible(&m_ok_button);
		m_ok_button.setButtonText("OK");
		m_ok_button.addListener(this);
		addAndMakeVisible(&m_cancel_button);
		m_cancel_button.setButtonText("Cancel");
		m_cancel_button.addListener(this);
	}
	void buttonClicked(Button* but) override
	{
		DialogWindow* dw = findParentComponentOfClass<DialogWindow>();
		if (dw == nullptr)
			return;
		if (but == &m_ok_button)
		{
			dw->exitModalState(1);
		}
		if (but == &m_cancel_button)
		{
			dw->exitModalState(2);
		}
	}

	void addEntry(String lbl, String initial)
	{
		auto entry = std::make_shared<field_t>(lbl, initial);
		addAndMakeVisible(&entry->m_label);
		addAndMakeVisible(&entry->m_line_edit);
		m_entries.push_back(entry);
	}
	void resized() override
	{
		int entryh = 25;
		int labelw = 150;
		for (int i=0;i<m_entries.size();++i)
		{
			m_entries[i]->m_label.setBounds(1, i*entryh, labelw, entryh - 1);
			m_entries[i]->m_line_edit.setBounds(1+labelw+2, i*entryh, 150, entryh - 1);
		}
		m_ok_button.setBounds(1, getHeight() - 25, 60, 24);
		m_cancel_button.setBounds(m_ok_button.getRight() + 1, m_ok_button.getY(), 60, 24);
	}
	StringArray getResults()
	{
		StringArray results;
		for (auto& e : m_entries)
			results.add(e->m_line_edit.getText());
		return results;
	}
private:
	std::vector<std::shared_ptr<field_t>> m_entries;
	TextButton m_ok_button;
	TextButton m_cancel_button;
};

StringArray GetUserInputsEx(String windowtitle, StringArray labels, StringArray initialentries)
{
	LookAndFeel_V3 lookandfeel;
	auto dlg = std::make_unique<UserInputsDialog>();
	dlg->setLookAndFeel(&lookandfeel);
	for (int i = 0; i < labels.size(); ++i)
	{
		if (i < initialentries.size())
		{
			dlg->addEntry(labels[i], initialentries[i]);
		}
	}
	dlg->setSize(320, labels.size() * 25 + 30);
	int r = DialogWindow::showModalDialog(windowtitle, dlg.get(), nullptr, Colours::lightgrey, true);
	if (r == 1)
		return dlg->getResults();
	return StringArray();
}

void testUserInputs()
{
	auto r = GetUserInputsEx("test", { "field 1","field 2" }, { "0.0","0.1" });
	for (auto& e : r)
	{
		ShowConsoleMsg(e.toRawUTF8());
		ShowConsoleMsg("\n");
	}
}

void toggleXYWindow(action_entry& ae)
{
	if (g_xy_wnd == nullptr)
	{
		g_xy_wnd = makeWindow("XY Control", new XYContainer, 500, 520, true, Colours::darkgrey);
		g_xy_wnd->m_assoc_action = &ae;
	}
	g_xy_wnd->setVisible(!g_xy_wnd->isVisible());
}

void toggleImage2MIDIWindow(action_entry& ae)
{
	if (g_image2midi_wnd == nullptr)
	{
		g_image2midi_wnd = makeWindow("Image2MIDI", new Image2MIDIGUI, 400, 400, true, Colours::lightgrey);
		g_image2midi_wnd->m_assoc_action = &ae;
	}
	g_image2midi_wnd->setVisible(!g_image2midi_wnd->isVisible());
}



class CSurfLoggerComponent : public Component
{
public:
	CSurfLoggerComponent()
	{
		addAndMakeVisible(&m_ed);
		m_ed.setMultiLine(true);
		m_ed.setReadOnly(true);
	}
	void resized() override
	{
		m_ed.setBounds(0, 0, getWidth(), getHeight());
	}
	void addMessage(String txt)
	{
		m_ed.insertTextAtCaret(txt);
		m_ed.setCaretPosition(m_ed.getText().length());
	}
private:
	TextEditor m_ed;
};

class MySurface : public IReaperControlSurface
{
public:

	void SetSurfaceSelected(MediaTrack *trackid, bool selected) override
	{
		if (g_csurflogger_wnd == nullptr)
			return;
		auto comp = g_csurflogger_wnd->getComponentAs<CSurfLoggerComponent>();
		if (selected)
			comp->addMessage("track " + String((int64_t)trackid) + " selected\n");
		else comp->addMessage("track " + String((int64_t)trackid) + " unselected\n");
	}
	// Inherited via IReaperControlSurface
	virtual const char * GetTypeString() override
	{
		return "mysurf";
	}

	virtual const char * GetDescString() override
	{
		return "My Surface";
	}

	virtual const char * GetConfigString() override
	{
		return "";
	}

};

void toggleCSurfLoggerWindow(action_entry& ae)
{
	if (g_csurflogger_wnd == nullptr)
	{
		g_csurflogger_wnd = makeWindow("CSurf Logger", new CSurfLoggerComponent, 400, 400, true, Colours::lightgrey);
		g_csurflogger_wnd->m_assoc_action = &ae;
	}
	g_csurflogger_wnd->setVisible(!g_csurflogger_wnd->isVisible());
}

void onActionWithValue(action_entry& ae)
{
	char buf[256];
	sprintf(buf, "%d %d %d\n", ae.m_val, ae.m_valhw, ae.m_relmode);
	ShowConsoleMsg(buf);
}

bool on_value_action(KbdSectionInfo *sec, int command, int val, int valhw, int relmode, HWND hwnd)
{
	for (auto& e : g_actions)
	{
		if (e->m_command_id != 0 && e->m_command_id == command) {
			Window::initMessageManager();
			e->m_val = val;
			e->m_valhw = valhw;
			e->m_relmode = relmode;
			e->m_func(*e);
			return true;
		}
	}
	return false; // failed to run relevant action
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

			add_action("JUCE test : Show/hide XY Control", "JUCETEST_SHOW_XYCONTROL", ToggleOff, [](action_entry& ae)
			{
				toggleXYWindow(ae);
			});
			
			add_action("JUCE test : Show/hide Image2MIDI", "JUCETEST_SHOW_IMAGE2MIDI", ToggleOff, [](action_entry& ae)
			{
				toggleImage2MIDIWindow(ae);
			});

			add_action("JUCE test : Show/hide CSurf Logger", "JUCETEST_SHOW_CSURFLOGGER", ToggleOff, [](action_entry& ae)
			{
				toggleCSurfLoggerWindow(ae);
			});

			add_action("JUCE test : Test user inputs", "JUCETEST_USERINPUTSEX", ToggleOff, [](action_entry& ae)
			{
				testUserInputs();
			});

			add_action("JUCE test : MIDI/OSC action test", "JUCETEST_MIDIOSCTEST", CannotToggle, onActionWithValue);

			rec->Register("hookcommand2", (void*)on_value_action);
			rec->Register("toggleaction", (void*)toggleActionCallback);
			MySurface* surf = new MySurface;
			rec->Register("csurf_inst", surf);
			return 1; // our plugin registered, return success
		}
		else
		{
			if (g_juce_messagemanager_inited == true)
			{
				g_xy_wnd = nullptr;
				g_rubberband_wnd = nullptr;
				shutdownJuce_GUI();
				g_juce_messagemanager_inited = false;
			}
			return 0;
		}
	}
};
