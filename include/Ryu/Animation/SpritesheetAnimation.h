#pragma once

#include <Ryu/Animation/AnimationData.h>
#include <Ryu/Animation/EditorEnums.h>
#include <Ryu/Control/CharacterEnums.h>
#include <Ryu/Events/EventEnums.h>
#include <Ryu/Events/Subject.h>

#include <SFML/Graphics.hpp>
#include <memory>

class CharacterBase;

//using baseCharPtr = std::shared_ptr<CharacterBase>;
using baseCharPtr = CharacterBase*;

struct AnimationFrame {
    float duration; // in seconds
    sf::Vector2i frameSize;
    Ryu::EEvent event;
};

class SpritesheetAnimation : public sf::Drawable,
                             public sf::Transformable,
                             public Subject {
  public:
    SpritesheetAnimation();
    SpritesheetAnimation(baseCharPtr owner);
    explicit SpritesheetAnimation(const sf::Texture &texture,
                                  baseCharPtr owner);

    void setTexture(const sf::Texture &texture);
    const sf::Texture *getTexture() const;

    void setFrameSize(sf::Vector2i mFrameSize);
    sf::Vector2i getFrameSize() const;

    /* TODO setNumFrames(numframes) == old method without the use of the
     * AnimationEditor (later delete it) */
    void setNumFrames(std::size_t numFrames);
    void setNumFrames(sf::Time aniDuration,
                      std::vector<RyuParser::Frame> &aniFrames);
    void setNumFrames(sf::Time aniDuration,
                      std::vector<RyuParser::FrameEditor> &aniFrames);
    std::size_t getNumFrames() const;
    std::size_t getFramesCount() const { return mFrames.size(); }

    void setStartFrame(sf::Vector2i startFrame);

    void setDuration(sf::Time duration);
    sf::Time getDuration() const;

    void setRepeating(bool flag);
    bool isRepeating() const;

    void restart();
    bool isFinished() const;

    void flipAnimationLeft();
    void flipAnimationRight();

    sf::FloatRect getLocalBounds() const;
    sf::FloatRect getGlobalBounds() const;
    int getCurrentFrame() const;
    void update(sf::Time dt);
    void setAnimationName(std::string name);
    sf::Sprite &getSprite() { return mSprite; }
    const sf::Sprite &getSprite() const { return mSprite; }
    sf::Vector2f getPivotNorm();
    void setPivotAbs(sf::Vector2i vec);
    void setPivotNorm(sf::Vector2f vec);
    void setAnimationPosition(sf::Vector2f posi);

    sf::Vector2i getPivotAbs();

  private:
    void draw(sf::RenderTarget &target, sf::RenderStates states) const;

  private:
    sf::Sprite mSprite;
    sf::Vector2i mFrameSize;
    sf::Vector2i mStartFrame;
    std::size_t mNumFrames;
    std::size_t mCurrentFrame;
    sf::Time mDuration;
    sf::Time mElapsedTime;
    bool mRepeat;
    bool lockPositioning = false;
    unsigned int maxTextureSize;
    sf::Vector2f mPivotNorm;
    sf::Vector2i mPivotAbs;
    // std::vector<AnimationFrame> mFrames;
    std::vector<RyuParser::Frame> mFrames;
    std::string animationIdName;
    baseCharPtr mOwner;
};
