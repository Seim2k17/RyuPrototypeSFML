#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Control/PlayerController.h>
#include <Ryu/Events/EventEnums.h>
#include <Ryu/Animation/AnimationData.h>
#include <Ryu/Animation/EditorEnums.h>
#include <Ryu/Animation/SpritesheetAnimation.h>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <SFML/System/Time.hpp>

#include <SFML/System/Vector2.hpp>
#include <fmt/core.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

// TODO: check to do better
// since SFML v3 we need a texture for a sprite
sf::Texture dummyTexture;

SpritesheetAnimation::SpritesheetAnimation()
    : mSprite(dummyTexture), mFrameSize(), mNumFrames(0), mCurrentFrame(0),
      mDuration(sf::Time::Zero), mElapsedTime(sf::Time::Zero), mRepeat(false), maxTextureSize(sf::Texture::getMaximumSize())
    , animationIdName(""),
      mStartFrame({0, 0}), mFrames({}), mOwner(nullptr)
    , mPivotAbs({}), mPivotNorm({})
{
    fmt::print("Maximum TextureSize: {}\n", maxTextureSize);
}

SpritesheetAnimation::SpritesheetAnimation(baseCharPtr owner)
    : mSprite(dummyTexture), mFrameSize(), mNumFrames(0), mCurrentFrame(0),
      mDuration(sf::Time::Zero), mElapsedTime(sf::Time::Zero), mRepeat(false),maxTextureSize(sf::Texture::getMaximumSize())
    , animationIdName(""),
      mStartFrame({0, 0}), mFrames({}), mOwner(owner)
    , mPivotAbs({}), mPivotNorm({})
{
    fmt::print("Maximum TextureSize: {}\n", maxTextureSize);
}

SpritesheetAnimation::SpritesheetAnimation(const sf::Texture &texture,
                                           baseCharPtr owner)
    : mSprite(texture), mFrameSize(), mNumFrames(0), mCurrentFrame(0),
      mDuration(sf::Time::Zero), mElapsedTime(sf::Time::Zero), mRepeat(false),maxTextureSize(sf::Texture::getMaximumSize())
    , animationIdName(""),
      mStartFrame({0, 0}), mFrames({}), mOwner(owner)
    , mPivotAbs({}), mPivotNorm({})
{ // set origin of texture to center
    sf::FloatRect bounds = mSprite.getLocalBounds();
    mSprite.setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f});
    fmt::print("Maximum TextureSize: {}\n", maxTextureSize);
    //std::cout << "Boundswidth: " << bounds.width
    //          << "Boundsheight: " << bounds.height << "\n";
    // mSprite.setOrigin(120.f, 144.f);
}

void SpritesheetAnimation::setTexture(const sf::Texture &texture) {
    mSprite.setTexture(texture);
}

const sf::Texture *SpritesheetAnimation::getTexture() const {
    return &mSprite.getTexture();
}

void SpritesheetAnimation::setFrameSize(sf::Vector2i frameSize) {
    mFrameSize = frameSize;
}

void SpritesheetAnimation::setAnimationName(std::string name)
{
    animationIdName = name;
}

sf::Vector2i SpritesheetAnimation::getFrameSize() const { return mFrameSize; }

// TODO this method needs to be renemaed because it does more then settung the
// number of frames
void SpritesheetAnimation::setNumFrames(std::size_t numFrames)
// void SpritesheetAnimation::setNumFrames(sf::Time aniDuration,
// std::vector<RyuAnimator::AnimationSpec::Frame> aniFrames)
{
    // TODO: here seems to be a bug when we recreate the frames !!! (is it even
    // needable ???) we need to use the data we got from the json config file
    // and not setting the data hard itself here !
    mNumFrames = numFrames;

    mFrames.clear();
    for (int i = 0; i < numFrames; ++i) {
        /*RyuAnimator::AnimationSpec*/ RyuParser::Frame frame;
        frame.duration = 100;
        frame.height = 96;
        frame.width = 80; // frameSize = {80,96},
        frame.x = 0;
        frame.y = 0;
        frame.event = Ryu::EEvent::None;
        mFrames.push_back(frame);
    }
}

/*TODO: we could alternatively implement a copy constructor ?
**
**
*/
void SpritesheetAnimation::setNumFrames(
    sf::Time aniDuration, std::vector<RyuParser::FrameEditor> &aniFrames) {
    std::vector<RyuParser::Frame> frames;

    for (auto &frame : aniFrames) {
        RyuParser::Frame f;
        f.duration = frame.duration;
        f.event = frame.event;
        f.height = frame.height;
        f.width = frame.width;
        f.x = frame.x;
        f.y = frame.y;

        frames.emplace_back(f);
    }

    setNumFrames(aniDuration, frames);
}

void SpritesheetAnimation::setNumFrames(
    sf::Time aniDuration, std::vector<RyuParser::Frame> &aniFrames) {

    mFrames = aniFrames;
    mNumFrames = aniFrames.size();
    mDuration = aniDuration;

    // std::cout << "FramesCount: " << aniFrames.size() << "\n";

    /* old implememattion
    mNumFrames = numFrames;
    if (aniDuration != sf::Time::Zero && aniDuration != mDuration)
    { mDuration = aniDuration; }// sum all frames from input !

     mFrames.clear();
     for(int i=0;i<numFrames;++i)
     {
            float dur;
            if (aniFrames.empty() || aniFrames.size() <= i)
            {
                    dur = 100;
            }
            else
            {
                    dur = (float)aniFrames.at(i).duration;
            }
            AnimationFrame frame{
                    .duration = dur,
                    .frameSize = {80,96},
                    .event = EEvent::None
            };
                    mFrames.push_back(frame);
     }*/
    //}
}

