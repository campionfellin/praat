/* Editor.c
 *
 * Copyright (C) 1992-2005 Paul Boersma
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * pb 2002/03/07 GPL
 * pb 2002/03/22 Editor_setPublish2Callback
 * pb 2005/09/01 do not add a "(cannot) undo" button if there is no data to save
 */

#include "ScriptEditor.h"
#include "ButtonEditor.h"
#include "machine.h"
#include "EditorM.h"
#include "praat_script.h"

/********** class EditorCommand **********/

static void classEditorCommand_destroy (I) {
	iam (EditorCommand);
	Melder_free (my itemTitle);
	Melder_free (my script);
	forget (my dialog);
	inherited (EditorCommand) destroy (me);
}

class_methods (EditorCommand, Thing)
	class_method_local (EditorCommand, destroy)
class_methods_end

/********** class EditorMenu **********/

#define EditorMenu_members Thing_members \
	Any editor; \
	const char *menuTitle; \
	Widget menuWidget; \
	Ordered commands;
#define EditorMenu_methods Thing_methods
class_create_opaque (EditorMenu, Thing)

static void classEditorMenu_destroy (I) {
	iam (EditorMenu);
	Melder_free (my menuTitle);
	forget (my commands);
	inherited (EditorCommand) destroy (me);
}

class_methods (EditorMenu, Thing)
	class_method_local (EditorMenu, destroy)
class_methods_end

/********** functions **********/

MOTIF_CALLBACK (commonCallback)
	iam (EditorCommand);
	if (my editor && ((Editor) my editor) -> methods -> scriptable)
		UiHistory_write ("\n%s", my itemTitle);
	if (! my commandCallback (me, NULL)) Melder_flushError (NULL);
MOTIF_CALLBACK_END

Widget EditorMenu_addCommand (EditorMenu menu, const char *itemTitle, long flags,
	int (*commandCallback) (EditorCommand cmd, Any sender))
{
	EditorCommand me = new (EditorCommand);
	my editor = menu -> editor;
	my menu = menu;
	if (! (my itemTitle = Melder_strdup (itemTitle))) { forget (me); return NULL; }
	my itemWidget = commandCallback == NULL ? motif_addSeparator (menu -> menuWidget) :
		motif_addItem (menu -> menuWidget, itemTitle, flags, commonCallback, me);
	Collection_addItem (menu -> commands, me);
	my commandCallback = commandCallback;
	return my itemWidget;
}

Widget EditorCommand_getItemWidget (EditorCommand me) { return my itemWidget; }

EditorMenu Editor_addMenu (Any editor, const char *menuTitle, long flags) {
	EditorMenu me = new (EditorMenu);
	my editor = editor;
	if (! (my menuTitle = Melder_strdup (menuTitle))) { forget (me); return NULL; }
	my menuWidget = motif_addMenu (((Editor) editor) -> menuBar, menuTitle, flags);
	Collection_addItem (((Editor) editor) -> menus, me);
	my commands = Ordered_create ();
	return me;
}

Widget EditorMenu_getMenuWidget (EditorMenu me) { return my menuWidget; }

Widget Editor_addCommand (Any editor, const char *menuTitle, const char *itemTitle, long flags,
	int (*commandCallback) (EditorCommand, Any))
{
	Editor me = (Editor) editor;
	int numberOfMenus = my menus -> size, imenu;
	for (imenu = 1; imenu <= numberOfMenus; imenu ++) {
		EditorMenu menu = my menus -> item [imenu];
		if (strequ (menuTitle, menu -> menuTitle))
			return EditorMenu_addCommand (menu, itemTitle, flags, commandCallback);
	}
	return Melder_errorp ("(Editor_addCommand:) No menu \"%s\". Cannot insert command.", menuTitle);
}

