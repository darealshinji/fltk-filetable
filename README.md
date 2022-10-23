Customizable file selection widgets in a table format.


The following classes are available:

fltk::filetable_
-> base class, must be sub-classed to be usable

fltk::filetable_simple
-> this widget only distincts between regular files and Unix special files

fltk::filetable_extension
-> file selection widget where the file icons are set based on the file
extensions (MS Windows style); Unix special files are recognized too

fltk::filetable_magic
-> file selection widget where the file icons are set based on the magic
bytes (Linux style); Unix special files are recognized too; this widget
uses multithreading and libmagic (experimental)

fltk::dirtree
-> a directory tree based on the Fl_Tree class

xdg
-> helper class to read the XDG paths from the user-dirs.dirs config file

fltk::mountbutton
-> work in progress

fltk::fileselection
-> convenience template class that provides most features of a file selection
widget, such as a sidebar and an address bar



My motivation to write these was that the default FLTK file selection was
practically not customizable and didn't look and feel modern enough, while
the native dialog was using GTK, which didn't feel right to me since I wanted
to use FLTK and not GTK.
I've tried to find a belance between being easy to use, highly customizable,
feature rich, looking modern and at the same time keeping it as simple as
possible (I don't intend to write a fully featured file manager).


Known issues or limitations:

* fltk::dirtree only lists directories; you need to subclass or modify it if you
  want files to be listed too

* fltk::fileselection has some minor focus issues

* auto-width doesn't work correctly on the fltk::filetable_ subclasses on
  startup, see the code in fltk::fileselection for a workaround

* fltk::filetable_magic might crash, you must define `FLTK_EXPERIMENTAL` to use it

* only SVG icons are supported

* icons and MIME type assossiations aren't taken from the Desktop Environment,
  you need to configure them yourself

