#pragma once
#include <Ryu/Scene/Entity.h>
#include <Ryu/Core/AssetIdentifiers.h>

//namespace ryu{

class Box : public Entity
{
    
    public:
        enum class Type
        {
            Pushable,
            Heavy
        };
    
    public:
        //explicit Box(Type type);
        Box(Box::Type type, const SceneTextureHolder& textures); 
        void drawCurrent(sf::RenderTarget& target, sf::RenderStates states) const;
        unsigned int getCategory() const override;
        void updateCurrent(sf::Time dt) override;    
        
    private:
        Type mType;
        sf::Sprite mSprite;

};

//} /// namespace ryu