Widget Editor_addCommandScript (Any editor, const char *menuTitle, const char *itemTitle, long flags,
	const char *script)
{
	Editor me = (Editor) editor;
	int numberOfMenus = my menus -> size, imenu;
	for (imenu = 1; imenu <= numberOfMenus; imenu ++) {
		EditorMenu menu = my menus -> item [imenu];
		if (strequ (menuTitle, menu -> menuTitle)) {
			EditorCommand cmd = new (EditorCommand);
			cmd -> editor = me;
			cmd -> menu = menu;
			cmd -> itemTitle = Melder_strdup (itemTitle);
			cmd -> itemWidget = script == NULL ? motif_addSeparator (menu -> menuWidget) :
				motif_addItem (menu -> menuWidget, itemTitle, flags, commonCallback, cmd);
			Collection_addItem (menu -> commands, cmd);
			cmd -> commandCallback = Editor_scriptCallback;
			cmd -> script = Melder_strdup (script);
			return cmd -> itemWidget;
		}
	}
	return Melder_errorp ("(Editor_addCommand:) No menu \"%s\". Cannot insert command.", menuTitle);
}

void Editor_setMenuSensitive (Any editor, const char *menuTitle, int sensitive) {
	Editor me = (Editor) editor;
	int numberOfMenus = my menus -> size, imenu;
	for (imenu = 1; imenu <= numberOfMenus; imenu ++) {
		EditorMenu menu = my menus -> item [imenu];
		if (strequ (menuTitle, menu -> menuTitle)) {
			XtSetSensitive (menu -> menuWidget, sensitive);
			return;
		}
	}
}

EditorCommand Editor_getMenuCommand (Any editor, const char *menuTitle, const char *itemTitle) {
	Editor me = (Editor) editor;
	int numberOfMenus = my menus -> size, imenu;
	for (imenu = 1; imenu <= numberOfMenus; imenu ++) {
		EditorMenu menu = my menus -> item [imenu];
		if (strequ (menuTitle, menu -> menuTitle)) {
			int numberOfCommands = menu -> commands -> size, icommand;
			for (icommand = 1; icommand <= numberOfCommands; icommand ++) {
				EditorCommand command = menu -> commands -> item [icommand];
				if (strequ (itemTitle, command -> itemTitle))
					return command;
			}
		}
	}
	return Melder_errorp ("(Editor_addCommand:) No menu \"%s\". Cannot insert command.", menuTitle);
}

int Editor_doMenuCommand (Any editor, const char *commandTitle, const char *arguments) {
	Editor me = (Editor) editor;
	int numberOfMenus = my menus -> size, imenu;
	for (imenu = 1; imenu <= numberOfMenus; imenu ++) {
		EditorMenu menu = my menus -> item [imenu];
		int numberOfCommands = menu -> commands -> size, icommand;
		for (icommand = 1; icommand <= numberOfCommands; icommand ++) {
			EditorCommand command = menu -> commands -> item [icommand];
			if (strequ (commandTitle, command -> itemTitle)) {
				if (! command -> commandCallback (command, (Any) arguments))
					return 0;
				return 1;
			}
		}
	}
	return Melder_error ("Command not available in %s.", our _className);
}

/********** class Editor **********/

static void destroy (I) {
	iam (Editor);
	Melder_stopPlaying (Melder_IMPLICIT);
	/*
	 * The following command must be performed before the shell is destroyed.
	 * Otherwise, we would be forgetting dangling command dialogs here.
	 */
	forget (my menus);
	if (my shell) {
		#if defined (UNIX)
			XtUnrealizeWidget (my shell);
		#endif
		XtDestroyWidget (my shell);
	}
	if (my destroyCallback) my destroyCallback (me, my destroyClosure);
	forget (my previousData);
	inherited (Editor) destroy (me);
}

static void nameChanged (I) {
	iam (Editor);
	XtVaSetValues (my shell, XmNtitle, my name ? my name : "",
		XmNiconName, my name ? my name : "", NULL);
}

static void goAway (I) { iam (Editor); forget (me); }

static void save (I) {
	iam (Editor);
	if (! my data) return;
	forget (my previousData);
	my previousData = Data_copy (my data);
}

static void restore (I) {
	iam (Editor);
	if (my data && my previousData)   /* Swap contents of my data and my previousData. */
		Thing_swap (my data, my previousData);
}

