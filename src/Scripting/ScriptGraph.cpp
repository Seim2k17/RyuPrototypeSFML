#include <Ryu/Scripting/ScriptGraph.h>

#include <algorithm>
#include <utility>

namespace RyuScript
{

const std::vector<NodeTypeInfo>&
ScriptNodeCatalog::nodeTypes()
{
    static const std::vector<NodeTypeInfo> types = {
        {"Event.OnLevelStart", "On Level Start", "Event", {}, {"then"}},
        {"Event.OnCollide", "On Collide", "Event", {}, {"then", "other"}},
        {"Action.SpawnObject", "Spawn Object", "Action", {"in", "object"}, {"done"}},
        {"Action.PlaySound", "Play Sound", "Action", {"in", "sound"}, {"done"}},
        {"Action.SetGravity", "Set Gravity", "Action", {"in", "value"}, {"done"}},
        {"Logic.Delay", "Delay", "Logic", {"in", "seconds"}, {"done"}},
        {"Logic.Branch", "Branch", "Logic", {"in", "condition"}, {"true", "false"}},
        {"Value.Float", "Float", "Value", {}, {"out"}},
        {"Value.String", "String", "Value", {}, {"out"}},
    };
    return types;
}

const NodeTypeInfo*
ScriptNodeCatalog::typeInfo(const std::string& type)
{
    const auto& types = nodeTypes();
    for (const auto& info : types)
    {
        if (info.type == type) { return &info; }
    }
    return nullptr;
}

const ScriptPin*
ScriptNode::pin(int pinId) const
{
    for (const auto& p : pins)
    {
        if (p.id == pinId) { return &p; }
    }
    return nullptr;
}

void
ScriptGraph::clear()
{
    mNodes.clear();
    mLinks.clear();
    mNextNodeId = 1;
    mNextPinId = 1;
    mNextLinkId = 1;
}

ScriptNode*
ScriptGraph::addNode(const std::string& type, float posX, float posY)
{
    const NodeTypeInfo* info = ScriptNodeCatalog::typeInfo(type);
    if (info == nullptr) { return nullptr; }

    ScriptNode node;
    node.id = mNextNodeId++;
    node.type = info->type;
    node.title = info->title;
    node.posX = posX;
    node.posY = posY;
    for (const auto& name : info->inputPins)
    {
        node.pins.push_back({mNextPinId++, name, PinKind::Input});
    }
    for (const auto& name : info->outputPins)
    {
        node.pins.push_back({mNextPinId++, name, PinKind::Output});
    }
    mNodes.push_back(std::move(node));
    return &mNodes.back();
}

bool
ScriptGraph::inputPinConnected(int nodeId, int pinId) const
{
    for (const auto& link : mLinks)
    {
        if (link.toNodeId == nodeId && link.toPinId == pinId) { return true; }
    }
    return false;
}

int
ScriptGraph::addLink(int fromNodeId, int fromPinId, int toNodeId, int toPinId)
{
    const ScriptNode* from = node(fromNodeId);
    const ScriptNode* to = node(toNodeId);
    if (from == nullptr || to == nullptr) { return -1; }

    const ScriptPin* outPin = from->pin(fromPinId);
    const ScriptPin* inPin = to->pin(toPinId);
    if (outPin == nullptr || inPin == nullptr) { return -1; }
    if (outPin->kind != PinKind::Output || inPin->kind != PinKind::Input)
    {
        return -1;
    }
    if (fromNodeId == toNodeId) { return -1; }
    if (inputPinConnected(toNodeId, toPinId)) { return -1; }

    mLinks.push_back(
        {mNextLinkId++, fromNodeId, fromPinId, toNodeId, toPinId});
    return mLinks.back().id;
}

void
ScriptGraph::removeLinksOfNode(int nodeId)
{
    mLinks.erase(std::remove_if(mLinks.begin(), mLinks.end(),
                                 [nodeId](const ScriptLink& link)
                                 {
                                     return link.fromNodeId == nodeId
                                         || link.toNodeId == nodeId;
                                 }),
                 mLinks.end());
}

void
ScriptGraph::removeNode(int nodeId)
{
    removeLinksOfNode(nodeId);
    mNodes.erase(std::remove_if(mNodes.begin(), mNodes.end(),
                                [nodeId](const ScriptNode& node)
                                { return node.id == nodeId; }),
                 mNodes.end());
}

void
ScriptGraph::removeLink(int linkId)
{
    mLinks.erase(std::remove_if(mLinks.begin(), mLinks.end(),
                                [linkId](const ScriptLink& link)
                                { return link.id == linkId; }),
                 mLinks.end());
}

ScriptNode*
ScriptGraph::node(int nodeId)
{
    for (auto& node : mNodes)
    {
        if (node.id == nodeId) { return &node; }
    }
    return nullptr;
}

const ScriptNode*
ScriptGraph::node(int nodeId) const
{
    for (const auto& node : mNodes)
    {
        if (node.id == nodeId) { return &node; }
    }
    return nullptr;
}

ScriptNode*
ScriptGraph::nodeByPin(int pinId)
{
    for (auto& node : mNodes)
    {
        if (node.pin(pinId) != nullptr) { return &node; }
    }
    return nullptr;
}

nlohmann::json
ScriptGraph::toJson() const
{
    using nlohmann::json;

    json j;
    j["nodes"] = json::array();
    for (const auto& node : mNodes)
    {
        json pins = json::array();
        for (const auto& pin : node.pins)
        {
            pins.push_back({{"id", pin.id},
                            {"name", pin.name},
                            {"kind", pin.kind == PinKind::Input ? "input"
                                                                : "output"}});
        }
        j["nodes"].push_back({{"id", node.id},
                              {"type", node.type},
                              {"title", node.title},
                              {"pos", {node.posX, node.posY}},
                              {"pins", pins}});
    }

    j["links"] = json::array();
    for (const auto& link : mLinks)
    {
        j["links"].push_back({{"id", link.id},
                              {"from", {link.fromNodeId, link.fromPinId}},
                              {"to", {link.toNodeId, link.toPinId}}});
    }
    return j;
}

bool
ScriptGraph::fromJson(const nlohmann::json& j)
{
    using nlohmann::json;

    if (!j.contains("nodes") || !j.contains("links")
        || !j["nodes"].is_array() || !j["links"].is_array())
    {
        return false;
    }

    clear();
    for (const auto& jn : j["nodes"])
    {
        if (!jn.contains("id") || !jn.contains("type") || !jn.contains("pos"))
        {
            return false;
        }
        ScriptNode node;
        node.id = jn["id"].get<int>();
        node.type = jn["type"].get<std::string>();
        node.title = jn.value("title", node.type);
        node.posX = jn["pos"][0].get<float>();
        node.posY = jn["pos"][1].get<float>();
        if (const NodeTypeInfo* info = ScriptNodeCatalog::typeInfo(node.type))
        {
            if (node.title.empty()) { node.title = info->title; }
        }
        if (jn.contains("pins"))
        {
            for (const auto& jp : jn["pins"])
            {
                node.pins.push_back(
                    {jp["id"].get<int>(),
                     jp.value("name", std::string("pin")),
                     jp["kind"].get<std::string>() == "input"
                         ? PinKind::Input
                         : PinKind::Output});
            }
        }
        else if (const NodeTypeInfo* info
                 = ScriptNodeCatalog::typeInfo(node.type))
        {
            for (const auto& name : info->inputPins)
            {
                node.pins.push_back({mNextPinId++, name, PinKind::Input});
            }
            for (const auto& name : info->outputPins)
            {
                node.pins.push_back({mNextPinId++, name, PinKind::Output});
            }
        }
        mNodes.push_back(std::move(node));
        mNextNodeId = std::max(mNextNodeId, node.id + 1);
        for (const auto& pin : mNodes.back().pins)
        {
            mNextPinId = std::max(mNextPinId, pin.id + 1);
        }
    }

    for (const auto& jl : j["links"])
    {
        ScriptLink link;
        link.id = jl["id"].get<int>();
        link.fromNodeId = jl["from"][0].get<int>();
        link.fromPinId = jl["from"][1].get<int>();
        link.toNodeId = jl["to"][0].get<int>();
        link.toPinId = jl["to"][1].get<int>();
        mLinks.push_back(link);
        mNextLinkId = std::max(mNextLinkId, link.id + 1);
    }
    return true;
}

} // namespace RyuScript
