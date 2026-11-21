#include <Ryu/Core/TextureRegistry.h>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace
{

using json = nlohmann::json;

/// tileset metadata of \p image read from the optional <stem>.json sidecar
void
readTilesetSidecar(const std::filesystem::path& jsonPath,
                   SceneTextureRegistry::TilesetInfo& tileset)
{
    std::error_code ec;
    if (not std::filesystem::exists(jsonPath, ec)) { return; }

    try
    {
        json data;
        std::ifstream f(jsonPath);
        f >> data;
        tileset.tileWidth = data.value("tileSize", json::array({32, 32}))[0];
        tileset.tileHeight = data.value("tileSize", json::array({32, 32}))[1];
        tileset.margin = data.value("margin", 0);
        tileset.spacing = data.value("spacing", 0);
        if (data.contains("tiles"))
        {
            for (auto it = data.at("tiles").begin();
                 it != data.at("tiles").end(); ++it)
            {
                SceneTextureRegistry::TileCollision collision;
                const std::string kind
                    = it->value("collision", std::string("full"));
                if (kind == "none")
                {
                    collision.kind = SceneTextureRegistry::TileCollision::Kind::None;
                }
                else if (kind == "rect")
                {
                    collision.kind = SceneTextureRegistry::TileCollision::Kind::Rect;
                    const auto rect
                        = it->value("rect", std::vector<int>{ 0, 0, 0, 0 });
                    collision.rect = sf::IntRect({ rect.at(0), rect.at(1) },
                                                 { rect.at(2), rect.at(3) });
                }
                else if (kind == "polygons")
                {
                    collision.kind = SceneTextureRegistry::TileCollision::Kind::Polygons;
                    for (const auto& polygon : it->at("polygons"))
                    {
                        std::vector<sf::Vector2f> vertices;
                        for (const auto& vertex : polygon)
                        {
                            vertices.push_back(
                                { vertex.at(0).get<float>(),
                                  vertex.at(1).get<float>() });
                        }
                        collision.polygons.push_back(std::move(vertices));
                    }
                }
                tileset.tileCollisions[it.key()] = std::move(collision);
            }
        }
    }
    catch (const std::exception& e)
    {
        fmt::print("TextureRegistry: failed to parse {}: {}.\n",
                   jsonPath.string(), e.what());
    }
}

bool
isTilesetId(const std::vector<std::string>& tilesetIds, const std::string& id)
{
    return std::find(tilesetIds.begin(), tilesetIds.end(), id)
        != tilesetIds.end();
}

} // anonymous namespace

void
SceneTextureRegistry::addEntry(const std::string& id,
                               const std::filesystem::path& path,
                               bool isTileset)
{
    if (mEntries.contains(id)) { return; }
    Entry& entry = mEntries[id];
    entry.image = path;
    loadEntry(entry);
    if (isTileset)
    {
        entry.tileset = {}; // sidecar/default metadata is applied in loadEntry
        mTilesetIds.push_back(id);
    }
    else
    {
        mImageIds.push_back(id);
    }
}

void
SceneTextureRegistry::scanDirectory(const std::filesystem::path& dir,
                                     bool asTilesets)
{
    std::error_code ec;
    if (not std::filesystem::exists(dir, ec))
    {
        return;
    }

    std::vector<std::filesystem::path> pngs;
    for (const auto& entry : std::filesystem::directory_iterator{ dir })
    {
        if (entry.path().extension() == ".png") { pngs.push_back(entry.path()); }
    }
    std::sort(pngs.begin(), pngs.end());
    for (const auto& png : pngs)
    {
        addEntry(png.stem().string(), png, asTilesets);
    }
}

void
SceneTextureRegistry::loadEntry(Entry& entry)
{
    entry.loaded = entry.texture.loadFromFile(entry.image);
    if (not entry.loaded)
    {
        fmt::print("TextureRegistry: could not load image '{}'.\n",
                   entry.image.string());
        return;
    }
}

