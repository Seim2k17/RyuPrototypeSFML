#include <Ryu/Debug/b2DrawSFML.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/StencilMode.hpp>
#include <fmt/core.h>
#include <cstdint>
#include <iostream>


/// Draw a closed polygon provided in CCW order.
void b2DrawSFML::DrawPolygonFcn( const b2Vec2* vertices, int vertexCount, b2HexColor color, void* context )
{
    b2DrawSFML* contextObj = static_cast<b2DrawSFML*>(context);
    contextObj->DrawPolygon(vertices, vertexCount, color);
}

/// Draw a solid closed polygon provided in CCW order.
void b2DrawSFML::DrawSolidPolygonFcn ( b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color,
								void* context )
{
    b2DrawSFML* contextObj = static_cast<b2DrawSFML*>(context);
    contextObj->DrawSolidPolygon(transform, vertices, vertexCount, radius, color);
}

/// Draw a circle.
void b2DrawSFML::DrawCircleFcn ( b2Vec2 center, float radius, b2HexColor color, void* context )
{
    b2DrawSFML* contextObj = static_cast<b2DrawSFML*>(context);
    contextObj->DrawCircle(center, radius, color);
}

/// Draw a solid circle.
void b2DrawSFML::DrawSolidCircleFcn( b2Transform transform, float radius, b2HexColor color, void* context )
{
    b2DrawSFML* contextObj = static_cast<b2DrawSFML*>(context);
    contextObj->DrawSolidCircle(transform, radius, color);
}

/// Draw a solid capsule.
void b2DrawSFML::DrawSolidCapsuleFcn( b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context )
{
    b2DrawSFML* contextObj = static_cast<b2DrawSFML*>(context);
    contextObj->DrawSolidCapsule(p1, p2, radius, color);
}

/// Draw a line segment.
void b2DrawSFML::DrawSegmentFcn( b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context )
{
    b2DrawSFML* contextObj = static_cast<b2DrawSFML*>(context);
    contextObj->DrawSegment(p1, p2, color);
}

/// Draw a transform. Choose your own length scale.
void b2DrawSFML::DrawTransformFcn( b2Transform transform, void* context )
{
    b2DrawSFML* contextObj = static_cast<b2DrawSFML*>(context);
    contextObj->DrawTransform(transform);
}

/// Draw a point.
void b2DrawSFML::DrawPointFcn ( b2Vec2 p, float size, b2HexColor color, void* context )
{
    b2DrawSFML* contextObj = static_cast<b2DrawSFML*>(context);
    contextObj->DrawPoint(p, size, color);
}

/// Draw a string in world space
void b2DrawSFML::DrawStringFcn( b2Vec2 p, const char* s, b2HexColor color, void* context )
{
    b2DrawSFML* contextObj = static_cast<b2DrawSFML*>(context);
    contextObj->DrawString(p, s, color);
}


b2DrawSFML::b2DrawSFML(float scale) noexcept
    : m_renderTarget { nullptr }
    , m_convexShape {}
    , m_circleShape {}
    , m_scale { scale }
{
   initialize_debug_draw(); 
}

void
b2DrawSFML::initialize_debug_draw()
{
    debugDraw.context = this;
    debugDraw.DrawPolygonFcn = DrawPolygonFcn;
    debugDraw.DrawSolidPolygonFcn = DrawSolidPolygonFcn;
    debugDraw.DrawCircleFcn = DrawCircleFcn;
    debugDraw.DrawSolidCircleFcn = DrawSolidCircleFcn;
    debugDraw.DrawSolidCapsuleFcn = DrawSolidCapsuleFcn;
    debugDraw.DrawSegmentFcn = DrawSegmentFcn;
    debugDraw.DrawTransformFcn = DrawTransformFcn;
    debugDraw.DrawPointFcn = DrawPointFcn;
    debugDraw.DrawStringFcn = DrawStringFcn;
}

b2DrawSFML::b2DrawSFML(sf::RenderTarget& renderTarget, float scale) noexcept
    : m_renderTarget { &renderTarget }
    , m_convexShape {}
    , m_circleShape {}
    , m_scale { scale }
{
}

b2DrawSFML::~b2DrawSFML()
{
}

const b2DebugDraw& b2DrawSFML::GetDebugDraw() const noexcept { return debugDraw; }
        
b2DebugDraw& b2DrawSFML::GetDebugDraw() noexcept { return debugDraw; }

void b2DrawSFML::SetTarget(sf::RenderTarget& renderTarget)
{
    m_renderTarget = &renderTarget;
}

void b2DrawSFML::SetAllFlags() noexcept
{
    fmt::print("SetFlags\n");
    debugDraw.drawShapes = true;    // ✅ Bodies (including boxes)
    debugDraw.drawJoints = true;    // ✅ Joints
    debugDraw.drawBounds = true;     // ✅ Bounding boxes (add this!)
    debugDraw.drawBodyNames = true;     // ✅ Bounding boxes (add this!)
    debugDraw.drawMass = true;     // ✅ Center of mass
    debugDraw.drawContacts = true;  // ✅ Contact points
    // debugDraw.drawCOMs = true;      // ✅ Center of mass markers
   // TODO: map following bits to bool-member
   /*
    SetFlags(
        b2DebugDraw::e_shapeBit |
        b2DebugDraw::e_jointBit |
        b2DebugDraw::e_aabbBit |
        b2DebugDraw::e_pairBit |
        b2DebugDraw::e_centerOfMassBit
    );
    */
}

void b2DrawSFML::SetRenderTarget(sf::RenderTarget& renderTarget) noexcept
{
    m_renderTarget = &renderTarget;
}

