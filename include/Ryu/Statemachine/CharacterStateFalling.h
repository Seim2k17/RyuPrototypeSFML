#pragma once

#include "CharacterState.h"
#include <memory>


class CharacterStateFalling : public CharacterState
{
    public:
        CharacterStateFalling();
        ~CharacterStateFalling();
        std::unique_ptr<CharacterState> handleInput(CharacterBase& character,EInput input) override;
        void update(CharacterBase& character) override;

        void enter(CharacterBase& character) override;
        void exit(CharacterBase& character) override;
    
};