void
SceneTextureRegistry::addLegacyEntries(const std::string& scenesDirectory)
{
    const auto base = std::filesystem::path(scenesDirectory);
    const auto legacy = std::map<std::string, std::filesystem::path>{
        { "BGMountain", "assets/backgrounds/99_dummy/722756.png" },
        { "BoxPushable", base / "99_dummy/box_wood.png" },
        { "BoxHeavy", base / "99_dummy/box_wood.png" },
        { "Button", base / "99_dummy/tile_button_1.png" },
        { "Grass", base / "99_dummy/tile_grass_1.png" },
        { "Grate", base / "99_dummy/tile_grate_1.png" },
        { "Ground", base / "99_dummy/tile_grass_1.png" },
        { "Ladder", base / "99_dummy/tile_grate_1.png" },
        { "Teleport", base / "99_dummy/tile_teleport_1.png" },
        { "Unknown", base / "99_dummy/box_empty.png" },
    };
    for (const auto& [id, path] : legacy)
    {
        addEntry(id, path, false);
    }
}

void
SceneTextureRegistry::load(const std::string& scenesDirectory)
{
    mEntries.clear();
    mImageIds.clear();
    mTilesetIds.clear();
    mSemantics.clear();

    addLegacyEntries(scenesDirectory);
    scanDirectory(std::filesystem::path(scenesDirectory) / "objects", false);
    scanDirectory(std::filesystem::path(scenesDirectory) / "tilesets", true);

    // semantic mappings are applied after the concrete ids are known so the
    // load log can flag mappings that point at nothing
    loadSemanticMappings(std::filesystem::path(scenesDirectory)
                              / "semantic.json");

    // tileset entries get their sidecar metadata + column/row counts
    // (image-only entries registered above keep the default metadata)
    for (const auto& id : mTilesetIds)
    {
        Entry& entry = mEntries.at(id);
        readTilesetSidecar(entry.image.parent_path()
                               / (entry.image.stem().string() + ".json"),
                           entry.tileset);
        const auto size = entry.texture.getSize();
        const auto usableX = static_cast<int>(std::max(
            0, static_cast<int>(size.x) - 2 * entry.tileset.margin
                + entry.tileset.spacing));
        const auto usableY = static_cast<int>(std::max(
            0, static_cast<int>(size.y) - 2 * entry.tileset.margin
                + entry.tileset.spacing));
        const auto stepX =
            entry.tileset.tileWidth + entry.tileset.spacing;
        const auto stepY =
            entry.tileset.tileHeight + entry.tileset.spacing;
        entry.tileset.columns = stepX > 0 ? usableX / stepX : 0;
        entry.tileset.rows = stepY > 0 ? usableY / stepY : 0;
        fmt::print("TextureRegistry: tileset '{}' {}x{} tiles of {}x{}.\n",
                   id, entry.tileset.columns, entry.tileset.rows,
                   entry.tileset.tileWidth, entry.tileset.tileHeight);
    }

    fmt::print("TextureRegistry: {} textures ({} whole images, {} tilesets).\n",
               mEntries.size(), mImageIds.size(), mTilesetIds.size());
}

SceneTextureRegistry::Entry*
SceneTextureRegistry::find(const std::string& id)
{
    const auto it = mEntries.find(id);
    return it != mEntries.end() ? &it->second : nullptr;
}

const SceneTextureRegistry::Entry*
SceneTextureRegistry::find(const std::string& id) const
{
    const auto it = mEntries.find(id);
    return it != mEntries.end() ? &it->second : nullptr;
}

const sf::Texture*
SceneTextureRegistry::texture(const std::string& id) const
{
    std::string concreteId = id;
    int col = -1;
    int row = -1;
    resolveConcrete(concreteId, col, row);
    const Entry* entry = find(concreteId);
    if (entry == nullptr or not entry->loaded) { return nullptr; }
    return &entry->texture;
}

