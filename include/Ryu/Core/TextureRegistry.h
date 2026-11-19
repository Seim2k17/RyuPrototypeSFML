/**
 * Data-driven scene texture registry.
 *
 * Replaces the hard-coded Textures::SceneID enum as the source of truth for
 * scene-object textures: whole-image textures and tilesets are discovered
 * from the asset directories at startup, so adding a texture means
 * dropping a png into the folder - no recompile, no enum edit.
 *
 * Layout (see AGENTS.md / level editor docs):
 * - assets/scenes/objects/   one png = one whole-image texture (id = stem)
 * - assets/scenes/tilesets/  png + optional <stem>.json sidecar with
 *                            tileSize/margin/spacing (id = stem);
 *                            default tile size 32x32 when no sidecar exists
 *
 * The legacy enum names (Grass, Grate, Button, ...) are seeded with the
 * former hard-coded paths so existing level JSONs keep loading unchanged.
 * Unknown ids or missing files fall back to a plain white texture so
 * rendering/editing never crash on a broken reference.
 *
 * @author: Simon Nguyen
 * @date: 02/02/2025
 */

#pragma once

#include <SFML/Graphics/Texture.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

/// Data-driven registry of scene textures (whole images + tilesets);
/// owned by the Renderer, used by the renderer scene paths and the level
/// editor texture picker
class SceneTextureRegistry
{
  public:
    /// uniform-grid tileset metadata of one registered image
    struct TilesetInfo
    {
        int tileWidth = 32;
        int tileHeight = 32;
        int margin = 0;
        int spacing = 0;
        int columns = 0;
        int rows = 0;
    };

    /// Scans \p scenesDirectory (objects/ + tilesets/ sub folders) and
    /// loads every registered texture; entries whose file is missing stay
    /// registered but render with the fallback texture.
    void load(const std::string& scenesDirectory = "assets/scenes");

    /// texture of \p id (nullptr = unknown id); the fallback texture of a
    /// missing file is obtained via fallbackTexture()
    const sf::Texture* texture(const std::string& id) const;

    /// tileset metadata of \p id (nullptr = \p id is not a tileset)
    const TilesetInfo* tileset(const std::string& id) const;

    /// rendered rect of \p id: the tile cell (\p tileCol/\p tileRow) when
    /// both are >= 0, otherwise the whole image
    sf::IntRect textureRect(const std::string& id, int tileCol,
                            int tileRow) const;

    /// ids for the editor picker, sorted: whole images, then tilesets
    const std::vector<std::string>& imageIds() const { return mImageIds; }
    const std::vector<std::string>& tilesetIds() const
    {
        return mTilesetIds;
    }

    /// plain white texture used when a registered file is missing or the
    /// id is unknown; guarantees rendering/editing never crash
    const sf::Texture& fallbackTexture() const;

  private:
    struct Entry
    {
        std::filesystem::path image;
        sf::Texture texture;
        bool loaded = false;
        TilesetInfo tileset;
    };

    /// registers image \p path under \p id (first registration wins) and
    /// marks it as tileset/whole image
    void addEntry(const std::string& id, const std::filesystem::path& path,
                  bool isTileset);

    /// scans \p dir for pngs (+ json sidecars when \p asTilesets)
    void scanDirectory(const std::filesystem::path& dir, bool asTilesets);

    /// loads the image + tileset metadata of a registered entry; keeps the
    /// entry registered (but marked unloaded) when the file is gone
    void loadEntry(Entry& entry);

    /// legacy enum names seeded with the former hard-coded paths
    void addLegacyEntries(const std::string& scenesDirectory);

    Entry* find(const std::string& id);
    const Entry* find(const std::string& id) const;

    std::map<std::string, Entry> mEntries;
    std::vector<std::string> mImageIds;
    std::vector<std::string> mTilesetIds;
    /// lazily created in fallbackTexture() (const on purpose)
    mutable sf::Texture mFallback;
    mutable bool mFallbackLoaded = false;
};