int Editor_scriptCallback (EditorCommand cmd, Any sender) {
	(void) sender;
	return DO_RunTheScriptFromAnyAddedEditorCommand (cmd -> editor, cmd -> script);
}

DIRECT (Editor, cb_close) our goAway (me); END

DIRECT (Editor, cb_undo)
	our restore (me);
	if (strnequ (my undoText, "Undo", 4)) my undoText [0] = 'R', my undoText [1] = 'e';
	else if (strnequ (my undoText, "Redo", 4)) my undoText [0] = 'U', my undoText [1] = 'n';
	else strcpy (my undoText, "Undo?");
	XtVaSetValues (my undoButton, motif_argXmString (XmNlabelString, my undoText), NULL);
	/*
	 * Send a message to myself (e.g., I will redraw myself).
	 */
	our dataChanged (me);
	/*
	 * Send a message to my boss (e.g., she will notify the others that depend on me).
	 */
	Editor_broadcastChange (me);
END

DIRECT (Editor, cb_searchManual) Melder_search (); END

DIRECT (Editor, cb_newScript)
	ScriptEditor scriptEditor = ScriptEditor_createFromText (my parent, me, NULL);
	if (! scriptEditor) return 0;
END

DIRECT (Editor, cb_openScript)
	ScriptEditor scriptEditor = ScriptEditor_createFromText (my parent, me, NULL);
	if (! scriptEditor) return 0;
	TextEditor_showOpen (scriptEditor);
END

static void createMenus (I) {
	iam (Editor);
	Editor_addMenu (me, "File", 0);
	if (our editable) {
		Editor_addMenu (me, "Edit", 0);
		if (my data)
			my undoButton = Editor_addCommand (me, "Edit", "Cannot undo", motif_INSENSITIVE + 'Z', cb_undo);
	}
}

static void createChildren (Any editor) {
	(void) editor;
}

static void dataChanged (Any editor) {
	(void) editor;
}

static void clipboardChanged (Any editor, Any clipboard) {
	(void) editor;
	(void) clipboard;
}

class_methods (Editor, Thing)
	class_method (destroy)
	class_method (nameChanged)
	class_method (goAway)
	us -> editable = TRUE;
	us -> scriptable = TRUE;
	class_method (createMenus)
	class_method (createChildren)
	class_method (dataChanged)
	class_method (save)
	class_method (restore)
	class_method (clipboardChanged)
class_methods_end

MOTIF_CALLBACK (cb_goAway)
	iam (Editor);
	our goAway (me);
MOTIF_CALLBACK_END