const SceneTextureRegistry::TilesetInfo*
SceneTextureRegistry::tileset(const std::string& id) const
{
    std::string concreteId = id;
    int col = -1;
    int row = -1;
    resolveConcrete(concreteId, col, row);
    if (not isTilesetId(mTilesetIds, concreteId)) { return nullptr; }
    const Entry* entry = find(concreteId);
    return entry != nullptr ? &entry->tileset : nullptr;
}

sf::IntRect
SceneTextureRegistry::textureRect(const std::string& id, int tileCol,
                                   int tileRow) const
{
    std::string concreteId = id;
    resolveConcrete(concreteId, tileCol, tileRow);
    const Entry* entry = find(concreteId);
    const sf::Vector2u size =
        (entry != nullptr and entry->loaded) ? entry->texture.getSize()
                                             : sf::Vector2u{ 8u, 8u };
    if (entry == nullptr or tileCol < 0 or tileRow < 0
        or not isTilesetId(mTilesetIds, concreteId))
    {
        return sf::IntRect({ 0, 0 },
                           { static_cast<int>(size.x),
                             static_cast<int>(size.y) });
    }
    const TilesetInfo& info = entry->tileset;
    if (info.columns <= 0 or info.rows <= 0)
    {
        return sf::IntRect({ 0, 0 },
                           { static_cast<int>(size.x),
                             static_cast<int>(size.y) });
    }
    const int col = tileCol % info.columns;
    const int row = tileRow % info.rows;
    const sf::Vector2i position{
        info.margin + col * (info.tileWidth + info.spacing),
        info.margin + row * (info.tileHeight + info.spacing) };
    return sf::IntRect(position, { info.tileWidth, info.tileHeight });
}

const SceneTextureRegistry::SemanticInfo*
SceneTextureRegistry::semantic(const std::string& id) const
{
    const auto it = mSemantics.find(id);
    return it != mSemantics.end() ? &it->second : nullptr;
}

std::string
SceneTextureRegistry::semanticIdFor(const std::string& textureId, int tileCol,
                                     int tileRow) const
{
    for (const auto& [id, info] : mSemantics)
    {
        if (info.textureId != textureId or info.tileCol != tileCol
            or info.tileRow != tileRow)
        {
            continue;
        }
        return id;
    }
    return {};
}

sf::Vector2f
SceneTextureRegistry::semanticOffsetPx(const std::string& id) const
{
    const auto it = mSemantics.find(id);
    if (it == mSemantics.end()) { return {}; }
    return { it->second.offsetX, it->second.offsetY };
}

void
SceneTextureRegistry::loadSemanticMappings(const std::filesystem::path& jsonPath)
{
    std::error_code ec;
    if (not std::filesystem::exists(jsonPath, ec)) { return; }

    try
    {
        json data;
        std::ifstream f(jsonPath);
        f >> data;
        for (auto it = data.begin(); it != data.end(); ++it)
        {
            SemanticInfo info;
            info.textureId = it->value("textureId", std::string());
            if (it->contains("tile"))
            {
                info.tileCol = it->at("tile").at(0).get<int>();
                info.tileRow = it->at("tile").at(1).get<int>();
            }
            if (it->contains("offsetPx"))
            {
                info.offsetX = it->at("offsetPx").at(0).get<float>();
                info.offsetY = it->at("offsetPx").at(1).get<float>();
            }
            mSemantics[it.key()] = info;
            fmt::print("TextureRegistry: semantic '{}' -> '{}' [{},{}]"
                       " (+{},{} px).\n",
                       it.key(), info.textureId, info.tileCol, info.tileRow,
                       info.offsetX, info.offsetY);
        }
        fmt::print("TextureRegistry: loaded {} semantic mapping(s).\n",
                   mSemantics.size());
    }
    catch (const std::exception& e)
    {
        fmt::print("TextureRegistry: failed to parse {}: {}.\n",
                   jsonPath.string(), e.what());
    }
}

