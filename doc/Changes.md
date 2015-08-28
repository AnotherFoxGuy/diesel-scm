Changes
================================================================================

Fuel V2.0.0 (2015-XX-XX)
--------------------------------------------------------------------------------
- Feature: New workspace view to visualise branches, tags, stashes and remotes
- Feature: Support for creating, merging and integrating branches
- Feature: Support for creating and deleting tags
- Feature: Support for updating the workspace to a particular revisions including
  specific branches or tags
- Feature: Support for creating a branch during commit, including private branches
- Feature: Added Search Box for filtering and searching within the list of files
- Feature: Display the active tags and branches in the status bar
- Feature: Fuel now automatically detects the http port of "fossil ui"
- Feature: Added menu shortcut to display only the modified files of the workspace
- Feature: Support for arbitrary number of remote urls, including pushing and
  pulling to specific remotes.
- Feature: Remote URL credentials are stored in the user keychain for systems that
  support one. Thanks to the QtKeychain project (https://github.com/frankosterfeld/qtkeychain)
- Feature: Folders now have different appereance depending on the modification
  status of their contents, including missing folders
- Feature: Support for multiple file user-actions
- Feature: Double-Click action also allows user actions
- Feature: Support for the versionable .fossil-settings/ignore-glob
- Feature: OSX: UI improvements
- Feature: OSX: Improved external application selection for application bundles
- Feature: Long operations can now be aborted via a button on the status bar
- Feature: Windows: Shift-Right-Click invokes the Explorer folder context menu on
  Workspace folders
- Misc: Reorganised menu structure.
- Misc: Separated Fuel and Fossil settings
- Bug Fix: Retain the folder tree state when refreshing the workspace
- Bug Fix: Fixed issue with the expanding width of the commit dialog
- Major internal refactoring

Fuel V1.0.1 (2015-08-23)
--------------------------------------------------------------------------------
- Added Localisations:
	- Italian (Thanks maxxlupi and Zangune)
	- Dutch (Thanks Rick Van Lieshout and Fly Man)
	- Korean (Thanks ardiefox)
- Reformated Docs into Markdown

Fuel V1.0.0 (2015-03-28)
--------------------------------------------------------------------------------
- Feature: Long Operations can now be aborted by pressing the Escape key
- Improvement: Better support for commit messages with international characters
- Improvement: Fossil queries about CR/NL inconsistencies are now handled better
- Improvement: Files in Conflicted state are now shown
- Localisations:
	- Russian (Thanks Mouse166)
	- Portuguese (Thanks emansije)

Fuel V0.9.7 (Unreleased)
--------------------------------------------------------------------------------
- Feature: Optionally use the internal browser for the Fossil UI
- Feature: Support for persisting the state (Column order and sizes) of the File View
- Feature: Dropping a Fossil checkout file or workspace folder on Fuel now opens that workspace
- Feature: Dropping a file on Fuel now adds that file to Fossil
- Feature: Commit Dialog: Pressing Ctrl-Enter within the comment box commences the commit,
  and Escape aborts it
- Feature: Support for localization
- Localisations:
	- French (Thanks Fringale)
	- German (Thanks stayawake)
	- Greek
	- Spanish (Thanks djnavas)
- Feature: Support for QT5
- Distribution: Fuel is now available in the Arch User Repository

Fuel V0.9.6 (2012-05-13)
--------------------------------------------------------------------------------
- Feature: Support for fossil stashes
- Feature: Support for dragging and dropping files out of Fuel
- Feature: Allow for opening workspaces via the checkout file or a workspace folder
- Feature: Display the actual file icons
- Feature: Windows: Shift-Right-Click invokes the Explorer file context menu
- Feature: Allow starting Fuel into an existing fossil workspace via the command line (Thanks Chris)
- Feature: Portable mode. When starting Fuel with the "--portable" option all settings
  will be stored in a Fuel.ini file. If a settings file already exists, Fuel will start
  into portable mode automatically. (Thanks Chris)
- Improvement: Always show unknown files when starting a new repository
- Bug Fix: Avoid specifying filenames explicitly when all modified files are selected.
  This addresses an issue preventing commits after a merge
- Bug Fix: Fixed issue where a complete repository would be committed even when
  the user has a specific set of files marked for commit
- Misc: Minor GUI bug fixes and usability enhancements

