#include "Ryu/Core/Renderer.h"
#include "Ryu/Core/AssetIdentifiers.h"
#include "Ryu/Events/EventEnums.h"
#include "Ryu/Events/EventBus.h"
#include "Ryu/Core/Utilities.h"
#include "Ryu/Scene/Entity.h"
#include "Ryu/Scene/SceneEnums.h"
#include "Ryu/Core/EventManager.h"
#include "Ryu/Character/CharacterIchi.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <vector>

namespace
{

bool
bodyIdsEqual(const b2BodyId& lhs, const b2BodyId& rhs)
{
    return lhs.index1 == rhs.index1 && lhs.generation == rhs.generation
           && lhs.world0 == rhs.world0;
}

} // anonymous namespace

Renderer::Renderer(sf::RenderWindow& window, EventManager& eventManager, const std::map<ELevel, std::vector<SceneObjectPhysicsParameters>>& sceneObjects)
     : mPhysicsAssetsManager()
     , mWindow(window)
     , mSceneAssetsManager()
     , mCharacterAssetsManager()
     , mSceneGraph()
     , mWorldBounds(
          {0.f, 0.f},
          {640.f, 360.f})  // Use fixed canvas resolution for world bounds
     , mSpawnPosition(mWorldBounds.size.x / 2.f,
              mWorldBounds.size.y - 50)  // Spawn near bottom (50 units from bottom)
     , mPushBox(nullptr)
     , mHoveredBodyId(b2_nullBodyId)
     , mHoverShowsOutline(false)
     , mSelectedBodyId(b2_nullBodyId)
     , mShowResizePreview(false)
     , mShowGridOverlay(false)
     , mGridCellSizePx({32.f, 32.f})
     , mGridColor(255, 0, 0, 90)
     , mShowCharacterSelection(false)
     , mActiveCommands()
     , mSceneObjects(sceneObjects)
     , mEventManager(eventManager)
 {
     // TODO: add assets to assetmanager like
     // baseTextureManager.load(Textures::PhysicAssetsID::Empty, "assets/scenes/99_dummy/box_empty.png");
     loadTextures();
     buildScene();
     // Scene nodes for the physics scene objects are created exclusively via
     // the ObjectCreated physics events (single source of truth: Physics).

     // Subscribe to physics events
     EventBus::subscribe(Ryu::EPhysicsEvent::ObjectCreated
                         , [this](std::any data)
                         {
                             auto event = std::any_cast<PhysicsObjectCreatedEvent>(data);
                             this->onPhysicsObjectCreated(event);
                         });

     EventBus::subscribe(Ryu::EPhysicsEvent::ObjectUpdated
                         , [this](std::any data)
                         {
                             auto event = std::any_cast<PhysicsObjectUpdatedEvent>(data);
                             this->onPhysicsObjectUpdated(event);
                         });


     EventBus::subscribe(Ryu::EPhysicsEvent::ObjectDestroyed
         , [this](std::any data)
         {
             auto event = std::any_cast<PhysicsObjectDestroyedEvent>(data);
             this->onPhysicsObjectDestroyed(event);
         });

}

void Renderer::loadTextures()
{
    mPhysicsAssetsManager.load(Textures::PhysicAssetsID::Crate,
                            "assets/scenes/99_dummy/box_wood.png");
    
    mSceneAssetsManager.load(Textures::SceneID::BoxPushable,
                        "assets/scenes/99_dummy/box_wood.png");
    mSceneAssetsManager.load(Textures::SceneID::BGMountain,
                        "assets/backgrounds/99_dummy/722756.png");
    mSceneAssetsManager.load(Textures::SceneID::Grass,
                        "assets/scenes/99_dummy/tile_grass_1.png");
    mSceneAssetsManager.load(Textures::SceneID::Button,
                        "assets/scenes/99_dummy/tile_button_1.png");
    mSceneAssetsManager.load(Textures::SceneID::Teleport,
                        "assets/scenes/99_dummy/tile_teleport_1.png");
    mSceneAssetsManager.load(Textures::SceneID::Grate,
                        "assets/scenes/99_dummy/tile_grate_1.png");

}

