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

    addLegacyEntries(scenesDirectory);
    scanDirectory(std::filesystem::path(scenesDirectory) / "objects", false);
    scanDirectory(std::filesystem::path(scenesDirectory) / "tilesets", true);

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
    const Entry* entry = find(id);
    if (entry == nullptr or not entry->loaded) { return nullptr; }
    return &entry->texture;
}

const SceneTextureRegistry::TilesetInfo*
SceneTextureRegistry::tileset(const std::string& id) const
{
    if (not isTilesetId(mTilesetIds, id)) { return nullptr; }
    const Entry* entry = find(id);
    return entry != nullptr ? &entry->tileset : nullptr;
}

sf::IntRect
SceneTextureRegistry::textureRect(const std::string& id, int tileCol,
                                   int tileRow) const
{
    const Entry* entry = find(id);
    const sf::Vector2u size =
        (entry != nullptr and entry->loaded) ? entry->texture.getSize()
                                             : sf::Vector2u{ 8u, 8u };
    if (entry == nullptr or tileCol < 0 or tileRow < 0
        or not isTilesetId(mTilesetIds, id))
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
