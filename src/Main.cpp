#include <Ryu/Core/Game.h>
#include <fmt/core.h>
#include <iostream>
#include <string>


int main()
{
	try
	{
		Game game;
		game.run();
	}
	catch(const std::exception& e)
	{
		fmt::print("Exception thrown: {}\n", (e.what()));
	}
	
	
	return 0;
}
