/**
 * ImGui script editor (opens with F3, next to the level editor).
 *
 * Starting point of the visual scripting integration: a node-graph editor
 * in the style of Unreal 4 blueprints, built on the imnodes widget
 * (https://github.com/Nelarius/imnodes). Nodes are backed by the node-type
 * catalog in Ryu/Scripting/ScriptGraph.h - that catalog is the place where
 * new library functionality is registered as script nodes.
 *
 * Graph interactions (provided by imnodes):
 * - drag nodes with the left mouse button, pan with the middle/alt-drag,
 *   zoom with the mouse wheel
 * - drag from an output pin to an input pin to create a link
 *   (links are strictly output->input, one link per input pin)
 * - drag a link off a pin to detach (delete) it
 * - select nodes/links and press Delete to remove them
 * - right-click the canvas: popup to add a node at that position
 * - toolbar: New file (clears the graph), Save and Load (.rscript JSON
 *   files, assets/scripts), node palette with filter
 *
 * NOTE: execution of a graph is not implemented yet; this editor only
 * creates and edits the data.
 *
 * @author: Simon Nguyen
 * @date: 06/06/2025
 */

#pragma once

#include <Ryu/Scripting/ScriptGraph.h>

#include <imgui.h>
#include <Thirdparty/imgui-filebrowser/imfilebrowser.h>

#include <string>

namespace RyuScript
{

class Editor
{
  public:
    Editor();
    ~Editor();

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    void createEditorWidgets(bool* p_open);

    bool showScriptEditor = false;

  private:
    void ensureImNodesContext();
    void drawToolbar();
    void drawPalette();
    void drawNodeGraph();
    void drawNode(ScriptNode& node);
    /// pushes new/deleted links and nodes from imnodes into the graph
    void handleEditorEvents();
    /// deletes nodes/links that are selected in the editor
    void handleDeleteSelection();

    /// creates a node of the given type at the given grid position
    void addNodeAt(const std::string& type, float gridX, float gridY);
    /// saves/loads the graph to/from the current file path
    void saveFile();
    void loadFile();
    void newFile();

    ScriptGraph mGraph;

    std::string mFilePath;
    std::string mFileName;
    bool mDirty;

    ImGui::FileBrowser mFileDialog;
    bool mFileDialogSaveMode;

    /// palette filter
    char mFilter[64];

    /// imnodes context created lazily (needs the ImGui context)
    bool mImNodesReady;
};

} // namespace RyuScript