void Renderer::buildScene() {
    // set Layer
    for (std::size_t i = 0; i < size_t(Layer::LayerCount); ++i) {
        std::shared_ptr<SceneNode> layer = std::make_shared<SceneNode>();
        mSceneLayers[i] = layer;
        mSceneGraph.attachChild(std::move(layer));
    }
    
    // TODO: background is also scene-dependent
    // move to where all the sceneobjects of a scene will be located
    sf::Texture &textureBg =
        mSceneAssetsManager.getResource(Textures::SceneID::BGMountain);
    sf::IntRect textureRect(sf::Vector2i(0, 0), sf::Vector2i(static_cast<int>(mWorldBounds.size.x), static_cast<int>(mWorldBounds.size.y)));

    std::unique_ptr<SpriteNode> backgroundSprite =
        std::make_unique<SpriteNode>(textureBg, textureRect);
    backgroundSprite->setPosition({mWorldBounds.position.x, mWorldBounds.position.y});
    mSceneLayers[static_cast<unsigned>(Layer::Background)]->attachChild(
        std::move(backgroundSprite));

    // pushable Box / moving platform test
    std::unique_ptr<Box> box =
        std::make_unique<Box>(Box::Type::Pushable, mSceneAssetsManager);
    mPushBox = box.get();
    mPushBox->setPosition(sf::Vector2f(760.f,80.f));
    
    mSceneLayers[static_cast<size_t>(Layer::Ground1)]->attachChild(
        std::move(box));

    auto player = mEventManager.requestPlayer();

    if (player) {
        mPlayerNode = std::static_pointer_cast<SceneNode>(player);
        mSceneLayers[static_cast<size_t>(Layer::Ground1)]->attachChild(
            mPlayerNode);
    } else {
        std::cerr << "WARNING: Player is null in Renderer::buildScene()" << std::endl;
    }
}

void
Renderer::setPlayerOnTop(bool onTop)
{
    if (not mPlayerNode) { return; }

    const auto attachLayer = onTop ? Layer::Foreground : Layer::Ground1;
    auto owner = std::find_if(mSceneLayers.begin(), mSceneLayers.end(),
        [this](const auto &layer) {
            return layer && layer.get() == mPlayerNode->getParent();
        });

    // already in the requested layer (detachChild asserts otherwise)
    if (owner != mSceneLayers.end()
        && owner->get()
             == mSceneLayers[static_cast<size_t>(attachLayer)].get())
    {
        return;
    }
    if (owner != mSceneLayers.end())
    {
        (*owner)->detachChild(*mPlayerNode);
    }
    mSceneLayers[static_cast<size_t>(attachLayer)]->attachChild(mPlayerNode);
}

void Renderer::draw()
{
    mWindow.draw(mSceneGraph);
    drawGridOverlay();
    drawLevelEditorOverlay();
}

void
Renderer::setCharacterSelection(const sf::Vector2f& centerPx,
                                const sf::Vector2f& sizePx, bool show)
{
    mCharacterSelectionCenterPx = centerPx;
    mCharacterSelectionSizePx = sizePx;
    mShowCharacterSelection = show;
}

void
Renderer::setGridOverlay(bool show, const sf::Vector2f& cellSizePx,
                         const sf::Color& color)
{
    mShowGridOverlay = show;
    mGridCellSizePx = cellSizePx;
    mGridColor = color;
}

void
Renderer::drawGridOverlay()
{
    if (not mShowGridOverlay) { return; }

    const float cellW = mGridCellSizePx.x;
    const float cellH = mGridCellSizePx.y;
    if (cellW < 4.f || cellH < 4.f) { return; }

    // vertical + horizontal lines over the whole world bounds, drawn after
    // the scene graph so the grid stays visible in the foreground
    std::vector<sf::Vertex> lines;
    lines.reserve(static_cast<std::size_t>((mWorldBounds.size.x / cellW + 1)
                                           + (mWorldBounds.size.y / cellH + 1)) * 2);

    for (float x = mWorldBounds.position.x; x <= mWorldBounds.position.x + mWorldBounds.size.x + 0.5f; x += cellW)
    {
        lines.push_back(sf::Vertex{{x, mWorldBounds.position.y}, mGridColor});
        lines.push_back(sf::Vertex{{x, mWorldBounds.position.y + mWorldBounds.size.y}, mGridColor});
    }
    for (float y = mWorldBounds.position.y; y <= mWorldBounds.position.y + mWorldBounds.size.y + 0.5f; y += cellH)
    {
        lines.push_back(sf::Vertex{{mWorldBounds.position.x, y}, mGridColor});
        lines.push_back(sf::Vertex{{mWorldBounds.position.x + mWorldBounds.size.x, y}, mGridColor});
    }
    mWindow.draw(lines.data(), lines.size(), sf::PrimitiveType::Lines);
}