std::size_t SpritesheetAnimation::getNumFrames() const { return mNumFrames; }

int SpritesheetAnimation::getCurrentFrame() const { return (int)mCurrentFrame; }

void SpritesheetAnimation::setDuration(sf::Time duration) {
    mDuration = duration;
}

sf::Time SpritesheetAnimation::getDuration() const { return mDuration; }

void SpritesheetAnimation::setRepeating(bool flag) { mRepeat = flag; }

bool SpritesheetAnimation::isRepeating() const { return mRepeat; }

void SpritesheetAnimation::restart() { mCurrentFrame = 0; }

bool SpritesheetAnimation::isFinished() const {
    //return mCurrentFrame >= mNumFrames;
    return mCurrentFrame == (mNumFrames-1);
}

sf::FloatRect SpritesheetAnimation::getLocalBounds() const {
    return sf::FloatRect(getOrigin(),
                         static_cast<sf::Vector2f>(getFrameSize()));
}

sf::FloatRect SpritesheetAnimation::getGlobalBounds() const {
    return getTransform().transformRect(getLocalBounds());
}

void SpritesheetAnimation::setStartFrame(sf::Vector2i startFrame) {
    mStartFrame = startFrame;
}

void
SpritesheetAnimation::setPivotAbs(sf::Vector2i vec)
{
    mPivotAbs = vec;
}

void
SpritesheetAnimation::setPivotNorm(sf::Vector2f vec)
{
    mPivotNorm = vec;
}

void SpritesheetAnimation::setAnimationPosition(sf::Vector2f posi)
{
    if(lockPositioning)
    {
        return;
    }

    lockPositioning = true;
    setPosition(posi);
    lockPositioning = false;
}

sf::Vector2f
SpritesheetAnimation::getPivotNorm(){return mPivotNorm;}

sf::Vector2i
SpritesheetAnimation::getPivotAbs(){return mPivotAbs;}

void SpritesheetAnimation::update(sf::Time dt) {
    /* Tasks:
     * X1. start ani with specific frame on spritesheet
     * 1b send signal when animation is finished
     * x1c. make Frame a struct
     * 2. Time per frame
     * 3. get movedirection / mirrow spritesheet IF sprite is morrowable
     */
    // TODO: here take the timedirectly from the Frame itself and not the
    // average time
    // sf::Time timePerFrame = mDuration / static_cast<float>(mNumFrames);
    //fmt::print(">>>SpriteAniPos: ({}/{})\n",getPosition().x,getPosition().y);
    sf::Time timePerFrame = sf::milliseconds(mFrames.at(0).duration);
    mElapsedTime += dt;
    sf::Vector2i textureBounds(mSprite.getTexture().getSize());
    sf::IntRect textureRect = mSprite.getTextureRect();
    if (mCurrentFrame == 0) {
        textureRect = sf::IntRect({mStartFrame.x, mStartFrame.y}, {mFrameSize.x,
            mFrameSize.y});
    }

    // std::cout << "texRec:" << textureRect.width << "," << textureRect.height
    // << "\n";
    // TODO: in the loop get the frametime
    while (mElapsedTime >= timePerFrame &&
           (mCurrentFrame < mNumFrames || mRepeat)) {
        // fmt::print("AniName: {}\n", animationIdName);
        if (mCurrentFrame + 1 <= mFrames.size()) {
            timePerFrame = sf::milliseconds(mFrames.at(mCurrentFrame).duration);
        }


        auto& texturePos = textureRect.position;
        auto textureSize = textureRect.size;
        texturePos.x += textureSize.x;
        if (texturePos.x + textureSize.x > textureBounds.x) {
            texturePos.x = 0;
            texturePos.y += textureSize.y;
        }
        mElapsedTime -= timePerFrame;
        // fire event / find it be carefully not to create it !!!!
        try{
            if(mCurrentFrame < mFrames.size())
            {
                const auto &frameEvent = mFrames.at(mCurrentFrame).event;
                // TODO move to debug gui !
                if(mOwner && frameEvent._value != Ryu::EEvent::None) {
                    fmt::print("Fire Event: {} from frame {} \n",
                               frameEvent._to_string(), std::to_string(mCurrentFrame));
                    // make pointer to owner -> use sharedPointer ?
                    mOwner->notifyObservers( frameEvent); //TODO: scenenode ?
                }

            }
            if (mRepeat) {
                mCurrentFrame = (mCurrentFrame + 1) % mNumFrames;
                if (mCurrentFrame == 0) {
                    textureRect = sf::IntRect({mStartFrame.x, mStartFrame.y},
                                              {mFrameSize.x, mFrameSize.y});
                }
            } else {
                mCurrentFrame++;
                fmt::print("CurrentFrame is now: {}\n", mCurrentFrame);
            }
        } catch (std::exception)
        {
            fmt::print("St went wrong.\n");
        }
        // std::cout << "F(" << mCurrentFrame << ")" <<
        // timePerFrame.asMilliseconds() << "\n";
    }
    // std::cout << "TextureRect: " << textureRect.width << "," <<
    // textureRect.height << "\n";
    mSprite.setTextureRect(textureRect);

}

void SpritesheetAnimation::flipAnimationLeft() {
    if (mSprite.getScale().x != -1) {
        // do not ever use mSprite.scale({x,y}) !!!
        mSprite.setScale({-1, 1});
    }
}

void SpritesheetAnimation::flipAnimationRight() {
    if (mSprite.getScale().x != 1) {
        // do not ever use mSprite.scale({x,y}) !!!
        mSprite.setScale({1, 1});
    }
}

void SpritesheetAnimation::draw(sf::RenderTarget &target,
                                sf::RenderStates states) const {
    states.transform *= getTransform();
    target.draw(mSprite, states);
}