extern void praat_addCommandsToEditor (Editor me);
int Editor_init (I, Widget parent, int x, int y, int width, int height, const char *title, Any data) {
	iam (Editor);
	int screenWidth = WidthOfScreen (DefaultScreenOfDisplay (XtDisplay (parent)));
	int screenHeight = HeightOfScreen (DefaultScreenOfDisplay (XtDisplay (parent)));
	int left, right, top, bottom;
	screenHeight -= Machine_getTitleBarHeight ();
	if (width < 0) width += screenWidth;
	if (height < 0) height += screenHeight;
	if (width > screenWidth - 10) width = screenWidth - 10;
	if (height > screenHeight - 10) height = screenHeight - 10;
	if (x > 0)
		right = (left = x) + width;
	else if (x < 0)
		left = (right = screenWidth + x) - width;
	else /* Randomize. */
		right = (left = NUMrandomInteger (4, screenWidth - width - 4)) + width;
	if (y > 0)
		bottom = (top = y) + height;
	else if (y < 0)
		top = (bottom = screenHeight + y) - height;
	else /* Randomize. */
		bottom = (top = NUMrandomInteger (4, screenHeight - height - 4)) + height;
	#ifndef _WIN32
		top += Machine_getTitleBarHeight ();
		bottom += Machine_getTitleBarHeight ();
	#endif
	my parent = parent;   /* Probably praat.topShell */
	my shell = motif_addShell (parent, 0);   /* Note that XtParent (my shell) will be NULL! */
	if (! my shell) return 0;
	#if ! defined (sun4)
	{
		/* Catch Window Manager "Close" and "Quit". */
		Atom atom = XmInternAtom (XtDisplay (my shell), "WM_DELETE_WINDOW", True);
		XmAddWMProtocols (my shell, & atom, 1);
		XmAddWMProtocolCallback (my shell, atom, cb_goAway, (void *) me);
	}
	#endif
	XtVaSetValues (my shell, XmNdeleteResponse, XmDO_NOTHING,
		XmNx, left, XmNy, top, XmNwidth, right - left, XmNheight, bottom - top, NULL);
	/*if (width != 0) XtVaSetValues (my shell, XmNwidth, right - left, NULL);
	if (height != 0) XtVaSetValues (my shell, XmNheight, bottom - top, NULL);*/
	Thing_setName (me, title);
	my dialog = XtVaCreateWidget ("editor", xmFormWidgetClass, my shell,
		XmNautoUnmanage, False, XmNdialogStyle, XmDIALOG_MODELESS, NULL);
	if (! my dialog) return 0;
	my data = data;

	/* Create menus. */

	my menus = Ordered_create ();
	my menuBar = motif_addMenuBar (my dialog);
	Editor_addMenu (me, "Help", 0);
	our createMenus (me);
	Melder_clearError ();   /* TEMPORARY: to protect against CategoriesEditor */
	Editor_addCommand (me, "Help", "-- search --", 0, NULL);
	my searchButton = Editor_addCommand (me, "Help", "Search manual...", 'M', cb_searchManual);
	if (our scriptable) {
		Editor_addCommand (me, "File", "New editor script", 0, cb_newScript);
		Editor_addCommand (me, "File", "Open editor script...", 0, cb_openScript);
		Editor_addCommand (me, "File", "-- script --", 0, 0);
	}
	/*
	 * Add the scripted commands.
	 */
	praat_addCommandsToEditor (me);

	Editor_addCommand (me, "File", "Close", 'W', cb_close);
	XtManageChild (my menuBar);

	our createChildren (me);
	XtManageChild (my dialog);
	XtRealizeWidget (my shell);
	return 1;
}

void Editor_raise (I) {
	iam (Editor);
	XMapRaised (XtDisplay (my shell), XtWindow (my shell));
}
 
void Editor_dataChanged (I, Any data) {
	iam (Editor);
	/*if (data) my data = data; BUG */
	(void) data;
	our dataChanged (me);
}

void Editor_clipboardChanged (I, Any data) {
	iam (Editor);
	our clipboardChanged (me, data);
}

void Editor_setDestroyCallback (I, void (*cb) (I, void *closure), void *closure) {
	iam (Editor);
	my destroyCallback = cb;
	my destroyClosure = closure;
}

void Editor_broadcastChange (I) {
	iam (Editor);
	if (my dataChangedCallback)
		my dataChangedCallback (me, my dataChangedClosure, NULL);
}

void Editor_setDataChangedCallback (I, void (*cb) (I, void *closure, Any data), void *closure) {
	iam (Editor);
	my dataChangedCallback = cb;
	my dataChangedClosure = closure;
}

void Editor_setPublishCallback (I, void (*cb) (I, void *closure, Any publish), void *closure) {
	iam (Editor);
	my publishCallback = cb;
	my publishClosure = closure;
}

void Editor_setPublish2Callback (I, void (*cb) (I, void *closure, Any publish1, Any publish2), void *closure) {
	iam (Editor);
	my publish2Callback = cb;
	my publish2Closure = closure;
}

void Editor_save (I, const char *text) {
	iam (Editor);
	our save (me);
	if (! my undoButton) return;
	XtSetSensitive (my undoButton, True);
	sprintf (my undoText, "Undo %s", text);
	XtVaSetValues (my undoButton, motif_argXmString (XmNlabelString, my undoText), NULL);
}

/* End of file Editor.c */