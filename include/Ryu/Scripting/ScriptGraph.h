/**
 * Data model for the (visual) scripting system.
 *
 * A ScriptGraph is a node graph like the blueprints in Unreal 4: nodes
 * (backed by library functionality, see ScriptNodeCatalog) are connected
 * through pins; an output pin can drive one or more input pins, an input
 * pin accepts exactly one connection.
 *
 * The graph is serializable to/from JSON (nlohmann) so the script editor
 * can save/load whole graphs as files (assets/scripts/*.rscript).
 *
 * NOTE: this is the starting point of the scripting integration: the data
 * model and the editor exist, an execution/interpreter backend is not
 * implemented yet.
 *
 * @author: Simon Nguyen
 * @date: 06/06/2025
 */

#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace RyuScript
{

/// direction of a pin on a node
enum class PinKind
{
    Input,
    Output
};

/// one connection socket of a node
struct ScriptPin
{
    int id; /// unique within the graph
    std::string name;
    PinKind kind;
};

/// a node instance in the graph; placed and connected in the editor
struct ScriptNode
{
    int id;
    std::string type; /// key into the ScriptNodeCatalog
    std::string title; /// editable label, defaults to the catalog title
    float posX; /// editor canvas position
    float posY;
    std::vector<ScriptPin> pins;

    const ScriptPin* pin(int pinId) const;
};

/// a connection: one output pin drives one input pin
struct ScriptLink
{
    int id;
    int fromNodeId;
    int fromPinId;
    int toNodeId;
    int toPinId;
};

/// static description of a node type (library functionality behind a node)
struct NodeTypeInfo
{
    std::string type; /// unique key, e.g. "Event.OnLevelStart"
    std::string title; /// shown in the editor
    std::string category; /// palette grouping
    std::vector<std::string> inputPins;
    std::vector<std::string> outputPins;
};

/// catalog of the available node types; the "library" behind the nodes.
/// New node functionality is registered here, the editor picks it up
/// automatically.
class ScriptNodeCatalog
{
  public:
    static const std::vector<NodeTypeInfo>& nodeTypes();
    static const NodeTypeInfo* typeInfo(const std::string& type);
};

/// a whole script: nodes + links, with JSON (de)serialization
class ScriptGraph
{
  public:
    /// removes all nodes and links (new file)
    void clear();

    /// creates a node of the given catalog type at the editor position,
    /// returns the new node (or nullptr for an unknown type)
    ScriptNode* addNode(const std::string& type, float posX, float posY);

    /// creates a link; refuses connections that are no valid
    /// output->input pairs or that would connect an already occupied
    /// input pin; returns the link id or -1
    int addLink(int fromNodeId, int fromPinId, int toNodeId, int toPinId);

    void removeNode(int nodeId);
    void removeLink(int linkId);

    /// removes all links attached to the node (used before removing it)
    void removeLinksOfNode(int nodeId);

    /// true if the input pin of the link is already connected
    bool inputPinConnected(int nodeId, int pinId) const;

    const std::vector<ScriptNode>& nodes() const { return mNodes; }
    const std::vector<ScriptLink>& links() const { return mLinks; }
    std::vector<ScriptNode>& nodes() { return mNodes; }
    std::vector<ScriptLink>& links() { return mLinks; }

    ScriptNode* node(int nodeId);
    const ScriptNode* node(int nodeId) const;

    /// node owning the given pin id (pins are unique across the graph)
    ScriptNode* nodeByPin(int pinId);

    nlohmann::json toJson() const;
    /// replaces the content; returns false on a parse/schema error
    bool fromJson(const nlohmann::json& j);

  private:
    std::vector<ScriptNode> mNodes;
    std::vector<ScriptLink> mLinks;
    int mNextNodeId = 1;
    int mNextPinId = 1;
    int mNextLinkId = 1;
};

} // namespace RyuScript
