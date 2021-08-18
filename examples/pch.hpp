#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_SVG_Image.H>
#include <FL/Fl_Table_Row.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>

#include <string>
#include <thread>
#include <vector>

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
