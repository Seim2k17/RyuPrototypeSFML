#pragma once

// SFML
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

// Box2D
#include <box2d/types.h>

///
class b2DrawSFML
{
public:

    ///
    explicit b2DrawSFML(float scale = 1.0f) noexcept;

    ///
    explicit b2DrawSFML(sf::RenderTarget& renderTarget, float scale = 1.0f) noexcept;

    ~b2DrawSFML();

    ///
    void SetAllFlags() noexcept;

    void SetTarget(sf::RenderTarget& renderTarget);

    ///
    void SetRenderTarget(sf::RenderTarget& renderTarget) noexcept;

    const b2DebugDraw& GetDebugDraw() const noexcept;
        
    b2DebugDraw& GetDebugDraw() noexcept;

    ///
    sf::RenderTarget& GetRenderTarget() noexcept;

    ///
    void SetScale(float scale) noexcept;

    ///
    float GetScale() noexcept;

    ///
    void DrawPolygon(const b2Vec2* vertices, int vertexCount, b2HexColor color) noexcept;

    ///
    void DrawSolidPolygon(b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color) noexcept;

    ///
    void DrawCircle(b2Vec2 center, float radius, b2HexColor color) noexcept;

    ///
    void DrawSolidCircle(b2Transform transform, float radius, b2HexColor color) noexcept;

    void DrawSolidCapsule(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color) noexcept;

    ///
    void DrawSegment(b2Vec2 p1, b2Vec2 p2, b2HexColor color) noexcept;

    ///
    void DrawTransform(b2Transform transform) noexcept;

    ///
    void DrawPoint(b2Vec2 p, float size, b2HexColor color) noexcept;
        
    void DrawString( b2Vec2 p, const char* s, b2HexColor color) noexcept;

    void setContact(bool contacted){contact = contacted;};

private:

    void initialize_debug_draw();

    // Static callback wrappers (to pass member functions as C-style callbacks)
    static void DrawPolygonFcn( const b2Vec2* vertices, int vertexCount, b2HexColor color, void* context );
    
    static void DrawSolidPolygonFcn ( b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color, void* context );

    static void DrawCircleFcn ( b2Vec2 center, float radius, b2HexColor color, void* context );

    static void DrawSolidCircleFcn( b2Transform transform, float radius, b2HexColor color, void* context );

    static void DrawSolidCapsuleFcn( b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context );

    static void DrawSegmentFcn( b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context );

    static void DrawTransformFcn( b2Transform transform, void* context );

    static void DrawPointFcn ( b2Vec2 p, float size, b2HexColor color, void* context );

    static void DrawStringFcn( b2Vec2 p, const char* s, b2HexColor color, void* context );

        
    /// PRIV:
    float M_ToPixels(float f) const noexcept;

    /// PRIV:
    sf::Vector2f M_ToPixels(b2Vec2 const& p) const noexcept;

    /// PRIV:
    sf::Color M_ConvertColor(b2HexColor const& color) const noexcept;

    /// PRIV:
    sf::Color M_ConvertColor(b2HexColor const& color, float newAlpha) const noexcept;

    sf::RenderTarget* m_renderTarget;
    sf::ConvexShape m_convexShape;
    sf::CircleShape m_circleShape;
    float m_scale;

    bool contact;

    b2DebugDraw debugDraw;
};


