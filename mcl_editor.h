/** ============================================================================
 *
 * mcl_editor JUCE module
 *
 * Copyright (C) Jonathan Zrake
 *
 * You may use, distribute and modify this code under the terms of the GPL3
 * license.
 * =============================================================================
 */

/******************************************************************************

BEGIN_JUCE_MODULE_DECLARATION

  ID:               mcl_editor
  vendor:           Jonathan Zrake
  version:          0.0.1
  name:             MCL Code Editor
  description:      modern code editor for JUCE
  website:          https://github.com/jzrake/MclTextEditor
  license:          GPL

  dependencies:     juce_core, juce_cryptography, juce_data_structures, juce_events, juce_graphics, juce_gui_basics, juce_gui_extra

END_JUCE_MODULE_DECLARATION

******************************************************************************/

#ifndef MCL_EDITOR_INCLUDED
#define MCL_EDITOR_INCLUDED

#include "AppConfig.h"

#include <juce_gui_extra/juce_gui_extra.h>



/** CONFIG: MCL_ENABLE_OPEN_GL
*
*	Disable this if your project doesn't include the open gl module
*/
#ifndef MCL_ENABLE_OPEN_GL
#define MCL_ENABLE_OPEN_GL 1
#endif

/** Config: TEST_MULTI_CARET_EDITING
*
*	Enable this to test the multi caret editing
*/
#ifndef TEST_MULTI_CARET_EDITING
#define TEST_MULTI_CARET_EDITING 1
#endif

/** Config: TEST_SYNTAX_SUPPORT
*
*	Enable this to test the syntax highlighting
*/
#ifndef TEST_SYNTAX_SUPPORT
#define TEST_SYNTAX_SUPPORT 1
#endif


/** Config: ENABLE_CARET_BLINK
*
*	Enable this to make the caret blink (disabled by default)
*/
#ifndef ENABLE_CARET_BLINK
#define ENABLE_CARET_BLINK 1
#endif

/** Config: PROFILE_PAINTS
*
*	Enable this to show profile statistics for the paint routine performance.
*/
#ifndef PROFILE_PAINTS
#define PROFILE_PAINTS 0
#endif


// I'd suggest to split up this big file to multiple files per class and include them here one by one
#include "code_editor/TextEditor.hpp"


#endif   // MCL_EDITOR_INCLUDED
