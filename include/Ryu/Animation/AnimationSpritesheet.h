#pragma once

#include <SFML/Graphics.hpp>

class AnimationSpritesheet : public sf::Drawable, public sf::Transformable
{
    public:
        AnimationSpritesheet();
		explicit AnimationSpritesheet(const sf::Texture& texture);

		void setTexture(const sf::Texture& texture);
		const sf::Texture* getTexture() const;

		void setFrameSize(sf::Vector2i mFrameSize);
		sf::Vector2i getFrameSize() const;

		void setNumFrames(std::size_t numFrames);
		std::size_t getNumFrames() const;

		void setStartFrame(std::size_t startFrame);

		void setDuration(sf::Time duration);
		sf::Time getDuration() const;

		void setRepeating(bool flag);
		bool isRepeating() const;

		void restart();
		bool isFinished() const;

		sf::FloatRect getLocalBounds() const;
		sf::FloatRect getGlobalBounds() const;
        void update(sf::Time dt);


	private:
		void draw(sf::RenderTarget& target, sf::RenderStates states) const;

    private:
        sf::Sprite mSprite;
        sf::Vector2i mFrameSize;
		std::size_t mStartFrame;
        std::size_t mNumFrames;
        std::size_t mCurrentFrame;
        sf::Time mDuration;
        sf::Time mElapsedTime;
        bool mRepeat;
};