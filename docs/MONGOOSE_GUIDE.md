- Monggose system components
  - Wizard is an online visual web UI editor
  - It allows the user to place visual elements
  - Create API endpoints to integrate those elements into an exsiting firmware. This process
    is refered to as "gluing" the web ui to the exising firmware.
  - Once the user likes how the UI works there is a button to generate C/C++ code
    for mongoose UI to a local folder.
    - I put it in ./lib/mongoose.
  - The mogoose folder contains 7 files that are not to be editied locally since they are regenerated
    each time the GUI is editied in the wizard and downloaded.
    - The files are:
    - mongoose_fs.c - a packed filessystem containing the web UI HTML, css, etc
    - mongoose_glue.c - some includes and the functions for the firmware uses to interact with
      the webUI.
    - mongoose_glue.h - header files for the above. usually included somewhere in the firmware.
    - mongoose_impl.c - the specific implements of the user created interface.
    - mongoose.c - the main monggose library
    - mongoose.h - header for the above. usually included in the firmware.
    - mongoose_wizard.json - a file that can be opened in the Wizard containing all the specs to
      render the UI for editing.
    - The is i file that the Wizard does not replace: mongoose_config.h.
    - This is for user customizations, defines, etc. to alter configurable mongoose settings.
- To glue the UI to the firmware the user creates a parallel data structure to match the ones in
  mongoose.h
- The standard mongoose getter / setter functions can be called. We don't use this method as it
  has some limitations since the file is replaced when the UI is regernated, any user customizations
  of the functions are lost.
- Instead mongoose allows the user to register handler functions for the various web API endpoints.
- These user created functions are expected to do whatever processing the user wants with their local
  (firmware side)  mongoose data structures. The function then copies the data from the local (firmware side)
  data structure for the web UI to retrieve  and display.
- Same pattern in reverse to get data from the web UI into the firmware.

I see some of this is in the doc file I gave you?

I don't know if you can read the mongoose_wizard.json file and visualize the UI?
