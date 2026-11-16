#include <RyuSuite/ScriptEditor.h>

#include <Ryu/Scripting/ScriptGraph.h>

#include <imgui.h>
#include <imnodes.h>

#include <fmt/core.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace imnodes = ImNodes;

namespace RyuScript
{

Editor::Editor()
:mFileName("untitled.rscript")
,mDirty(false)
,mFileDialogSaveMode(false)
,mImNodesReady(false)
{
    mFilter[0] = '\0';
}

Editor::~Editor()
{
    imnodes::DestroyContext();
}

void
Editor::newFile()
{
    mGraph.clear();
    mFileName = "untitled.rscript";
    mFilePath.clear();
    mDirty = false;
}

void
Editor::saveFile()
{
    if (mFilePath.empty())
    {
        mFileDialogSaveMode = true;
        mFileDialog.SetTitle("Save Ryu script");
        mFileDialog.Open();
        return;
    }
    fs::path path(mFilePath);
    if (path.has_parent_path())
    {
        fs::create_directories(path.parent_path());
    }
    // sync the node positions from the editor before serializing
    for (auto& node : mGraph.nodes())
    {
        const ImVec2 pos = imnodes::GetNodeGridSpacePos(node.id);
        node.posX = pos.x;
        node.posY = pos.y;
    }
    std::ofstream out(mFilePath);
    out << mGraph.toJson().dump(2);
    mDirty = false;
    fmt::print("RyuScript: saved {}\n", mFilePath);
}

void
Editor::loadFile()
{
    mFileDialogSaveMode = false;
    mFileDialog.SetTitle("Load Ryu script");
    mFileDialog.Open();
}

void
Editor::addNodeAt(const std::string& type, float gridX, float gridY)
{
    ScriptNode* node = mGraph.addNode(type, gridX, gridY);
    if (node == nullptr) { return; }
    imnodes::SetNodeGridSpacePos(node->id, ImVec2(gridX, gridY));
    mDirty = true;
}

void
Editor::createEditorWidgets(bool* p_open)
{
    if (!*p_open) { return; }

    ensureImNodesContext();
    ImGui::SetNextWindowSize({760.f, 480.f}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Ryu script editor", p_open))
    {
        drawToolbar();
        drawPalette();

        mFileDialog.Display();
        if (mFileDialog.HasSelected())
        {
            fs::path selected = mFileDialog.GetSelected();
            mFileDialog.ClearSelected();
            if (mFileDialogSaveMode)
            {
                mFilePath = selected.string();
                mFileName = selected.filename().string();
                saveFile();
            }
            else
            {
                std::ifstream in(selected.string());
                if (!in)
                {
                    fmt::print("RyuScript: could not open {}\n",
                               selected.string());
                }
                else
                {
                    try
                    {
                        nlohmann::json j = nlohmann::json::parse(in);
                        if (mGraph.fromJson(j))
                        {
                            mFilePath = selected.string();
                            mFileName = selected.filename().string();
                            mDirty = false;
                            for (auto& node : mGraph.nodes())
                            {
                                imnodes::SetNodeGridSpacePos(
                                    node.id, ImVec2(node.posX, node.posY));
                            }
                            fmt::print("RyuScript: loaded {}\n", mFilePath);
                        }
                        else
                        {
                            fmt::print("RyuScript: invalid script file {}\n",
                                       selected.string());
                        }
                    }
                    catch (const nlohmann::json::exception& e)
                    {
                        fmt::print("RyuScript: parse error: {}\n", e.what());
                    }
                }
            }
        }

        drawNodeGraph();
    }
    ImGui::End();
}

/// the imnodes context needs the ImGui context, which is created in
/// Game::run (after the editors were constructed) - so create it lazily
void
Editor::ensureImNodesContext()
{
    if (!mImNodesReady)
    {
        imnodes::CreateContext();
        imnodes::StyleColorsDark();
        mImNodesReady = true;
    }
}

void
Editor::drawToolbar()
{
    if (ImGui::Button("New"))
    {
        newFile();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        saveFile();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        loadFile();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(mFileName.c_str());
    if (mDirty)
    {
        ImGui::SameLine();
        ImGui::TextUnformatted("*");
    }
}

void
Editor::drawPalette()
{
    if (!ImGui::CollapsingHeader("Node palette",
                                 ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }
    ImGui::InputTextWithHint("##script_filter", "filter nodes", mFilter,
                             sizeof(mFilter));
    const auto& types = ScriptNodeCatalog::nodeTypes();
    std::string lastCategory;
    for (const auto& info : types)
    {
        if (mFilter[0] != '\0'
            && info.title.find(mFilter) == std::string::npos
            && info.type.find(mFilter) == std::string::npos)
        {
            continue;
        }
        if (info.category != lastCategory)
        {
            ImGui::SeparatorText(info.category.c_str());
            lastCategory = info.category;
        }
        ImGui::Bullet();
        ImGui::SameLine();
        if (ImGui::Selectable(info.title.c_str()))
        {
            addNodeAt(info.type, 0.f, 0.f);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", info.type.c_str());
        }
    }
}

void
Editor::drawNodeGraph()
{
    imnodes::BeginNodeEditor();

    // add-node popup over the canvas
    {
        const bool openPopup = ImGui::IsWindowFocused(
                                   ImGuiFocusedFlags_RootAndChildWindows)
                               && imnodes::IsEditorHovered()
                               && ImGui::IsMouseReleased(
                                      ImGuiMouseButton_Right);
        if (openPopup)
        {
            ImGui::OpenPopup("script_add_node");
        }
        if (ImGui::BeginPopup("script_add_node"))
        {
            const ImVec2 clickPos =
                ImGui::GetMousePosOnOpeningCurrentPopup();
            std::string lastCategory;
            for (const auto& info : ScriptNodeCatalog::nodeTypes())
            {
                if (info.category != lastCategory)
                {
                    ImGui::SeparatorText(info.category.c_str());
                    lastCategory = info.category;
                }
                if (ImGui::MenuItem(info.title.c_str()))
                {
 addNodeAt(info.type, clickPos.x - 20.f, clickPos.y - 20.f);
                }
            }
            ImGui::EndPopup();
        }
    }

    for (auto& node : mGraph.nodes())
    {
        drawNode(node);
    }

    for (const auto& link : mGraph.links())
    {
        imnodes::Link(link.id, link.fromPinId, link.toPinId);
    }

    imnodes::EndNodeEditor();

    handleEditorEvents();
    handleDeleteSelection();
}

void
Editor::drawNode(ScriptNode& node)
{
    imnodes::BeginNode(node.id);
    imnodes::BeginNodeTitleBar();
    ImGui::TextUnformatted(node.title.c_str());
    imnodes::EndNodeTitleBar();

    // pins in two columns: inputs left, outputs right; rows are filled
    // top-down, missing pins leave an empty cell
    size_t inIndex = 0;
    size_t outIndex = 0;
    size_t numInputs = 0;
    size_t numOutputs = 0;
    for (const auto& pin : node.pins)
    {
        if (pin.kind == PinKind::Input) { ++numInputs; }
        else { ++numOutputs; }
    }
    const size_t numRows = numInputs > numOutputs ? numInputs : numOutputs;

    for (size_t row = 0; row < numRows; ++row)
    {
        if (inIndex < numInputs)
        {
            // find the inIndex-th input pin
            const ScriptPin* pin = nullptr;
            size_t seen = 0;
            for (const auto& p : node.pins)
            {
                if (p.kind == PinKind::Input && seen++ == inIndex)
                {
                    pin = &p;
                    break;
                }
            }
            imnodes::BeginInputAttribute(pin->id);
            ImGui::TextUnformatted(pin->name.c_str());
            imnodes::EndInputAttribute();
            ++inIndex;
        }
        else
        {
            ImGui::TextUnformatted("");
        }
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(24.f, 0.f));
        ImGui::SameLine();
        if (outIndex < numOutputs)
        {
            const ScriptPin* pin = nullptr;
            size_t seen = 0;
            for (const auto& p : node.pins)
            {
                if (p.kind == PinKind::Output && seen++ == outIndex)
                {
                    pin = &p;
                    break;
                }
            }
            imnodes::BeginOutputAttribute(pin->id);
            ImGui::TextUnformatted(pin->name.c_str());
            imnodes::EndOutputAttribute();
            ++outIndex;
        }
    }

    imnodes::EndNode();
}

void
Editor::handleEditorEvents()
{
    int startNode = 0;
    int startPin = 0;
    int endNode = 0;
    int endPin = 0;
    if (imnodes::IsLinkCreated(&startNode, &startPin, &endNode, &endPin))
    {
        // imnodes reports the attributes in drag order; order the link so
        // that it really goes output -> input
        const ScriptNode* start = mGraph.node(startNode);
        const ScriptNode* end = mGraph.node(endNode);
        if (start != nullptr && end != nullptr)
        {
            const ScriptPin* startP = start->pin(startPin);
            const ScriptPin* endP = end->pin(endPin);
            if (startP != nullptr && endP != nullptr)
            {
                if (startP->kind == PinKind::Input)
                {
                    std::swap(startNode, endNode);
                    std::swap(startPin, endPin);
                }
                mGraph.addLink(startNode, startPin, endNode, endPin);
                mDirty = true;
            }
        }
    }

    int destroyedLinkId = 0;
    while (imnodes::IsLinkDestroyed(&destroyedLinkId))
    {
        mGraph.removeLink(destroyedLinkId);
        mDirty = true;
    }
}

void
Editor::handleDeleteSelection()
{
    const int numNodes = imnodes::NumSelectedNodes();
    const int numLinks = imnodes::NumSelectedLinks();
    if (numNodes + numLinks == 0) { return; }
    if (!ImGui::IsKeyReleased(ImGuiKey_Delete)) { return; }

    if (numLinks > 0)
    {
        std::vector<int> selected(numLinks);
        imnodes::GetSelectedLinks(selected.data());
        for (int id : selected)
        {
            mGraph.removeLink(id);
        }
    }
    if (numNodes > 0)
    {
        std::vector<int> selected(numNodes);
        imnodes::GetSelectedNodes(selected.data());
        for (int id : selected)
        {
            mGraph.removeNode(id);
        }
    }
    mDirty = true;
}

} // namespace RyuScript