sf::RenderTarget& b2DrawSFML::GetRenderTarget() noexcept
{
    return *m_renderTarget;
}

void b2DrawSFML::SetScale(float scale) noexcept
{
    m_scale = scale;
}

float b2DrawSFML::GetScale() noexcept
{
    return m_scale;
}

void b2DrawSFML::DrawPolygon(const b2Vec2* vertices, int vertexCount, b2HexColor color) noexcept
{
    //std::cout << "DrawPolygon" << "\n";
    auto const count = static_cast<size_t>(vertexCount);

    m_convexShape.setPointCount(count);

    for (size_t i = 0; i < count; ++i)
    {
        m_convexShape.setPoint(i, M_ToPixels(vertices[i]));
    }

    m_convexShape.setFillColor(sf::Color::Blue);
    m_convexShape.setOutlineColor(M_ConvertColor(color));
    m_convexShape.setOutlineThickness(1.0f);

/*
    m_convexShape.setFillColor(sf::Color::Transparent);
    m_convexShape.setOutlineColor(M_ConvertColor(color));
    m_convexShape.setOutlineThickness(-1.0f);
*/

    m_renderTarget->draw(m_convexShape);
}

void b2DrawSFML::DrawSolidPolygon(b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color) noexcept
{
    //std::cout << "DrawSolidPolygon" << "\n";
    auto const count = static_cast<size_t>(vertexCount);

    m_convexShape.setPointCount(count);

    for (size_t i = 0; i < count; ++i)
    {
        m_convexShape.setPoint(i, M_ToPixels(vertices[i]));
    }

    m_convexShape.setFillColor(M_ConvertColor(color, 0.25f));/// 2nd param: alpha
    //m_convexShape.setOutlineColor(sf::Color::Transparent);

//    if()
//    {
        m_convexShape.setOutlineColor(sf::Color::Green);
//    }
//    else
    {
//        m_convexShape.setOutlineColor(sf::Color::Red);
    }

    m_convexShape.setOutlineThickness(2.0f);

    m_renderTarget->draw(m_convexShape);
}

void b2DrawSFML::DrawCircle(b2Vec2 center, float radius, b2HexColor color) noexcept
{
    m_circleShape.setRadius(M_ToPixels(radius));
    m_circleShape.setPosition(M_ToPixels(center));
    m_circleShape.setOrigin(M_ToPixels({ radius, radius }));
    m_circleShape.setFillColor(sf::Color::Transparent);
    m_circleShape.setOutlineColor(M_ConvertColor(color));
    m_circleShape.setOutlineThickness(-1.0f);

    m_renderTarget->draw(m_circleShape);
}

void b2DrawSFML::DrawSolidCircle(b2Transform transform, float radius, b2HexColor color) noexcept
{
    m_circleShape.setRadius(M_ToPixels(radius));
    m_circleShape.setPosition(M_ToPixels({transform.p.x, transform.p.y}));
    m_circleShape.setOrigin(M_ToPixels({ radius, radius }));
    m_circleShape.setFillColor(M_ConvertColor(color, 0.8f));
    m_circleShape.setOutlineColor(sf::Color::Transparent);
    m_circleShape.setOutlineThickness(0.0f);

    m_renderTarget->draw(m_circleShape);
}

//TODO: implement
void b2DrawSFML::DrawSolidCapsule(b2Vec2 /*p1*/, b2Vec2 /*p2*/, float /*radius*/, b2HexColor /*color*/) noexcept {}
void b2DrawSFML::DrawString( b2Vec2 /*p*/, const char* /*s*/, b2HexColor /*color*/) noexcept {}

void b2DrawSFML::DrawSegment(b2Vec2 p1, b2Vec2 p2, b2HexColor color) noexcept
{
    auto const convertedColor = M_ConvertColor(color);

    sf::Vertex const line[] = {
        { M_ToPixels(p1), convertedColor },
        { M_ToPixels(p2), convertedColor }
    };

    m_renderTarget->draw(line, 2u, sf::PrimitiveType::Lines);
}


void b2DrawSFML::DrawTransform(b2Transform xf) noexcept
{
    auto const axisScale = 0.4f;

    // TODO:  check how to draw transform in box2d 3.1.
    //DrawSegment(xf.p, xf.p + axisScale * xf.q, b2HexColor{ 1.0f, 0.0f, 0.0f });
    //DrawSegment(xf.p, xf.p + axisScale * xf.q, b2HexColor{ 0.0f, 1.0f, 0.0f });
}

void b2DrawSFML::DrawPoint(b2Vec2 p, float size, b2HexColor color) noexcept
{
    // TODO: check how to draw a point with transform
    //DrawSolidCircle(p, size, color);
}

float b2DrawSFML::M_ToPixels(float f) const noexcept
{
    return f * m_scale;
}

sf::Vector2f b2DrawSFML::M_ToPixels(b2Vec2 const& p) const noexcept
{
    return { p.x * m_scale, p.y * m_scale };
}

sf::Color b2DrawSFML::M_ConvertColor(b2HexColor const& color) const noexcept 
{
    return M_ConvertColor(color, 255.0f); // Full opacity by default
}

sf::Color b2DrawSFML::M_ConvertColor(b2HexColor const& color, float newAlpha) const noexcept
{
    // Extract RGB from the 32-bit hex value
    uint32_t hex = static_cast<uint32_t>(color);

    return {
        static_cast<std::uint8_t>((hex >> 16) & 0xFF), // Red (bits 16-23)
        static_cast<std::uint8_t>((hex >> 8)  & 0xFF), // Green (bits 8-15)
        static_cast<std::uint8_t>(hex & 0xFF),        // Blue (bits 0-7)
        static_cast<std::uint8_t>(newAlpha)           // Alpha
    };
}