b2BodyId
Renderer::getSceneObjectAt(ELevel level, const b2Vec2& position) const
{
    if (not mSceneObjects.contains(level)) { return b2_nullBodyId; }

    for (const auto& object : mSceneObjects.at(level))
    {
        if (B2_IS_NULL(object.mPhysicsBodyId)) { continue; }

        // object rects use center positions; stored in world meters,
        // hit-test happens in pixels
        const sf::Vector2f centerPx{
            Converter::metersToPixels(object.mPosition.x),
            Converter::metersToPixels(object.mPosition.y)};
        const sf::Vector2f sizePx{
            Converter::metersToPixels(object.mSize.x),
            Converter::metersToPixels(object.mSize.y)};
        const sf::FloatRect rect{centerPx - sizePx / 2.f, sizePx};

        if (rect.contains({position.x, position.y}))
        {
            return object.mPhysicsBodyId;
        }
    }
    return b2_nullBodyId;
}

void
Renderer::setHoverHighlight(b2BodyId bodyId, bool withOutline)
{
    mHoveredBodyId = bodyId;
    mHoverShowsOutline = withOutline;
}

void
Renderer::setSelectedHighlight(b2BodyId bodyId)
{
    mSelectedBodyId = bodyId;
}

void
Renderer::setResizePreview(const sf::Vector2f& positionPx,
                           const sf::Vector2f& sizePx, bool show)
{
    mResizePreviewPositionPx = positionPx;
    mResizePreviewSizePx = sizePx;
    mShowResizePreview = show;
}

