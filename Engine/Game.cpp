/****************************************************************************************** 
 *	Chili DirectX Framework Version 16.07.20											  *	
 *	Game.cpp																			  *
 *	Copyright 2016 PlanetChili.net <http://www.planetchili.net>							  *
 *																						  *
 *	This file is part of The Chili DirectX Framework.									  *
 *																						  *
 *	The Chili DirectX Framework is free software: you can redistribute it and/or modify	  *
 *	it under the terms of the GNU General Public License as published by				  *
 *	the Free Software Foundation, either version 3 of the License, or					  *
 *	(at your option) any later version.													  *
 *																						  *
 *	The Chili DirectX Framework is distributed in the hope that it will be useful,		  *
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of						  *
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the						  *
 *	GNU General Public License for more details.										  *
 *																						  *
 *	You should have received a copy of the GNU General Public License					  *
 *	along with The Chili DirectX Framework.  If not, see <http://www.gnu.org/licenses/>.  *
 ******************************************************************************************/
#include "MainWindow.h"
#include "Game.h"

Game::Game(MainWindow& wnd)
	:
	wnd(wnd),
	gfx(wnd),
	rng(rd()),
	ColorDist(0, 2),
	Food_x(1,brd.GetWidth()-1),
	Food_y(1,brd.GetHeight()-1),
	brd(gfx)
{
	snake.InitHead();
	food.Jump({ Food_x(rng),Food_y(rng) });
}

void Game::Go()
{
	gfx.BeginFrame();	
	UpdateModel();
	ComposeFrame();
	gfx.EndFrame();
}

void Game::UpdateModel()
{
	if (!GameOver)
	{
		++counter;

		snake.CheckForInput(wnd.kbd);


		if (snake.CheckFood(food))
		{
			food.Jump({ Food_x(rng),Food_y(rng) });
		}
		if (counter >= Timer)
		{
			GameOver = CheckForGameOver(snake, brd);
			if (!GameOver)
			{
				snake.Move();
				counter = 0;
			}
		}
	}
}

bool Game::CheckForGameOver(Snake& snake, Board& brd)
{
	if (snake.EatsItself() || brd.isOutsideBoard(snake.GetNextHeadLocation()) )
	{
		return true;
	}
	else
		return false;
}


void Game::ComposeFrame()
{
	
	brd.DrawBorder(Colors::Blue);
	
	snake.DrawToBoard(brd);
	food.DrawToBoard(brd);
	
	if (GameOver)
	{
		gfx.DrawGameOver(Graphics::ScreenWidth / 2 - 42, Graphics::ScreenHeight / 2 - 32);
	}
}
