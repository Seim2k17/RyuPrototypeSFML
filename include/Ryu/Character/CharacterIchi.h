#pragma once

#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Physics/Raycast.h>
#include <Ryu/Physics/RaycastTypes.h>

#include <box2d/b2_math.h>

// namespace ryu {

using IchiTextureManager = AssetManager<sf::Texture, Textures::SpritesheetID>;

class CharacterIchi : public CharacterBase {
  public:
    CharacterIchi(ECharacterState startState,
                  const sf::Vector2i &position);
    void setTextureOnCharacter(Textures::SpritesheetID textureId) override;

    // TODO: make it private and load otherwise ? / atm. public for Game when
    // creating the mPlayer in Cdor
    void loadTextures();
    void moveCharacter(sf::Vector2f velocity);
    unsigned int getCategory() const override;
    void update(sf::Time deltaTime) override;
    void onNotify(const SceneNode &entity, Ryu::EEvent event) override;
    void checkTeleportContact(std::string name);
    void teleportCharacter(float x, float y);

  private:
    void drawCurrent(sf::RenderTarget &target,
                     sf::RenderStates states) const override;
    void allowedMovement(bool &xMove, bool &yMove);
    IchiTextureManager ichiTextureManager;
};

//} /// namespace ryu