void
Renderer::drawLevelEditorOverlay()
{
    constexpr float overlayAlpha = 180.f; // 70% opacity
    const sf::Color hoverFill{255, 140, 0,
                              static_cast<std::uint8_t>(overlayAlpha)};
    const sf::Color outline = sf::Color::Red;
    const float outlineThickness = 1.f;

    sf::RectangleShape rect;

    // live-resize preview of the object currently being resized
    if (mShowResizePreview)
    {
        rect.setPosition(mResizePreviewPositionPx - mResizePreviewSizePx / 2.f);
        rect.setSize(mResizePreviewSizePx);
        rect.setFillColor(hoverFill);
        rect.setOutlineColor(outline);
        rect.setOutlineThickness(outlineThickness);
        mWindow.draw(rect);
    }

    // 8 resize handles (4 corners + edge midpoints) around the object rect
    auto drawEdgeMarkers = [&](const sf::Vector2f& centerPx,
                               const sf::Vector2f& sizePx)
    {
        constexpr float handleSize = 4.f;
        const sf::Vector2f half = sizePx / 2.f;
        const sf::Vector2f handlePoints[8] = {
            {-half.x, -half.y}, {0.f, -half.y}, {half.x, -half.y},
            {-half.x, 0.f}, {half.x, 0.f},
            {-half.x, half.y}, {0.f, half.y}, {half.x, half.y},
        };
        rect.setFillColor(outline);
        rect.setOutlineThickness(0.f);
        for (const auto& point : handlePoints)
        {
            rect.setPosition(centerPx + point
                             - sf::Vector2f{handleSize, handleSize} / 2.f);
            rect.setSize({handleSize, handleSize});
            mWindow.draw(rect);
        }
    };

    auto drawHoverHighlight = [&](b2BodyId bodyId)
    {
        if (B2_IS_NULL(bodyId)) { return; }
        for (const auto& [level, objects] : mSceneObjects)
        {
            for (const auto& object : objects)
            {
                if (not bodyIdsEqual(object.mPhysicsBodyId, bodyId)) { continue; }

                const sf::Vector2f centerPx{
                    Converter::metersToPixels(object.mPosition.x),
                    Converter::metersToPixels(object.mPosition.y)};
                const sf::Vector2f sizePx{
                    Converter::metersToPixels(object.mSize.x),
                    Converter::metersToPixels(object.mSize.y)};

                rect.setPosition(centerPx - sizePx / 2.f);
                rect.setSize(sizePx);
                rect.setFillColor(hoverFill);
                rect.setOutlineColor(mHoverShowsOutline ? outline
                                                        : sf::Color::Transparent);
                rect.setOutlineThickness(mHoverShowsOutline ? outlineThickness
                                                            : 0.f);
                mWindow.draw(rect);
                if (mHoverShowsOutline) { drawEdgeMarkers(centerPx, sizePx); }
                return;
            }
        }
    };

    auto drawSelectionHighlight = [&](b2BodyId bodyId)
    {
        if (B2_IS_NULL(bodyId)) { return; }
        for (const auto& [level, objects] : mSceneObjects)
        {
            for (const auto& object : objects)
            {
                if (not bodyIdsEqual(object.mPhysicsBodyId, bodyId)) { continue; }

                const sf::Vector2f centerPx{
                    Converter::metersToPixels(object.mPosition.x),
                    Converter::metersToPixels(object.mPosition.y)};
                const sf::Vector2f sizePx{
                    Converter::metersToPixels(object.mSize.x),
                    Converter::metersToPixels(object.mSize.y)};

                rect.setPosition(centerPx - sizePx / 2.f);
                rect.setSize(sizePx);
                rect.setFillColor(hoverFill);
                rect.setOutlineColor(outline);
                rect.setOutlineThickness(outlineThickness);
                mWindow.draw(rect);

                drawEdgeMarkers(centerPx, sizePx);
                return;
            }
        }
    };

    // hover tint from the level-object list (fill only, no outline)
    // main-character selection (level editor): orange fill + red outline,
    // drawn after the scene graph so it stays in the foreground
    if (mShowCharacterSelection)
    {
        rect.setPosition(mCharacterSelectionCenterPx
                         - mCharacterSelectionSizePx / 2.f);
        rect.setSize(mCharacterSelectionSizePx);
        rect.setFillColor(hoverFill);
        rect.setOutlineColor(outline);
        rect.setOutlineThickness(outlineThickness);
        mWindow.draw(rect);
    }

    if (not bodyIdsEqual(mHoveredBodyId, mSelectedBodyId)
        && not B2_IS_NULL(mHoveredBodyId))
    {
        drawHoverHighlight(mHoveredBodyId);
    }

    // selection mode visuals (fill + red outline + handles) only while the
    // Select tool is on
    if (not B2_IS_NULL(mSelectedBodyId))
    {
        drawSelectionHighlight(mSelectedBodyId);
    }
}

void Renderer::update(sf::Time dt)
{
    while (!mActiveCommands.isEmpty()) {
        mSceneGraph.onCommand(mActiveCommands.pop(), dt);
    }
    mSceneGraph.update(dt);
}

CommandQueue& Renderer::getActiveCommands()
{
    return mActiveCommands;
}


void
Renderer::onPhysicsObjectCreated(const PhysicsObjectCreatedEvent& event)
{
    // Create a SpriteNode for the physics object
    sf::Texture* texture = nullptr;
    
    if (auto* physicsTextureId = std::get_if<Textures::PhysicAssetsID>(&event.textureId))
    {
        if (*physicsTextureId != Textures::PhysicAssetsID::Empty)
        {
            texture = &mPhysicsAssetsManager.getResource(*physicsTextureId);
        }
        else
        {
            texture = &mPhysicsAssetsManager.getResource(Textures::PhysicAssetsID::Empty);
        }
    }
    else if (auto* sceneTextureId = std::get_if<Textures::SceneID>(&event.textureId))
    {
        texture = &mSceneAssetsManager.getResource(*sceneTextureId);
    }
    else if(auto* spriteSheetTextureId = std::get_if<Textures::SpritesheetID>(&event.textureId))
    {
        texture = &mCharacterAssetsManager.getResource(*spriteSheetTextureId);
    }

    if (texture)
    {
        // Convert size from meters (Box2D) to pixels (SFML)
        sf::Vector2f sizePixels{
            Converter::metersToPixels(event.size.x),
            Converter::metersToPixels(event.size.y)
        };
        
        auto spriteNode = std::make_unique<SpriteNode>(*texture);
        
        // Scale the sprite to match the physics object size
        sf::Vector2f textureSize(static_cast<float>(texture->getSize().x), 
                                static_cast<float>(texture->getSize().y));
        sf::Vector2f scale(sizePixels.x / textureSize.x, sizePixels.y / textureSize.y);
        spriteNode->setScale(scale);
        
        // Set origin to center for proper positioning
        spriteNode->setOrigin(textureSize / 2.0f);
        
        // Convert position from meters (Box2D) to pixels (SFML)
        spriteNode->setPosition(sf::Vector2f{
            Converter::metersToPixels(event.position.x),
            Converter::metersToPixels(event.position.y)
        });
        
        // Attach to the layer specified in the scene object configuration
        auto& layerNode = mSceneLayers[static_cast<size_t>(
            std::clamp(event.layer,
                       static_cast<int>(Layer::Background),
                       static_cast<int>(Layer::LayerCount) - 1))];
        auto nodeShared = layerNode->attachChildReturningPtr(std::move(spriteNode));
        
        // Store the mapping from bodyId to the sprite node for updates
        mPhysicsBodyToNode[event.bodyId] = nodeShared;
    }
}

