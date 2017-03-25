Example of a Reaper extension plugin that uses the JUCE library.

In Reaper it adds the following actions :

**"JUCE test : Show/hide XY Control"** : Shows/hides a window with tabbed XY controls to control track FX parameters in Reaper. Serves as a generic example of how to use the Reaper API together with JUCE components.

**"JUCE test : Show/hide RubberBand"** : Shows/hides a window that is a GUI for the Rubberband time/pitch processing command line program. This serves as an example of how to do an operation in the background without locking up the GUI. TODO : Process only the portion of audio in the selected item in Reaper, not the whole audio file that the item references.

Executable binaries of Rubberband for Windows and macOS are included in this repo. Rubberband's own site : http://breakfastquay.com/rubberband/

These are not fully developed features suitable for end users. (At least at the moment.)