void
SceneTextureRegistry::resolveConcrete(std::string& id, int& tileCol,
                                       int& tileRow) const
{
    const auto it = mSemantics.find(id);
    if (it == mSemantics.end()) { return; }
    const SemanticInfo& info = it->second;
    id = info.textureId;
    tileCol = info.tileCol;
    tileRow = info.tileRow;
}

const SceneTextureRegistry::TileCollision*
SceneTextureRegistry::tileCollision(const std::string& id, int tileCol,
                                     int tileRow) const
{
    std::string concreteId = id;
    resolveConcrete(concreteId, tileCol, tileRow);
    if (tileCol < 0 or tileRow < 0) { return nullptr; }
    const Entry* entry = find(concreteId);
    if (entry == nullptr) { return nullptr; }
    const auto it = entry->tileset.tileCollisions.find(
        fmt::format("{},{}", tileCol, tileRow));
    if (it == entry->tileset.tileCollisions.end()) { return nullptr; }
    return &it->second;
}

void
SceneTextureRegistry::setTileCollision(const std::string& id, int tileCol,
                                       int tileRow, const TileCollision& collision)
{
    Entry* entry = find(id);
    if (entry == nullptr or tileCol < 0 or tileRow < 0)
    {
        return;
    }
    const std::string key = fmt::format("{},{}", tileCol, tileRow);
    if (collision.kind == TileCollision::Kind::Full)
    {
        entry->tileset.tileCollisions.erase(key);
    }
    else
    {
        entry->tileset.tileCollisions[key] = collision;
    }
}

bool
SceneTextureRegistry::saveTilesetMetadata(const std::string& id)
{
    const Entry* entry = find(id);
    if (entry == nullptr) { return false; }
    const std::filesystem::path jsonPath = entry->image.parent_path()
        / (entry->image.stem().string() + ".json");

    json tiles = json::object();
    for (const auto& [key, collision] : entry->tileset.tileCollisions)
    {
        json tile;
        switch (collision.kind)
        {
        case TileCollision::Kind::None:
            tile["collision"] = "none";
            break;
        case TileCollision::Kind::Rect:
            tile["collision"] = "rect";
            tile["rect"] = { collision.rect.position.x,
                             collision.rect.position.y,
                             collision.rect.size.x, collision.rect.size.y };
            break;
        case TileCollision::Kind::Polygons:
        {
            tile["collision"] = "polygons";
            json polygons = json::array();
            for (const auto& polygon : collision.polygons)
            {
                json vertices = json::array();
                for (const auto& vertex : polygon)
                {
                    vertices.push_back(
                        json::array({ vertex.x, vertex.y }));
                }
                polygons.push_back(std::move(vertices));
            }
            tile["polygons"] = std::move(polygons);
            break;
        }
        case TileCollision::Kind::Full:
            continue;
        }
        tiles[key] = std::move(tile);
    }

    json out = {
        { "tileSize", json::array(
              { entry->tileset.tileWidth, entry->tileset.tileHeight }) },
        { "margin", entry->tileset.margin },
        { "spacing", entry->tileset.spacing }
    };
    if (not tiles.empty()) { out["tiles"] = std::move(tiles); }

    std::ofstream f(jsonPath);
    f << out.dump(2) << "\n";
    fmt::print("TextureRegistry: saved tileset metadata of '{}' to {}.\n",
               id, jsonPath.string());
    return true;
}

const sf::Texture&
SceneTextureRegistry::fallbackTexture() const
{
    if (not mFallbackLoaded)
    {
        mFallbackLoaded = mFallback.resize({ 8, 8 });
        if (mFallbackLoaded)
        {
            sf::Image image({ 8, 8 }, sf::Color::White);
            mFallback.update(image);
        }
    }
    return mFallback;
}