void
Renderer::onPhysicsObjectUpdated(const PhysicsObjectUpdatedEvent& event)
{
    // Find the sprite node for this physics body
    auto it = mPhysicsBodyToNode.find(event.bodyId);
    if (it != mPhysicsBodyToNode.end() && !it->second.expired())
    {
        auto node = it->second.lock();
        if (node)
        {
            // Convert position from meters (Box2D) to pixels (SFML)
            node->setPosition(sf::Vector2f{
                Converter::metersToPixels(event.position.x),
                Converter::metersToPixels(event.position.y)
            });
            node->setRotation(sf::radians(event.rotation));
        }
    }
}

void
Renderer::onPhysicsObjectDestroyed(const PhysicsObjectDestroyedEvent& event)
{
    // Remove the destroyed physics object from the mapping and detach its
    // sprite node from the scene graph
    auto it = mPhysicsBodyToNode.find(event.bodyId);
    if (it != mPhysicsBodyToNode.end() && !it->second.expired())
    {
        if (auto node = it->second.lock())
        {
            // detach from the node's actual parent layer (detachChild asserts
            // when the node is not a child of the node it is called on)
            auto owner = std::find_if(mSceneLayers.begin(), mSceneLayers.end(),
                [&node](const auto &layer) {
                    return layer && layer.get() == node->getParent();
                });
            if (owner != mSceneLayers.end()) { (*owner)->detachChild(*node); }
        }
    }
    mPhysicsBodyToNode.erase(event.bodyId);

    // The node itself is destroyed by the scenegraph (detachChild owns it).
    // Note: With the scenegraph approach, we don't need to manually destroy
    // objects.
}

void Renderer::createSceneFromConfiguration(ELevel level)
{
    for (auto& obj : mSceneObjects.at(level))
    {
        const sf::Texture& texture = mSceneAssetsManager.getResource(obj.mTextureId);
        
        // Convert size from meters (Box2D) to pixels (SFML)
        sf::Vector2f sizePixels{
            Converter::metersToPixels(obj.mSize.x),
            Converter::metersToPixels(obj.mSize.y)
        };
        
        // Create SpriteNode with texture and scale it to the desired size
        auto spriteNode = std::make_unique<SpriteNode>(texture);
        
        // Scale the sprite to match the physics object size
        sf::Vector2f textureSize(static_cast<float>(texture.getSize().x), 
                                static_cast<float>(texture.getSize().y));
        sf::Vector2f scale(sizePixels.x / textureSize.x, sizePixels.y / textureSize.y);
        spriteNode->setScale(scale);
        
        // Set origin to center for proper positioning
        spriteNode->setOrigin(textureSize / 2.0f);
        
        // Convert position from meters (Box2D) to pixels (SFML)
        spriteNode->setPosition(sf::Vector2f{
            Converter::metersToPixels(obj.mPosition.x),
            Converter::metersToPixels(obj.mPosition.y)
        });
        
        // Use the layer specified in the scene object configuration
        mSceneLayers[static_cast<size_t>(obj.mLayer)]->attachChild(std::move(spriteNode));
    }
}
